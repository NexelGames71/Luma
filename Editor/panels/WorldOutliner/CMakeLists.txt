add_library(LumaPanelWorldOutliner STATIC
    src/WorldOutliner.cpp)

add_library(Luma::PanelWorldOutliner ALIAS LumaPanelWorldOutliner)

target_include_directories(LumaPanelWorldOutliner
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelWorldOutliner
    PUBLIC Luma::Core Luma::Scene Luma::Slate)

set_target_properties(LumaPanelWorldOutliner PROPERTIES FOLDER "Editor/Panels")
