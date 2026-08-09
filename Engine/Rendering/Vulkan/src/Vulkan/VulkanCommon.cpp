#include "Vulkan/VulkanCommon.h"

namespace Luma {

const char* VkResultString(VkResult result) {
    switch (result) {
        case VK_SUCCESS:                     return "VK_SUCCESS";
        case VK_NOT_READY:                   return "VK_NOT_READY";
        case VK_TIMEOUT:                     return "VK_TIMEOUT";
        case VK_SUBOPTIMAL_KHR:              return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY:    return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:  return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:           return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:   return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_OUT_OF_DATE_KHR:       return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_SURFACE_LOST_KHR:      return "VK_ERROR_SURFACE_LOST_KHR";
        default:                             return "VK_ERROR_<other>";
    }
}

}  // namespace Luma
