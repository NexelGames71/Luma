#pragma once

#include <string>
#include <vector>

#include "Vulkan/VulkanCommon.h"

namespace Luma {

// Owns the VkInstance and (when validation is on) the debug messenger.
class VulkanInstance {
public:
    VulkanInstance(const std::string& appName,
                   const std::vector<const char*>& windowExtensions,
                   bool enableValidation);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    VkInstance Handle() const { return m_instance; }
    bool ValidationEnabled() const { return m_validationEnabled; }
    const std::vector<const char*>& EnabledLayers() const { return m_layers; }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    bool m_validationEnabled = false;
    std::vector<const char*> m_layers;
};

}  // namespace Luma
