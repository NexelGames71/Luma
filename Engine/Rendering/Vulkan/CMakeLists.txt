# Luma::RenderVulkan - Vulkan 1.3 backend implementing Luma::RHI.
# All Vulkan types are confined to this module (Vulkan linked PRIVATE).

add_library(LumaRenderVulkan STATIC
    src/Vulkan/VulkanCommon.cpp
    src/Vulkan/VulkanInstance.cpp
    src/Vulkan/VulkanDevice.cpp
    src/Vulkan/VulkanSwapchain.cpp
    src/Vulkan/VulkanShader.cpp
    src/Vulkan/VulkanMemory.cpp
    src/Vulkan/VulkanTexture.cpp
    src/Vulkan/UI/VulkanUIPass.cpp
    src/Vulkan/VulkanRenderer.cpp)

add_library(Luma::RenderVulkan ALIAS LumaRenderVulkan)

# --- Shader compilation: GLSL -> SPIR-V via glslc -------------------------
set(_luma_shader_src ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(_luma_shader_out ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${_luma_shader_out})

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Luma: glslc not found (needed to compile shaders)")
endif()

set(_luma_spv_outputs)
foreach(shader ui.vert ui.frag)
    add_custom_command(
        OUTPUT ${_luma_shader_out}/${shader}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3
                ${_luma_shader_src}/${shader}
                -o ${_luma_shader_out}/${shader}.spv
        DEPENDS ${_luma_shader_src}/${shader}
        COMMENT "glslc ${shader}"
        VERBATIM)
    list(APPEND _luma_spv_outputs ${_luma_shader_out}/${shader}.spv)
endforeach()

add_custom_target(LumaShaders DEPENDS ${_luma_spv_outputs})
add_dependencies(LumaRenderVulkan LumaShaders)

target_include_directories(LumaRenderVulkan
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/ThirdParty/stb)

target_link_libraries(LumaRenderVulkan
    PUBLIC Luma::Core Luma::RHI Luma::Platform
    PRIVATE Vulkan::Vulkan)

# Let the engine locate the SDK's validation layer even when it isn't registered
# with the loader (common on machines without VULKAN_SDK set globally).
if(DEFINED ENV{VULKAN_SDK})
    set(_luma_vk_layer_dir "$ENV{VULKAN_SDK}/Bin")
elseif(Vulkan_ROOT)
    set(_luma_vk_layer_dir "${Vulkan_ROOT}/Bin")
endif()
if(_luma_vk_layer_dir)
    target_compile_definitions(LumaRenderVulkan PRIVATE
        LUMA_VULKAN_LAYER_PATH="${_luma_vk_layer_dir}")
endif()

target_compile_definitions(LumaRenderVulkan PRIVATE
    LUMA_SHADER_DIR="${_luma_shader_out}")

set_target_properties(LumaRenderVulkan PROPERTIES FOLDER "Engine")
