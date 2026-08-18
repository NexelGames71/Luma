# Luma::RenderGraph - Render graph system for managing render passes

add_library(LumaRenderGraph STATIC
    src/RenderGraph.cpp
    src/RenderGraphBuilder.cpp
    src/RenderGraphResources.cpp)

add_library(Luma::RenderGraph ALIAS LumaRenderGraph)

target_include_directories(LumaRenderGraph
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LumaRenderGraph
    PUBLIC Luma::Core Luma::RHI Luma::Math)

set_target_properties(LumaRenderGraph PROPERTIES FOLDER "Engine")