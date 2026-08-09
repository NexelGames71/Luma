#pragma once

#include <vulkan/vulkan.h>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Log.h"
#include "Luma/Core/Types.h"

// Shared helpers for the Vulkan backend.

namespace Luma {

const char* VkResultString(VkResult result);

// Checks a VkResult; logs + asserts on failure. Use for calls that must succeed.
#define VK_CHECK(expr)                                                      \
    do {                                                                    \
        VkResult luma_vk_result = (expr);                                   \
        if (luma_vk_result != VK_SUCCESS) {                                 \
            LUMA_LOG_ERROR("Vulkan", "{} failed: {}", #expr,               \
                           ::Luma::VkResultString(luma_vk_result));         \
            LUMA_ASSERT(false, "Vulkan call failed");                       \
        }                                                                   \
    } while (0)

}  // namespace Luma
