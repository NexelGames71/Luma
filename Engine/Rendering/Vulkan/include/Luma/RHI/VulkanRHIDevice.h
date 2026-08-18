#pragma once

#include "Luma/RHI/RHIContext.h"

// Public factory for the Vulkan-backed RHI adapter. Mirrors
// Luma/RHI/VulkanRenderer.h: the concrete VulkanRHIDevice and all Vulkan
// types stay private to Luma::RenderVulkan; callers see only RHIDevice.
//
// Usage:
//   auto* device = CreateVulkanRHIDevice(renderer->GetVulkanInstance(),
//                                       renderer->GetVulkanDevice());
//   deferredRenderer->Initialize(device);
//   delete device;
//
// The adapter is non-owning: the VulkanInstance and VulkanDevice must outlive
// the adapter. DestroyRHIDevice (or plain delete) frees only the adapter.

namespace Luma {
namespace RHI {

// Opaque handles; the concrete types are private to Luma::RenderVulkan.
using VulkanInstanceHandle = void*;
using VulkanDeviceHandle = void*;

RHIDevice* CreateVulkanRHIDevice(VulkanInstanceHandle instance,
                                  VulkanDeviceHandle device);

}  // namespace RHI
}  // namespace Luma
