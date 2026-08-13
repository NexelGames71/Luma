# Test-only build of the ContentBrowser panel so the editor tests can
# link against its public API.
add_library(LumaPanelContentBrowserForTest STATIC
    ${CMAKE_SOURCE_DIR}/Editor/panels/ContentBrowser/src/ContentBrowser.cpp
    ${CMAKE_SOURCE_DIR}/Editor/panels/FileSystemTree/src/FileSystemTree.cpp)

add_library(Luma::PanelContentBrowserForTest ALIAS LumaPanelContentBrowserForTest)

target_include_directories(LumaPanelContentBrowserForTest
    PUBLIC
        ${CMAKE_SOURCE_DIR}/Editor/panels/ContentBrowser/include
        ${CMAKE_SOURCE_DIR}/Editor/panels)

target_link_libraries(LumaPanelContentBrowserForTest
    PUBLIC Luma::Core Luma::Asset Luma::Scene Luma::Slate
           Luma::PanelFileSystemTree)

set_target_properties(LumaPanelContentBrowserForTest PROPERTIES FOLDER "Tests/Editor")
