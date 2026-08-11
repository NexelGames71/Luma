# Luma::Grid - editor ground-grid line geometry (renderer-agnostic).

add_library(LumaGrid STATIC
    src/Grid.cpp)

add_library(Luma::Grid ALIAS LumaGrid)

target_include_directories(LumaGrid
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaGrid PUBLIC Luma::Core Luma::Math Luma::RHI)

set_target_properties(LumaGrid PROPERTIES FOLDER "Engine/Rendering")
