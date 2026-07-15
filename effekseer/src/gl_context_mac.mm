#include <dmsdk/sdk.h>

#if defined(__APPLE__)
#include <AppKit/AppKit.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>

extern "C" bool Effekseer_MakeContextCurrent() {
    CGLContextObj ctx = CGLGetCurrentContext();
    if (ctx == nullptr) {
        dmLogError("Effekseer: No OpenGL context is current on this thread.");
        return false;
    }
    return true;
}

extern "C" void Effekseer_ClearContextCurrent() {
    // We do NOT unset Defold's context — it belongs to Defold.
}

#else

extern "C" bool Effekseer_MakeContextCurrent() { return true; }
extern "C" void Effekseer_ClearContextCurrent() {}

#endif
