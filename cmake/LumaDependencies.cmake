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
