add_library(LumaPanelContentBrowser STATIC
    src/ContentBrowser.cpp)

add_library(Luma::PanelContentBrowser ALIAS LumaPanelContentBrowser)

target_include_directories(LumaPanelContentBrowser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelContentBrowser
    PUBLIC Luma::Core Luma::Asset Luma::Scene Luma::Slate)

set_target_properties(LumaPanelContentBrowser PROPERTIES FOLDER "Editor/Panels")
