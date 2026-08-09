# Luma::Platform - OS window / input layer. Wraps GLFW (kept private).

luma_require_glfw()

add_library(LumaPlatform STATIC
    src/Window/GlfwWindow.cpp)

add_library(Luma::Platform ALIAS LumaPlatform)

target_include_directories(LumaPlatform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# glfw is PRIVATE: no GLFW symbols or headers escape Luma::Platform.
target_link_libraries(LumaPlatform
    PUBLIC Luma::Core
    PRIVATE glfw)

set_target_properties(LumaPlatform PROPERTIES FOLDER "Engine")
