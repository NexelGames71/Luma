# LumaEditor.exe - one binary, two modes (project browser + editor), connected
# by relaunching itself with --project (Godot's model).

add_executable(LumaEditor
    main.cpp
    SplashScreen.cpp
    EditorScreen.cpp)

target_link_libraries(LumaEditor
    PRIVATE Luma::Core Luma::Math Luma::Platform Luma::RenderVulkan Luma::Slate
            Luma::Project Luma::ProjectBrowser)

set_target_properties(LumaEditor PROPERTIES
    OUTPUT_NAME "LumaEditor"
    FOLDER "Editor")
