#define EXTENSION_NAME effekseer
#define LIB_NAME "effekseer"
#define MODULE_NAME "effekseer"

#include <dmsdk/sdk.h>
#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <string>
#include "TextureLoaderDefold.h"
#include <map>
#include <vector>

struct ActiveEffectInfo {
    Effekseer::Handle current_handle;
    Effekseer::EffectRef effect;
    int callback_ref = LUA_NOREF;
    bool loop = false;
    float x = 0, y = 0, z = 0;
    float sx = 1, sy = 1, sz = 1;
};

static Effekseer::ManagerRef g_Manager = nullptr;
static EffekseerRendererGL::RendererRef g_Renderer = nullptr;
static Effekseer::Matrix44 g_cameraMatrix;
static Effekseer::Matrix44 g_projectionMatrix;
lua_State* g_L = nullptr; // Global lua_State for TextureLoader
extern std::string g_CurrentBasePath;

static int g_NextHandleId = 1;
static std::map<int, ActiveEffectInfo> g_ActiveEffects;

static int g_NextEffectId = 1;
static std::map<int, Effekseer::EffectRef> g_LoadedEffects;

#if defined(__APPLE__)
extern "C" bool Effekseer_MakeContextCurrent();
extern "C" void Effekseer_ClearContextCurrent();
#else
extern "C" bool Effekseer_MakeContextCurrent() { return true; }
extern "C" void Effekseer_ClearContextCurrent() {}
#endif

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
extern "C" void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) { return nullptr; }
extern "C" GLboolean glUnmapBuffer(GLenum target) { return GL_TRUE; }
extern "C" void glGetQueryObjectui64v(GLuint id, GLenum pname, uint64_t* params) {}
#endif

static int Effekseer_Init(lua_State* L)
{
    if (g_Manager == nullptr)
    {
        if (!Effekseer_MakeContextCurrent())
        {
            dmLogError("Effekseer requires OpenGL, but no context was found! Are you running on Vulkan/Metal?");
            return 0;
        }
#if defined(__EFFEKSEER_RENDERER_GLES3__)
        EffekseerRendererGL::OpenGLDeviceType deviceType = EffekseerRendererGL::OpenGLDeviceType::OpenGLES3;
#elif defined(__EFFEKSEER_RENDERER_GLES2__)
        EffekseerRendererGL::OpenGLDeviceType deviceType = EffekseerRendererGL::OpenGLDeviceType::OpenGLES2;
#else
        EffekseerRendererGL::OpenGLDeviceType deviceType = EffekseerRendererGL::OpenGLDeviceType::OpenGL3;
#endif
        g_Renderer = EffekseerRendererGL::Renderer::Create(8000, deviceType);
        g_Manager = Effekseer::Manager::Create(8000);

        g_Manager->SetSpriteRenderer(g_Renderer->CreateSpriteRenderer());
        g_Manager->SetRibbonRenderer(g_Renderer->CreateRibbonRenderer());
        g_Manager->SetRingRenderer(g_Renderer->CreateRingRenderer());
        g_Manager->SetTrackRenderer(g_Renderer->CreateTrackRenderer());
        g_Manager->SetModelRenderer(g_Renderer->CreateModelRenderer());

        // Use our custom Defold texture loader, wrapping the default GL texture loader
        Effekseer::TextureLoaderRef defaultTextureLoader = g_Renderer->CreateTextureLoader();
        g_Manager->SetTextureLoader(Effekseer::MakeRefPtr<TextureLoaderDefold>(defaultTextureLoader));
        
        Effekseer::ModelLoaderRef defaultModelLoader = g_Renderer->CreateModelLoader();
        g_Manager->SetModelLoader(Effekseer::MakeRefPtr<ModelLoaderDefold>(defaultModelLoader));

        Effekseer::MaterialLoaderRef defaultMaterialLoader = g_Renderer->CreateMaterialLoader();
        g_Manager->SetMaterialLoader(Effekseer::MakeRefPtr<MaterialLoaderDefold>(defaultMaterialLoader));

        Effekseer::CurveLoaderRef defaultCurveLoader = Effekseer::MakeRefPtr<Effekseer::CurveLoader>();
        g_Manager->SetCurveLoader(Effekseer::MakeRefPtr<CurveLoaderDefold>(defaultCurveLoader));

        Effekseer_ClearContextCurrent();
    }
    return 0;
}

static int Effekseer_Update(lua_State* L)
{
    float dt = (float)luaL_checknumber(L, 1);
    if (g_Manager)
    {


        static int frame_counter = 0;
        frame_counter++;
        if (frame_counter % 60 == 0 && !g_ActiveEffects.empty()) {
            printf("[Effekseer] Active effects tracking: %zu, Exists(handle)=%d\\n", g_ActiveEffects.size(), g_Manager->Exists(g_ActiveEffects.begin()->second.current_handle));
        }

        g_Manager->Update(dt * 60.0f); // Effekseer updates are typically 60fps based

        std::vector<int> to_remove;
        for (auto& kv : g_ActiveEffects) {
            if (!g_Manager->Exists(kv.second.current_handle)) {
                if (kv.second.loop) {
                    kv.second.current_handle = g_Manager->Play(kv.second.effect, kv.second.x, kv.second.y, kv.second.z);
                    g_Manager->SetScale(kv.second.current_handle, kv.second.sx, kv.second.sy, kv.second.sz);
                    
                    if (kv.second.callback_ref != LUA_NOREF) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, kv.second.callback_ref);
                        lua_pushinteger(L, kv.first);
                        if (lua_pcall(L, 1, 0, 0) != 0) {
                            dmLogError("Error calling Effekseer loop callback: %s", lua_tostring(L, -1));
                            lua_pop(L, 1);
                        }
                    }
                } else {
                    if (kv.second.callback_ref != LUA_NOREF) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, kv.second.callback_ref);
                        if (lua_isfunction(L, -1)) {
                            lua_pushinteger(L, kv.first);
                            if (lua_pcall(L, 1, 0, 0) != 0) {
                                dmLogError("Effekseer Callback error: %s", lua_tostring(L, -1));
                                lua_pop(L, 1);
                            }
                        } else {
                            lua_pop(L, 1);
                        }
                        luaL_unref(L, LUA_REGISTRYINDEX, kv.second.callback_ref);
                        kv.second.callback_ref = LUA_NOREF;
                    }
                    to_remove.push_back(kv.first);
                }
            }
        }
        for (int id : to_remove) {
            g_ActiveEffects.erase(id);
        }
    }
    return 0;
}

static int Effekseer_Draw(lua_State* L)
{
    if (g_Manager != nullptr)
    {
        if (!Effekseer_MakeContextCurrent()) return 0;
        g_Renderer->SetProjectionMatrix(g_projectionMatrix);
        g_Renderer->SetCameraMatrix(g_cameraMatrix);
        g_Renderer->BeginRendering();
        g_Manager->Draw();
        g_Renderer->EndRendering();
        
        // Debug OpenGL errors and context
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR)
        {
            printf("[Effekseer] GL ERROR after Draw: 0x%04X\n", err);
        }

        static bool printed_gl = false;
        if (!printed_gl) {
            printed_gl = true;
            const GLubyte* renderer = glGetString(GL_RENDERER);
            const GLubyte* version = glGetString(GL_VERSION);
            printf("[Effekseer] GL Renderer: %s\n", renderer ? (const char*)renderer : "NULL");
            printf("[Effekseer] GL Version: %s\n", version ? (const char*)version : "NULL");
        }

        Effekseer_ClearContextCurrent();
    }
    return 0;
}

static int Effekseer_Clear(lua_State* L)
{
    if (!Effekseer_MakeContextCurrent()) return 0;
    // Clear the screen with black color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Effekseer_ClearContextCurrent();
    return 0;
}

static int Effekseer_GetPlayingCount(lua_State* L)
{
    int count = 0;
    if (g_Manager)
    {
        count = g_Manager->GetTotalInstanceCount();
    }
    lua_pushinteger(L, count);
    return 1;
}

static int Effekseer_PlayEffect(lua_State* L)
{
    size_t size = 0;
    const char* data = luaL_checklstring(L, 1, &size);
    float scale = (float)luaL_optnumber(L, 2, 1.0);
    const char* basePath = luaL_optstring(L, 3, nullptr);

    bool loop = false;
    int callback_ref = LUA_NOREF;
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "loop");
        if (lua_isboolean(L, -1)) {
            loop = lua_toboolean(L, -1); printf("[Effekseer] loop set to %d\n", loop);
        }
        lua_pop(L, 1);

        lua_getfield(L, 4, "callback");
        if (lua_isfunction(L, -1)) {
            callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_pop(L, 1);
        }
    }

    if (g_Manager)
    {
        Effekseer::EffectRef effect;
        if (basePath) {
            std::string bpStr(basePath);
            g_CurrentBasePath = bpStr;
            // Convert basePath to UTF16
            std::u16string u16BasePath(bpStr.begin(), bpStr.end());
            effect = Effekseer::Effect::Create(g_Manager, (void*)data, (int32_t)size, scale, (const char16_t*)u16BasePath.c_str());
            g_CurrentBasePath = "";
        } else {
            effect = Effekseer::Effect::Create(g_Manager, (void*)data, (int32_t)size, scale);
        }

        if (effect == nullptr) {
            dmLogError("[Effekseer] ERROR: Effect::Create failed! Probably missing textures.");
            if (callback_ref != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
            }
            lua_pushinteger(L, -1);
            return 1;
        }
        Effekseer::Handle handle = g_Manager->Play(effect, 0, 0, 0);
        printf("[Effekseer] Played effect, loop=%d, callback_ref=%d\\n", loop, callback_ref);
        
        int my_id = g_NextHandleId++;
        ActiveEffectInfo info;
        info.current_handle = handle;
        info.effect = effect;
        info.loop = loop;
        info.callback_ref = callback_ref;
        g_ActiveEffects[my_id] = info;

        lua_pushinteger(L, my_id);
        return 1;
    }
    
    if (callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
    }
    lua_pushinteger(L, -1);
    return 1;
}

static int Effekseer_LoadEffect(lua_State* L)
{
    size_t size = 0;
    const char* data = luaL_checklstring(L, 1, &size);
    float scale = (float)luaL_optnumber(L, 2, 1.0);
    const char* basePath = luaL_optstring(L, 3, nullptr);

    if (g_Manager)
    {
        Effekseer::EffectRef effect;
        if (basePath) {
            std::string bpStr(basePath);
            g_CurrentBasePath = bpStr;
            std::u16string u16BasePath(bpStr.begin(), bpStr.end());
            effect = Effekseer::Effect::Create(g_Manager, (void*)data, (int32_t)size, scale, (const char16_t*)u16BasePath.c_str());
            g_CurrentBasePath = "";
        } else {
            effect = Effekseer::Effect::Create(g_Manager, (void*)data, (int32_t)size, scale);
        }

        if (effect == nullptr) {
            dmLogError("[Effekseer] ERROR: Effect::Create failed!");
            lua_pushinteger(L, -1);
            return 1;
        }
        
        int my_id = g_NextEffectId++;
        g_LoadedEffects[my_id] = effect;
        lua_pushinteger(L, my_id);
        return 1;
    }
    lua_pushinteger(L, -1);
    return 1;
}

static int Effekseer_Play(lua_State* L)
{
    int effect_id = luaL_checkinteger(L, 1);
    
    bool loop = false;
        int callback_ref = LUA_NOREF;
    float speed = 1.0f;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "loop");
        if (lua_isboolean(L, -1)) loop = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 2, "speed");
        if (lua_isnumber(L, -1)) speed = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 2, "callback");
        if (lua_isfunction(L, -1)) callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        else lua_pop(L, 1);
    }

    if (g_Manager)
    {
        auto it = g_LoadedEffects.find(effect_id);
        if (it != g_LoadedEffects.end()) {
            Effekseer::Handle handle = g_Manager->Play(it->second, 0, 0, 0);
            
            if (speed != 1.0f) {
                g_Manager->SetSpeed(handle, speed);
            }
            
            int my_id = g_NextHandleId++;
            ActiveEffectInfo info;
            info.current_handle = handle;
            info.effect = it->second;
            info.loop = loop;
            info.callback_ref = callback_ref;
            g_ActiveEffects[my_id] = info;

            lua_pushinteger(L, my_id);
            return 1;
        } else {
            dmLogError("[Effekseer] ERROR: Effect ID %d not found!", effect_id);
        }
    }
    
    if (callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
    }
    lua_pushinteger(L, -1);
    return 1;
}

static int Effekseer_SetLocation(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);
    
    if (g_Manager)
    {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            it->second.x = x;
            it->second.y = y;
            it->second.z = z;
            g_Manager->SetLocation(it->second.current_handle, x, y, z);
        }
    }
    return 0;
}

static int Effekseer_SetRotationQuat(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);
    float w = (float)luaL_checknumber(L, 5);
    
    if (g_Manager)
    {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            float angle = 2.0f * acosf(w);
            float s = sqrtf(1.0f - w * w);
            float ax = (s < 0.001f) ? 1.0f : x / s;
            float ay = (s < 0.001f) ? 0.0f : y / s;
            float az = (s < 0.001f) ? 0.0f : z / s;
            g_Manager->SetRotation(it->second.current_handle, Effekseer::Vector3D(ax, ay, az), angle);
        }
    }
    return 0;
}

static int Effekseer_SetScale(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    float sx = (float)luaL_checknumber(L, 2);
    float sy = (float)luaL_checknumber(L, 3);
    float sz = (float)luaL_checknumber(L, 4);
    
    if (g_Manager)
    {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            it->second.sx = sx;
            it->second.sy = sy;
            it->second.sz = sz;
            g_Manager->SetScale(it->second.current_handle, sx, sy, sz);
        }
    }
    return 0;
}

static int Effekseer_SetSpeed(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    float speed = (float)luaL_checknumber(L, 2);
    
    if (g_Manager)
    {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            g_Manager->SetSpeed(it->second.current_handle, speed);
        }
    }
    return 0;
}

static int Effekseer_SetPaused(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    bool paused = lua_toboolean(L, 2);
    
    if (g_Manager)
    {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            g_Manager->SetPaused(it->second.current_handle, paused);
        }
    }
    return 0;
}

static int Effekseer_StopEffect(lua_State* L)
{
    int id = luaL_checkinteger(L, 1);
    if (g_Manager) {
        auto it = g_ActiveEffects.find(id);
        if (it != g_ActiveEffects.end()) {
            g_Manager->StopEffect(it->second.current_handle);
            if (it->second.callback_ref != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, it->second.callback_ref);
            }
            g_ActiveEffects.erase(it);
        }
    }
    return 0;
}

static int Effekseer_StopAllEffects(lua_State* L)
{
    if (g_Manager) {
        g_Manager->StopAllEffects();
        for (auto& kv : g_ActiveEffects) {
            if (kv.second.callback_ref != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, kv.second.callback_ref);
            }
        }
        g_ActiveEffects.clear();
    }
    return 0;
}

static int Effekseer_SetCameraMatrix(lua_State* L)
{
    dmVMath::Matrix4* mat = dmScript::CheckMatrix4(L, 1);
    if (g_Renderer && mat)
    {
        memcpy(g_cameraMatrix.Values, mat, sizeof(float) * 16);
    }
    return 0;
}

static int Effekseer_SetProjectionMatrix(lua_State* L)
{
    dmVMath::Matrix4* mat = dmScript::CheckMatrix4(L, 1);
    if (g_Renderer && mat)
    {
        memcpy(g_projectionMatrix.Values, mat, sizeof(float) * 16);
    }
    return 0;
}

static const luaL_reg Module_methods[] =
{
    {"init", Effekseer_Init},
    {"update", Effekseer_Update},
    {"draw", Effekseer_Draw},
    {"clear", Effekseer_Clear},
    {"play_effect", Effekseer_PlayEffect},
    {"load_effect", Effekseer_LoadEffect},
    {"play", Effekseer_Play},
    {"set_location", Effekseer_SetLocation},
    {"set_rotation_quat", Effekseer_SetRotationQuat},
    {"set_scale", Effekseer_SetScale},
    {"set_speed", Effekseer_SetSpeed},
    {"set_paused", Effekseer_SetPaused},
    {"stop_effect", Effekseer_StopEffect},
    {"stop_all_effects", Effekseer_StopAllEffects},
    {"set_camera_matrix", Effekseer_SetCameraMatrix},
    {"set_projection_matrix", Effekseer_SetProjectionMatrix},
    {"get_playing_count", Effekseer_GetPlayingCount},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
}

static dmExtension::Result Effekseer_PostRender(dmExtension::Params* params)
{
    if (g_Manager && g_Renderer)
    {
        // Automatically draw Effekseer at the end of the frame
        if (!Effekseer_MakeContextCurrent()) return dmExtension::RESULT_OK;
        
        g_Renderer->SetProjectionMatrix(g_projectionMatrix);
        g_Renderer->SetCameraMatrix(g_cameraMatrix);
        g_Renderer->BeginRendering();
        g_Manager->Draw();
        g_Renderer->EndRendering();

        // Restore context if needed
        Effekseer_ClearContextCurrent();
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeEffekseer(dmExtension::AppParams* params)
{
    // Register Post-Render callback
    dmExtension::RegisterCallback(dmExtension::CALLBACK_POST_RENDER, (FExtensionCallback)Effekseer_PostRender);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeEffekseer(dmExtension::Params* params)
{
    g_L = params->m_L;
    lua_State* L = params->m_L;
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeEffekseer(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeEffekseer(dmExtension::Params* params)
{
    if (g_Manager)
    {
        g_Manager->StopAllEffects();
    }
    for (auto& kv : g_ActiveEffects) {
        if (kv.second.callback_ref != LUA_NOREF) {
            luaL_unref(params->m_L, LUA_REGISTRYINDEX, kv.second.callback_ref);
        }
    }
    g_ActiveEffects.clear();
    g_LoadedEffects.clear();

    if (g_Manager)
    {
        g_Manager.Reset();
    }
    if (g_Renderer)
    {
        g_Renderer.Reset();
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeEffekseer, AppFinalizeEffekseer, InitializeEffekseer, 0, 0, FinalizeEffekseer)
// FORCE REBUILD 7
