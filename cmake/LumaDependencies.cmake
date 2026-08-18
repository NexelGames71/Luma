# LumaDependencies.cmake
# Declares third-party dependencies via FetchContent (version-pinned).
# Dependencies are only materialized when a module asks for them, via the
# luma_require_* helpers, so configuring Core alone stays fast and offline.

include(FetchContent)

# GLFW is fetched from git so we control the exact tag; disable its extras.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4          # pinned release
    GIT_SHALLOW    TRUE)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1       # pinned release
    GIT_SHALLOW    TRUE)

# Assimp for 3D asset importing (FBX, OBJ, GLTF, etc.)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
# Enable Assimp's built-in ZLIB to avoid external dependency
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
# Fix MSVC compilation flags for exception handling
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.0      # pinned release
    GIT_SHALLOW    TRUE)

# Idempotent helpers so multiple modules can request the same dependency.
function(luma_require_glfw)
    if(NOT TARGET glfw)
        FetchContent_MakeAvailable(glfw)
    endif()
endfunction()

function(luma_require_catch2)
    if(NOT TARGET Catch2::Catch2WithMain)
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
    endif()
endfunction()

function(luma_require_assimp)
    if(NOT TARGET assimp)
        FetchContent_MakeAvailable(assimp)
    endif()
endfunction()
