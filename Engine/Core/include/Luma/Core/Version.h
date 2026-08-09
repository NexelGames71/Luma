#pragma once

namespace Luma {

// Semantic version of the engine, wired from the CMake project version.
struct Version {
    int major;
    int minor;
    int patch;
};

Version EngineVersion();

// Human-readable version string, e.g. "Luma Engine 0.1.0".
const char* EngineVersionString();

}  // namespace Luma
