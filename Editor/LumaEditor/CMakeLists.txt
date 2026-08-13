# LumaEditor.exe - one binary, two modes (project browser + editor), connected
# by relaunching itself with --project (Godot's model).

add_executable(LumaEditor
    main.cpp
    SplashScreen.cpp
    EditorScreen.cpp)

target_link_libraries(LumaEditor
    PRIVATE Luma::Core Luma::Math Luma::Platform Luma::RenderVulkan Luma::Slate
            Luma::Project Luma::ProjectBrowser Luma::Grid Luma::Scene
            Luma::Gizmo Luma::VFS)

set_target_properties(LumaEditor PROPERTIES
    OUTPUT_NAME "LumaEditor"
    FOLDER "Editor")

# Stage the editor content tree (logos, fonts, splash assets) next to the
# executable so the binary is self-contained when launched outside the repo
# (CI, packaging, IDE F5, etc.). Keep the source of truth under Content/Editor.
add_custom_command(TARGET LumaEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Content/Editor
        $<TARGET_FILE_DIR:LumaEditor>/Content/Editor
    COMMENT "Staging Content/Editor next to LumaEditor.exe")
