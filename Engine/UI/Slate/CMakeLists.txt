# Luma::Slate - our own immediate-mode UI framework (drawn via the RHI).
# Uses stb_truetype (vendored) only for glyph rasterization.

add_library(LumaSlate STATIC
    src/Font.cpp
    src/DrawList.cpp
    src/Context.cpp)

add_library(Luma::Slate ALIAS LumaSlate)

target_include_directories(LumaSlate
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/ThirdParty/stb)

target_link_libraries(LumaSlate PUBLIC Luma::Core Luma::RHI Luma::Platform)

set_target_properties(LumaSlate PROPERTIES FOLDER "Engine/UI")
