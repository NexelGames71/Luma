#pragma once

#include "Vulkan/VulkanCommon.h"

namespace Luma {

u32 FindMemoryType(VkPhysicalDevice physical, u32 typeBits,
                   VkMemoryPropertyFlags properties);

// A VkBuffer plus its backing memory (optionally persistently mapped).
struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

GpuBuffer CreateBuffer(VkPhysicalDevice physical, VkDevice device,
                       VkDeviceSize size, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties, bool persistentMap);

void DestroyBuffer(VkDevice device, GpuBuffer& buffer);

// Allocates + begins a single-use command buffer from `pool`.
VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool pool);

// Ends, submits (on `queue`), waits, and frees a one-time command buffer.
void EndOneTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue,
                        VkCommandBuffer cmd);

}  // namespace Luma
