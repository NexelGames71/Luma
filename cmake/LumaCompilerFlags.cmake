# LumaCompilerFlags.cmake
# Defines the `Luma::Flags` interface target carrying warning/standard settings,
# per-config LUMA_CONFIG_* defines, and fills in compiler/linker flags for the
# custom Development and Shipping configurations (CMake only pre-populates the
# standard Debug/Release/etc. configs).

add_library(luma_flags INTERFACE)
add_library(Luma::Flags ALIAS luma_flags)

target_compile_features(luma_flags INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(luma_flags INTERFACE
        /W4 /permissive- /EHsc /utf-8 /Zc:preprocessor
        # Silence warnings from third-party (SYSTEM) headers so /WX only polices
        # our own code.
        /external:W0
        $<$<CONFIG:Development>:/WX>)
    target_compile_definitions(luma_flags INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
endif()

target_compile_definitions(luma_flags INTERFACE
    $<$<CONFIG:Debug>:LUMA_CONFIG_DEBUG=1>
    $<$<CONFIG:Development>:LUMA_CONFIG_DEVELOPMENT=1>
    $<$<CONFIG:Release>:LUMA_CONFIG_RELEASE=1>
    $<$<CONFIG:Shipping>:LUMA_CONFIG_SHIPPING=1>)

# --- Custom configuration flags (MSVC) -------------------------------------
# Development: optimized with debug info, assertions ENABLED (no NDEBUG).
# Shipping:    fully optimized, assertions/logging compiled out (NDEBUG).
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEVELOPMENT "/Zi /O2 /Ob1"
        CACHE STRING "Development C++ flags" FORCE)
    set(CMAKE_C_FLAGS_DEVELOPMENT   "/Zi /O2 /Ob1"
        CACHE STRING "Development C flags" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT    "/DEBUG:FULL /INCREMENTAL:NO"
        CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "/DEBUG:FULL /INCREMENTAL:NO"
        CACHE STRING "" FORCE)
    set(CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT ""
        CACHE STRING "" FORCE)

    set(CMAKE_CXX_FLAGS_SHIPPING "/O2 /Ob2 /DNDEBUG"
        CACHE STRING "Shipping C++ flags" FORCE)
    set(CMAKE_C_FLAGS_SHIPPING   "/O2 /Ob2 /DNDEBUG"
        CACHE STRING "Shipping C flags" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_SHIPPING    "/INCREMENTAL:NO /OPT:REF /OPT:ICF"
        CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_SHIPPING "/INCREMENTAL:NO /OPT:REF /OPT:ICF"
        CACHE STRING "" FORCE)
    set(CMAKE_STATIC_LINKER_FLAGS_SHIPPING ""
        CACHE STRING "" FORCE)
endif()

mark_as_advanced(
    CMAKE_CXX_FLAGS_DEVELOPMENT CMAKE_C_FLAGS_DEVELOPMENT
    CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT
    CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT
    CMAKE_CXX_FLAGS_SHIPPING CMAKE_C_FLAGS_SHIPPING
    CMAKE_EXE_LINKER_FLAGS_SHIPPING CMAKE_SHARED_LINKER_FLAGS_SHIPPING
    CMAKE_STATIC_LINKER_FLAGS_SHIPPING)
