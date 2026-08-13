# Luma::VFS - virtual file system, well-known roots, and file watcher.
# Depends on Core (for types/log). No engine subsystems above it.

add_library(LumaVFS STATIC
    src/Path.cpp
    src/VFS.cpp
    src/FileWatcher.cpp)

add_library(Luma::VFS ALIAS LumaVFS)

target_include_directories(LumaVFS
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaVFS PUBLIC Luma::Core)

set_target_properties(LumaVFS PROPERTIES FOLDER "Engine/VFS")
