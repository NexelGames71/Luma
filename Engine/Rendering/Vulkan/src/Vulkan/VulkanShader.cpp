#include "Vulkan/VulkanShader.h"

#include <fstream>
#include <vector>

namespace Luma {

VkShaderModule LoadShaderModule(VkDevice device, const std::string& spirvPath) {
    std::ifstream file(spirvPath, std::ios::binary | std::ios::ate);
    if (!file) {
        LUMA_LOG_ERROR("Vulkan", "shader not found: {}", spirvPath);
        return VK_NULL_HANDLE;
    }
    std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        LUMA_LOG_ERROR("Vulkan", "invalid SPIR-V size ({}): {}", size,
                       spirvPath);
        return VK_NULL_HANDLE;
    }
    file.seekg(0);
    std::vector<u32> code(static_cast<usize>(size) / 4);
    file.read(reinterpret_cast<char*>(code.data()), size);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<usize>(size);
    info.pCode = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));
    return module;
}

}  // namespace Luma
