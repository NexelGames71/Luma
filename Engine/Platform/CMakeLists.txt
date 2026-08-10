# Luma::Platform - OS window / input layer. Wraps GLFW (kept private).

luma_require_glfw()

add_library(LumaPlatform STATIC
    src/Window/GlfwWindow.cpp
    src/Process/Process.cpp)

add_library(Luma::Platform ALIAS LumaPlatform)

target_include_directories(LumaPlatform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# glfw + Vulkan are PRIVATE: no GLFW/Vulkan symbols or headers escape the module.
# Vulkan is used only for glfwCreateWindowSurface in GlfwWindow.cpp.
target_link_libraries(LumaPlatform
    PUBLIC Luma::Core
    PRIVATE glfw Vulkan::Vulkan)

set_target_properties(LumaPlatform PROPERTIES FOLDER "Engine")
