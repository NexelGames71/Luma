# Luma automated tests (Catch2 v3, run via ctest).

luma_require_catch2()

add_executable(LumaTests
    Core/AssertTests.cpp
    Core/LogTests.cpp
    Core/ConfigTests.cpp
    Core/EventTests.cpp
    Core/LayerStackTests.cpp
    Core/ApplicationTests.cpp
    Core/MemoryTests.cpp
    Project/ProjectTests.cpp
    Serialization/SerialValueTests.cpp
    Serialization/JsonTests.cpp
    Serialization/ReflectionTests.cpp
    Scene/SceneSerializerTests.cpp
    Scene/CameraComponentTests.cpp
    Math/MathTests.cpp
    Slate/ThemeTests.cpp
    VFS/PathTests.cpp
    VFS/VFSTests.cpp
    VFS/FileWatcherTests.cpp)

target_link_libraries(LumaTests PRIVATE Luma::Core Luma::Project Luma::Serialization Luma::Scene Luma::Slate Luma::VFS Catch2::Catch2WithMain)

set_target_properties(LumaTests PROPERTIES FOLDER "Tests")

# One ctest entry that runs the whole Catch2 binary (reports per-assertion on
# failure). Fine-grained catch_discover_tests can be adopted later.
add_test(NAME LumaTests COMMAND LumaTests)
