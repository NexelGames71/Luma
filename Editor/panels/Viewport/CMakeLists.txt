add_library(LumaPanelViewport STATIC
    src/Viewport.cpp)

add_library(Luma::PanelViewport ALIAS LumaPanelViewport)

target_include_directories(LumaPanelViewport
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelViewport
    PUBLIC Luma::Core Luma::Math Luma::Scene Luma::Slate Luma::Gizmo)

set_target_properties(LumaPanelViewport PROPERTIES FOLDER "Editor/Panels")
