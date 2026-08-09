#pragma once

#include <memory>

#include "Luma/RHI/Renderer.h"

// Public factory for the Vulkan backend. The concrete VulkanRenderer and all
// Vulkan types stay private to Luma::RenderVulkan; callers see only Renderer.

namespace Luma {

class Window;

std::unique_ptr<Renderer> CreateVulkanRenderer(Window& window,
                                               const RendererConfig& config);

}  // namespace Luma
