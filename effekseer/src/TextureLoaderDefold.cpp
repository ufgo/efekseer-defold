#include "TextureLoaderDefold.h"
#include <string>
#include <vector>
#include <dmsdk/dlib/sys.h>

// Convert UTF-16 to UTF-8 simply (assuming mostly ASCII/Latin paths for now)
static std::string UTF16ToUTF8(const char16_t* path) {
    std::string result;
    while (*path) {
        result += (char)(*path & 0xFF);
        path++;
    }
    // Replace backslashes with forward slashes
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '\\') {
            result[i] = '/';
        }
    }
    return result;
}

extern lua_State* g_L;
std::string g_CurrentBasePath = "";

static std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = path.find('/');
    while (end != std::string::npos) {
        if (end > start) parts.push_back(path.substr(start, end - start));
        start = end + 1;
        end = path.find('/', start);
    }
    if (start < path.length()) parts.push_back(path.substr(start));
    return parts;
}

static bool LoadResourceFromDefold(const std::string& resourcePath, const std::string& utf8Path, std::vector<char>& outData) {
    if (!g_L) return false;
    bool loaded = false;
    int top = lua_gettop(g_L);
    lua_getglobal(g_L, "sys");
    if (lua_istable(g_L, -1)) {
        lua_getfield(g_L, -1, "load_resource");
        if (lua_isfunction(g_L, -1)) {
            lua_pushstring(g_L, resourcePath.c_str());
            if (lua_pcall(g_L, 1, 1, 0) == 0) {
                if (lua_isstring(g_L, -1)) {
                    size_t resourceSize = 0;
                    const char* resourceData = lua_tolstring(g_L, -1, &resourceSize);
                    if (resourceData && resourceSize > 0) {
                        outData.assign(resourceData, resourceData + resourceSize);
                        loaded = true;
                    }
                }
            }
            
            if (!loaded && !g_CurrentBasePath.empty()) {
                // Fallback: try all suffixes combined with g_CurrentBasePath
                std::vector<std::string> parts = SplitPath(utf8Path);
                std::string suffix = "";
                for (int i = (int)parts.size() - 1; i >= 0; --i) {
                    if (parts[i] == ".." || parts[i] == ".") break;
                    
                    if (suffix.empty()) suffix = parts[i];
                    else suffix = parts[i] + "/" + suffix;
                    
                    std::string fallbackPath = g_CurrentBasePath + suffix;
                    size_t pos;
                    while ((pos = fallbackPath.find("//")) != std::string::npos) {
                        fallbackPath.replace(pos, 2, "/");
                    }
                    if (fallbackPath.length() > 0 && fallbackPath[0] != '/') {
                        fallbackPath = "/" + fallbackPath;
                    }
                    
                    lua_settop(g_L, top + 1);
                    lua_getfield(g_L, -1, "load_resource");
                    lua_pushstring(g_L, fallbackPath.c_str());
                    if (lua_pcall(g_L, 1, 1, 0) == 0) {
                        if (lua_isstring(g_L, -1)) {
                            size_t resourceSize = 0;
                            const char* resourceData = lua_tolstring(g_L, -1, &resourceSize);
                            if (resourceData && resourceSize > 0) {
                                outData.assign(resourceData, resourceData + resourceSize);
                                loaded = true;
                                dmLogInfo("Loaded fallback resource: %s", fallbackPath.c_str());
                                break;
                            }
                        }
                    }
                }
            }

            if (!loaded) {
                // Original fallback: extract filename and try in /assets/
                size_t lastSlash = utf8Path.find_last_of('/');
                std::string filename = (lastSlash != std::string::npos) ? utf8Path.substr(lastSlash + 1) : utf8Path;
                std::string fallbackPath = "/assets/" + filename;
                
                lua_settop(g_L, top + 1);
                lua_getfield(g_L, -1, "load_resource");
                lua_pushstring(g_L, fallbackPath.c_str());
                if (lua_pcall(g_L, 1, 1, 0) == 0) {
                    if (lua_isstring(g_L, -1)) {
                        size_t resourceSize = 0;
                        const char* resourceData = lua_tolstring(g_L, -1, &resourceSize);
                        if (resourceData && resourceSize > 0) {
                            outData.assign(resourceData, resourceData + resourceSize);
                            loaded = true;
                            dmLogInfo("Loaded fallback resource: %s", fallbackPath.c_str());
                        }
                    }
                }
            }
        }
    }
    lua_settop(g_L, top);
    return loaded;
}

TextureLoaderDefold::TextureLoaderDefold(Effekseer::TextureLoaderRef baseLoader)
    : m_baseLoader(baseLoader)
{
}

TextureLoaderDefold::~TextureLoaderDefold()
{
}

Effekseer::TextureRef TextureLoaderDefold::Load(const char16_t* path, Effekseer::TextureType textureType)
{
    if (!path) return nullptr;

    std::string utf8Path = UTF16ToUTF8(path);
    std::string resourcePath = utf8Path;
    if (resourcePath.length() > 0 && resourcePath[0] != '/') {
        resourcePath = "/" + resourcePath;
    }

    dmLogInfo("TextureLoaderDefold::Load requested: %s", utf8Path.c_str());

    std::vector<char> data;
    if (LoadResourceFromDefold(resourcePath, utf8Path, data)) {
        bool isMipMapEnabled = Effekseer::TextureLoaderHelper::GetIsMipmapEnabled(path);
        return m_baseLoader->Load(data.data(), (int32_t)data.size(), textureType, isMipMapEnabled);
    }
    return nullptr;
}

void TextureLoaderDefold::Unload(Effekseer::TextureRef data)
{
    if (m_baseLoader && data) {
        m_baseLoader->Unload(data);
    }
}

// -------------------------------------------------------------------------
// ModelLoaderDefold
// -------------------------------------------------------------------------

ModelLoaderDefold::ModelLoaderDefold(Effekseer::ModelLoaderRef baseLoader)
    : m_baseLoader(baseLoader)
{
}

ModelLoaderDefold::~ModelLoaderDefold()
{
}

Effekseer::ModelRef ModelLoaderDefold::Load(const char16_t* path)
{
    if (!path) return nullptr;

    std::string utf8Path = UTF16ToUTF8(path);
    std::string resourcePath = utf8Path;
    if (resourcePath.length() > 0 && resourcePath[0] != '/') {
        resourcePath = "/" + resourcePath;
    }

    dmLogInfo("ModelLoaderDefold::Load requested: %s", utf8Path.c_str());

    std::vector<char> data;
    if (LoadResourceFromDefold(resourcePath, utf8Path, data)) {
        return m_baseLoader->Load(data.data(), (int32_t)data.size());
    }
    return nullptr;
}

void ModelLoaderDefold::Unload(Effekseer::ModelRef data)
{
    if (m_baseLoader && data) {
        m_baseLoader->Unload(data);
    }
}

// -------------------------------------------------------------------------
// MaterialLoaderDefold
// -------------------------------------------------------------------------

MaterialLoaderDefold::MaterialLoaderDefold(Effekseer::MaterialLoaderRef baseLoader)
    : m_baseLoader(baseLoader)
{
}

MaterialLoaderDefold::~MaterialLoaderDefold()
{
}

Effekseer::MaterialRef MaterialLoaderDefold::Load(const char16_t* path)
{
    if (!path) return nullptr;

    std::string utf8Path = UTF16ToUTF8(path);
    std::string resourcePath = utf8Path;
    if (resourcePath.length() > 0 && resourcePath[0] != '/') {
        resourcePath = "/" + resourcePath;
    }

    dmLogInfo("MaterialLoaderDefold::Load requested: %s", utf8Path.c_str());

    std::vector<char> data;
    if (LoadResourceFromDefold(resourcePath, utf8Path, data)) {
        Effekseer::MaterialFileType fileType = Effekseer::MaterialFileType::Code;
        if (utf8Path.length() > 0 && utf8Path.back() == 'd') {
            fileType = Effekseer::MaterialFileType::Compiled;
        }
        return m_baseLoader->Load(data.data(), (int32_t)data.size(), fileType);
    }
    return nullptr;
}

void MaterialLoaderDefold::Unload(Effekseer::MaterialRef data)
{
    if (m_baseLoader && data) {
        m_baseLoader->Unload(data);
    }
}

// -------------------------------------------------------------------------
// CurveLoaderDefold
// -------------------------------------------------------------------------

CurveLoaderDefold::CurveLoaderDefold(Effekseer::CurveLoaderRef baseLoader)
    : m_baseLoader(baseLoader)
{
}

CurveLoaderDefold::~CurveLoaderDefold()
{
}

Effekseer::CurveRef CurveLoaderDefold::Load(const char16_t* path)
{
    if (!path) return nullptr;

    std::string utf8Path = UTF16ToUTF8(path);
    std::string resourcePath = utf8Path;
    if (resourcePath.length() > 0 && resourcePath[0] != '/') {
        resourcePath = "/" + resourcePath;
    }

    dmLogInfo("CurveLoaderDefold::Load requested: %s", utf8Path.c_str());

    std::vector<char> data;
    if (LoadResourceFromDefold(resourcePath, utf8Path, data)) {
        return m_baseLoader->Load(data.data(), (int32_t)data.size());
    }
    return nullptr;
}

void CurveLoaderDefold::Unload(Effekseer::CurveRef data)
{
    if (m_baseLoader && data) {
        m_baseLoader->Unload(data);
    }
}
