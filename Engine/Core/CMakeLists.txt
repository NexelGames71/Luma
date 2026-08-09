# Luma::Core - foundational module. Depends only on the C++ standard library.

# Sources are added task-by-task as the module comes online.
add_library(LumaCore STATIC
    src/Version.cpp
    src/Assert.cpp
    src/Log.cpp
    src/Config.cpp)

add_library(Luma::Core ALIAS LumaCore)

target_include_directories(LumaCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaCore PUBLIC Luma::Flags)

target_compile_definitions(LumaCore PRIVATE
    LUMA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LUMA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LUMA_VERSION_PATCH=${PROJECT_VERSION_PATCH})

set_target_properties(LumaCore PROPERTIES FOLDER "Engine")
