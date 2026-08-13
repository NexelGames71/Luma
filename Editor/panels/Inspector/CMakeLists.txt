add_library(LumaPanelInspector STATIC
    src/Inspector.cpp)

add_library(Luma::PanelInspector ALIAS LumaPanelInspector)

target_include_directories(LumaPanelInspector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelInspector
    PUBLIC Luma::Core Luma::Scene Luma::Slate)

set_target_properties(LumaPanelInspector PROPERTIES FOLDER "Editor/Panels")
