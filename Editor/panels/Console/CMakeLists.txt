add_library(LumaPanelConsole STATIC
    src/Console.cpp)

add_library(Luma::PanelConsole ALIAS LumaPanelConsole)

target_include_directories(LumaPanelConsole
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(LumaPanelConsole
    PUBLIC Luma::Core Luma::Scene Luma::Slate)

set_target_properties(LumaPanelConsole PROPERTIES FOLDER "Editor/Panels")
