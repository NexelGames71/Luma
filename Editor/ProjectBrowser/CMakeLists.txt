# Luma::ProjectBrowser - the Cave-style project browser screen (Slate UI).

add_library(LumaProjectBrowser STATIC
    src/ProjectBrowser.cpp)

add_library(Luma::ProjectBrowser ALIAS LumaProjectBrowser)

target_include_directories(LumaProjectBrowser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaProjectBrowser
    PUBLIC Luma::Core Luma::Slate Luma::Project)

set_target_properties(LumaProjectBrowser PROPERTIES FOLDER "Editor")
