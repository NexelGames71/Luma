# Luma::RenderVulkan - Vulkan 1.3 backend implementing Luma::RHI.
# All Vulkan types are confined to this module (Vulkan linked PRIVATE).

add_library(LumaRenderVulkan STATIC
    src/Vulkan/VulkanCommon.cpp
    src/Vulkan/VulkanInstance.cpp
    src/Vulkan/VulkanDevice.cpp
    src/Vulkan/VulkanSwapchain.cpp
    src/Vulkan/VulkanRenderer.cpp)

add_library(Luma::RenderVulkan ALIAS LumaRenderVulkan)

target_include_directories(LumaRenderVulkan
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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

set_target_properties(LumaRenderVulkan PROPERTIES FOLDER "Engine")
