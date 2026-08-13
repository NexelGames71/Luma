cmake_minimum_required(VERSION 3.28)

project(LumaEngine
    VERSION 0.1.0
    DESCRIPTION "Luma Engine - a production-grade Vulkan game engine"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Build configurations
# Luma uses four configs: Debug, Development, Release, Shipping.
# ---------------------------------------------------------------------------
get_property(_luma_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_luma_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Release;Shipping"
        CACHE STRING "Luma build configurations" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Development" CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Development Release Shipping)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Map our custom configs onto standard ones for imported/third-party targets.
set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT Development Release "")
set(CMAKE_MAP_IMPORTED_CONFIG_SHIPPING    Shipping Release "")

# Put all binaries in one predictable place per config.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(LUMA_BUILD_TESTS "Build Luma automated tests" ON)

include(LumaCompilerFlags)
include(LumaDependencies)

# ---------------------------------------------------------------------------
# Vulkan SDK detection (validated now; the renderer starts using it in M2)
# ---------------------------------------------------------------------------
if(NOT DEFINED Vulkan_ROOT AND EXISTS "D:/VulkanSDK/1.4.341.1")
    set(Vulkan_ROOT "D:/VulkanSDK/1.4.341.1" CACHE PATH "Vulkan SDK root")
endif()
find_package(Vulkan REQUIRED)
message(STATUS "Luma: Vulkan found -> version='${Vulkan_VERSION}' include='${Vulkan_INCLUDE_DIRS}'")
if(Vulkan_glslc_FOUND OR Vulkan_GLSLC_EXECUTABLE)
    message(STATUS "Luma: glslc -> ${Vulkan_GLSLC_EXECUTABLE}")
endif()

# ---------------------------------------------------------------------------
# Engine modules
# ---------------------------------------------------------------------------
add_subdirectory(Engine/Core)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Math/CMakeLists.txt)
    add_subdirectory(Engine/Math)
endif()

# Later modules are added as they come online (keeps every milestone buildable).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Serialization/CMakeLists.txt)
    add_subdirectory(Engine/Serialization)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/VFS/CMakeLists.txt)
    add_subdirectory(Engine/VFS)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Rendering/RHI/CMakeLists.txt)
    add_subdirectory(Engine/Rendering/RHI)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Platform/CMakeLists.txt)
    add_subdirectory(Engine/Platform)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Rendering/Grid/CMakeLists.txt)
    add_subdirectory(Engine/Rendering/Grid)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Rendering/Mesh/CMakeLists.txt)
    add_subdirectory(Engine/Rendering/Mesh)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Rendering/Vulkan/CMakeLists.txt)
    add_subdirectory(Engine/Rendering/Vulkan)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Project/CMakeLists.txt)
    add_subdirectory(Engine/Project)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Scene/CMakeLists.txt)
    add_subdirectory(Engine/Scene)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Engine/UI/Slate/CMakeLists.txt)
    add_subdirectory(Engine/UI/Slate)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Sandbox/CMakeLists.txt)
    add_subdirectory(Runtime/Sandbox)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Editor/Gizmo/CMakeLists.txt)
    add_subdirectory(Editor/Gizmo)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Editor/ProjectBrowser/CMakeLists.txt)
    add_subdirectory(Editor/ProjectBrowser)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Editor/LumaEditor/CMakeLists.txt)
    add_subdirectory(Editor/LumaEditor)
endif()

if(LUMA_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(Tests)
endif()

message(STATUS "Luma: configured LumaEngine ${PROJECT_VERSION} (tests=${LUMA_BUILD_TESTS})")
