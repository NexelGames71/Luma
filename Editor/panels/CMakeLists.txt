# Editor panels — each one is its own sub-module with a header + cpp + a
# public API in Luma::Editor::Panels::*. EditorScreen constructs the panel
# instances and calls panel->Draw(ui, rect, ctx) from each DockSpace drawer.

add_subdirectory(WorldOutliner)
add_subdirectory(Viewport)
add_subdirectory(Inspector)
add_subdirectory(Console)
add_subdirectory(ContentBrowser)
add_subdirectory(FileSystemTree)
