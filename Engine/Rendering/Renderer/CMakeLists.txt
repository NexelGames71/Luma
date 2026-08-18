# Luma::Renderer - Scene renderer and material system

add_library(LumaRenderer STATIC
    src/SceneRenderer.cpp
    src/PrimitiveSceneInfo.cpp
    src/StaticMeshBatch.cpp
    src/Material.cpp
    src/MaterialShader.cpp
    src/GBufferRendering.cpp
    src/Lighting.cpp
    src/ShadowRendering.cpp
    src/DeferredShadingRenderer.cpp)

add_library(Luma::Renderer ALIAS LumaRenderer)

target_include_directories(LumaRenderer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/Engine/Rendering/Vulkan/src)

target_link_libraries(LumaRenderer
    PUBLIC Luma::Core Luma::RHI Luma::Math Luma::Shader Luma::RenderGraph Luma::UniformBuffer Luma::Mesh Vulkan::Vulkan)

# Register deferred renderer factory
target_compile_definitions(LumaRenderer PRIVATE DEFERRED_RENDERER_EXPORTS)

set_target_properties(LumaRenderer PROPERTIES FOLDER "Engine")