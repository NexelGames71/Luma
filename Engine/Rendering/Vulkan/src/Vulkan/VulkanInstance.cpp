#include "Vulkan/VulkanInstance.h"

#include <cstdlib>
#include <cstring>

namespace Luma {
namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

// If the validation layer isn't registered with the loader, point the loader at
// the SDK's layer directory (baked in at build time) so validation works without
// requiring the user to set environment variables.
void EnsureLayerSearchPath() {
#if defined(LUMA_VULKAN_LAYER_PATH) && defined(_WIN32)
    if (std::getenv("VK_LAYER_PATH") == nullptr &&
        std::getenv("VK_ADD_LAYER_PATH") == nullptr) {
        _putenv_s("VK_ADD_LAYER_PATH", LUMA_VULKAN_LAYER_PATH);
    }
#endif
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) {
    const char* msg = data && data->pMessage ? data->pMessage : "";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LUMA_LOG_ERROR("Vulkan", "{}", msg);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LUMA_LOG_WARN("Vulkan", "{}", msg);
    } else {
        LUMA_LOG_TRACE("Vulkan", "{}", msg);
    }
    return VK_FALSE;
}

bool ValidationLayerAvailable() {
    u32 count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, kValidationLayer) == 0) return true;
    }
    return false;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugInfo() {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = &DebugCallback;
    return info;
}

}  // namespace

VulkanInstance::VulkanInstance(const std::string& appName,
                               const std::vector<const char*>& windowExtensions,
                               bool enableValidation) {
    if (enableValidation) EnsureLayerSearchPath();
    m_validationEnabled = enableValidation && ValidationLayerAvailable();
    if (enableValidation && !m_validationEnabled) {
        LUMA_LOG_WARN("Vulkan",
                      "validation requested but {} not available; disabling",
                      kValidationLayer);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Luma Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = windowExtensions;
    if (m_validationEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        m_layers.push_back(kValidationLayer);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<u32>(m_layers.size());
    createInfo.ppEnabledLayerNames = m_layers.data();

    // Chain a debug messenger so instance create/destroy is also covered.
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = MakeDebugInfo();
    if (m_validationEnabled) createInfo.pNext = &debugInfo;

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
    LUMA_LOG_INFO("Vulkan", "instance created (API 1.3, validation={})",
                  m_validationEnabled);

    if (m_validationEnabled) {
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance,
                                  "vkCreateDebugUtilsMessengerEXT"));
        if (create) {
            create(m_instance, &debugInfo, nullptr, &m_debugMessenger);
        }
    }
}

VulkanInstance::~VulkanInstance() {
    if (m_debugMessenger) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance,
                                  "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(m_instance, m_debugMessenger, nullptr);
    }
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

}  // namespace Luma
