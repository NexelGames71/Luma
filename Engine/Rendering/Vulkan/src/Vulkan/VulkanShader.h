#pragma once

#include <string>

#include "Vulkan/VulkanCommon.h"

namespace Luma {

// Loads a compiled SPIR-V file and creates a VkShaderModule. Returns
// VK_NULL_HANDLE on failure (logged). Caller owns/destroys the module.
VkShaderModule LoadShaderModule(VkDevice device, const std::string& spirvPath);

}  // namespace Luma
