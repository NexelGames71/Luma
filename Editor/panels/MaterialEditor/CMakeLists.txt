add_library(LumaPanelMaterialEditor STATIC
    src/MaterialEditor.cpp)

add_library(Luma::PanelMaterialEditor ALIAS LumaPanelMaterialEditor)

target_include_directories(LumaPanelMaterialEditor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelMaterialEditor
    PUBLIC Luma::Core Luma::Scene Luma::Slate Luma::Material Luma::Asset)

set_target_properties(LumaPanelMaterialEditor PROPERTIES FOLDER "Editor/Panels")
