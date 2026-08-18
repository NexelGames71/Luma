#pragma once

#include <memory>
#include <string>

#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHIPipeline.h"
#include "Luma/RHI/RHIShader.h"

#include <vulkan/vulkan.h>

// VulkanRHIResourceFactory is needed for the unique_ptr member.
#include "Vulkan/RHI/VulkanRHIResources.h"

// Forward declarations of the existing Vulkan backend types so the factory
// can accept them as opaque handles without pulling in more.
namespace Luma {
class VulkanInstance;
class VulkanDevice;
}  // namespace Luma

namespace Luma {
namespace RHI {

// ============================================================================
// VulkanRHICommandQueue
// ============================================================================

// Adapter that exposes an existing VkQueue as RHICommandQueue. Non-owning;
// the underlying VkQueue is owned by VulkanDevice.
class VulkanRHICommandQueue final : public RHICommandQueue {
public:
    VulkanRHICommandQueue(VkQueue queue, const char* type);
    ~VulkanRHICommandQueue() override = default;

    void Submit(u32 numLists, RHICommandList** lists) override;
    void Present() override;
    u64 Signal() override;
    void Wait(u64 fenceValue) override;
    void Flush() override;
    const char* GetQueueType() const override { return m_type; }

    VkQueue Handle() const { return m_queue; }

private:
    VkQueue m_queue = VK_NULL_HANDLE;
    const char* m_type = "graphics";
};

// ============================================================================
// VulkanRHIContext
// ============================================================================

class VulkanRHIDevice;

class VulkanRHIContext final : public RHIContext {
public:
    explicit VulkanRHIContext(VulkanRHIDevice* device);
    ~VulkanRHIContext() override = default;

    RHIDevice* GetDevice() override;
    RHIResourceFactory* GetResourceFactory() override;
    RHIPipelineStateFactory* GetPipelineStateFactory() override;
    RHIShaderFactory* GetShaderFactory() override;
    RHICommandList* CreateCommandList() override;
    void DestroyCommandList(RHICommandList* cmdList) override;
    RHICommandQueue* GetGraphicsQueue() override;
    RHICommandQueue* GetComputeQueue() override;
    RHICommandQueue* GetCopyQueue() override;
    void Flush() override;
    bool IsDeviceLost() override;

private:
    VulkanRHIDevice* m_device;
};

// ============================================================================
// VulkanRHIDevice
// ============================================================================

// Adapter over the existing Vulkan backend objects. Non-owning; the caller
// (typically VulkanRenderer or main.cpp) keeps the VulkanInstance /
// VulkanDevice alive for at least the lifetime of the adapter.
class VulkanRHIDevice final : public RHIDevice {
public:
    VulkanRHIDevice(VulkanInstance* instance, VulkanDevice* device);
    ~VulkanRHIDevice() override;

    VulkanRHIDevice(const VulkanRHIDevice&) = delete;
    VulkanRHIDevice& operator=(const VulkanRHIDevice&) = delete;

    // RHIDevice interface
    const char* GetDeviceName() override;
    RHIContext* GetContext() override { return &m_context; }
    RHIResourceFactory* GetResourceFactory() override;
    RHIPipelineStateFactory* GetPipelineStateFactory() override;
    RHIShaderFactory* GetShaderFactory() override;
    RHICommandList* CreateCommandList() override;
    void DestroyCommandList(RHICommandList* cmdList) override;
    RHICommandQueue* GetGraphicsQueue() override;
    RHICommandQueue* GetComputeQueue() override;
    RHICommandQueue* GetCopyQueue() override;
    void Flush() override;
    void WaitIdle() override;
    bool IsDeviceLost() override { return false; }
    const DeviceLimits& GetLimits() override { return m_limits; }
    bool IsFormatSupported(ETextureFormat format, ETextureUsage usage) override;
    ESampleCount GetOptimalSampleCount(ETextureFormat format) override;

    std::string GetShaderDir() const;

    // Backend-internal accessors used by the resource/pipeline/shader factories.
    VkPhysicalDevice PhysicalHandle() const { return m_physical; }
    VkDevice LogicalHandle() const { return m_logical; }
    VkQueue GraphicsQueueHandle() const { return m_graphicsQueue; }
    VulkanDevice* Backend() const { return m_backend; }

private:
    void QueryLimits();

    VulkanInstance* m_instance = nullptr;  // borrowed
    VulkanDevice* m_backend = nullptr;     // borrowed
    VulkanRHIContext m_context;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_logical = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties m_props{};
    DeviceLimits m_limits{};
    std::string m_deviceName;

    // Lazily-created subobjects.
    std::unique_ptr<VulkanRHIResourceFactory> m_resourceFactory;
    std::unique_ptr<RHIPipelineStateFactory> m_pipelineFactory;
    std::unique_ptr<RHIShaderFactory> m_shaderFactory;
    std::unique_ptr<VulkanRHICommandQueue> m_graphicsQueueAdapter;
};

// Factory: builds a VulkanRHIDevice wrapping the existing Vulkan backend.
// The returned pointer is owned by the caller (use delete).
RHIDevice* CreateVulkanRHIDevice(VulkanInstance* instance, VulkanDevice* device);

}  // namespace RHI
}  // namespace Luma