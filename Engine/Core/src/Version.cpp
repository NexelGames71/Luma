#include "Luma/Core/Version.h"

// These are supplied by the build system (see Engine/Core/CMakeLists.txt).
#ifndef LUMA_VERSION_MAJOR
#define LUMA_VERSION_MAJOR 0
#endif
#ifndef LUMA_VERSION_MINOR
#define LUMA_VERSION_MINOR 1
#endif
#ifndef LUMA_VERSION_PATCH
#define LUMA_VERSION_PATCH 0
#endif

namespace Luma {

Version EngineVersion() {
    return Version{LUMA_VERSION_MAJOR, LUMA_VERSION_MINOR, LUMA_VERSION_PATCH};
}

const char* EngineVersionString() {
#define LUMA_STR2(x) #x
#define LUMA_STR(x) LUMA_STR2(x)
    return "Luma Engine " LUMA_STR(LUMA_VERSION_MAJOR) "." LUMA_STR(
        LUMA_VERSION_MINOR) "." LUMA_STR(LUMA_VERSION_PATCH);
#undef LUMA_STR
#undef LUMA_STR2
}

}  // namespace Luma
