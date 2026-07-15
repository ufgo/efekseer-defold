#include <dmsdk/sdk.h>
#include <dmsdk/dlib/vmath.h>

void test(lua_State* L) {
    dmVMath::Matrix4* mat = dmScript::CheckMatrix4(L, 1);
}
