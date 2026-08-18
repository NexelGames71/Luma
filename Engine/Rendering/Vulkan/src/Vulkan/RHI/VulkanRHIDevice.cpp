#include "Vulkan/RHI/VulkanRHIDevice.h"

#include "Luma/RHI/VulkanRHIDevice.h"  // public (void*,void*) overload

#include <cstring>
#include <vector>

#include "Luma/Core/Log.h"
#include "Vulkan/RHI/VulkanRHICommandList.h"
#include "Vulkan/RHI/VulkanRHIPipeline.h"
#include "Vulkan/RHI/VulkanRHIResources.h"
#include "Vulkan/RHI/VulkanRHIShader.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanInstance.h"

namespace Luma {
namespace RHI {

// ============================================================================
// VulkanRHICommandQueue
// ============================================================================

VulkanRHICommandQueue::VulkanRHICommandQueue(VkQueue queue, const char* type)
    : m_queue(queue), m_type(type) {}

// We don't have a logger-once macro available; use a simple static guard so
// repeated calls during one run don't spam. The deferred renderer's first
// stage never calls Submit/Present/etc., so this never triggers in practice.
namespace { struct SubmitLogged { static bool& Once() { static bool v = false; return v; } }; }

void VulkanRHICommandQueue::Submit(u32 numLists, RHICommandList** lists) {
    if (numLists == 0 || !lists) return;

    // Collect the VkCommandBuffers from each RHI command list.
    std::vector<VkCommandBuffer> cmds;
    cmds.reserve(numLists);
    for (u32 i = 0; i < numLists; ++i) {
        auto* vkList = static_cast<VulkanRHICommandList*>(lists[i]);
        if (vkList && vkList->Handle())
            cmds.push_back(vkList->Handle());
    }
    if (cmds.empty()) return;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = static_cast<u32>(cmds.size());
    submit.pCommandBuffers = cmds.data();
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
}

void VulkanRHICommandQueue::Present() {
    if (!SubmitLogged::Once()) {
        SubmitLogged::Once() = true;
        LUMA_LOG_WARN("RHI", "VulkanRHICommandQueue::Present not yet implemented");
    }
}

u64 VulkanRHICommandQueue::Signal() { return 0; }
void VulkanRHICommandQueue::Wait(u64 /*fenceValue*/) {}
void VulkanRHICommandQueue::Flush() {}

// ============================================================================
// VulkanRHIContext
// ============================================================================

VulkanRHIContext::VulkanRHIContext(VulkanRHIDevice* device) : m_device(device) {}

RHIDevice* VulkanRHIContext::GetDevice() { return m_device; }

RHIResourceFactory* VulkanRHIContext::GetResourceFactory() {
    return m_device->GetResourceFactory();
}

RHIPipelineStateFactory* VulkanRHIContext::GetPipelineStateFactory() {
    return m_device->GetPipelineStateFactory();
}

RHIShaderFactory* VulkanRHIContext::GetShaderFactory() {
    return m_device->GetShaderFactory();
}

RHICommandList* VulkanRHIContext::CreateCommandList() {
    return m_device->CreateCommandList();
}

void VulkanRHIContext::DestroyCommandList(RHICommandList* cmdList) {
    m_device->DestroyCommandList(cmdList);
}

RHICommandQueue* VulkanRHIContext::GetGraphicsQueue() {
    return m_device->GetGraphicsQueue();
}
RHICommandQueue* VulkanRHIContext::GetComputeQueue() {
    return m_device->GetComputeQueue();
}
RHICommandQueue* VulkanRHIContext::GetCopyQueue() {
    return m_device->GetCopyQueue();
}

void VulkanRHIContext::Flush() { m_device->Flush(); }
bool VulkanRHIContext::IsDeviceLost() { return m_device->IsDeviceLost(); }

// ============================================================================
// VulkanRHIDevice
// ============================================================================

VulkanRHIDevice::VulkanRHIDevice(VulkanInstance* instance, VulkanDevice* device)
    : m_instance(instance), m_backend(device), m_context(this) {
    m_physical = device->Physical();
    m_logical = device->Logical();
    m_graphicsQueue = device->GraphicsQueue();

    QueryLimits();

    m_deviceName = m_props.deviceName;
    // Stash on the std::string with trailing null removed.
    if (!m_deviceName.empty() && m_deviceName.back() == '\0') {
        m_deviceName.pop_back();
    }
}

VulkanRHIDevice::~VulkanRHIDevice() = default;

const char* VulkanRHIDevice::GetDeviceName() { return m_deviceName.c_str(); }

RHIResourceFactory* VulkanRHIDevice::GetResourceFactory() {
    if (!m_resourceFactory) {
        m_resourceFactory = std::make_unique<VulkanRHIResourceFactory>(
            m_physical, m_logical);
    }
    return m_resourceFactory.get();
}

RHIPipelineStateFactory* VulkanRHIDevice::GetPipelineStateFactory() {
    if (!m_pipelineFactory) {
        m_pipelineFactory = std::make_unique<VulkanRHIPipelineStateFactory>();
    }
    return m_pipelineFactory.get();
}

RHIShaderFactory* VulkanRHIDevice::GetShaderFactory() {
    if (!m_shaderFactory) {
        m_shaderFactory = std::make_unique<VulkanRHIShaderFactory>();
    }
    return m_shaderFactory.get();
}

RHICommandList* VulkanRHIDevice::CreateCommandList() {
    return new VulkanRHICommandList(this);
}

void VulkanRHIDevice::DestroyCommandList(RHICommandList* cmdList) {
    delete cmdList;
}

RHICommandQueue* VulkanRHIDevice::GetGraphicsQueue() {
    if (!m_graphicsQueueAdapter) {
        m_graphicsQueueAdapter = std::make_unique<VulkanRHICommandQueue>(
            m_graphicsQueue, "graphics");
    }
    return m_graphicsQueueAdapter.get();
}

RHICommandQueue* VulkanRHIDevice::GetComputeQueue() {
    // Vulkan exposes graphics+present queues on the devices Luma targets;
    // until compute is wired, the graphics queue doubles as compute/copy.
    return GetGraphicsQueue();
}

RHICommandQueue* VulkanRHIDevice::GetCopyQueue() {
    return GetGraphicsQueue();
}

void VulkanRHIDevice::Flush() {
    if (m_logical != VK_NULL_HANDLE) vkDeviceWaitIdle(m_logical);
}

void VulkanRHIDevice::WaitIdle() {
    if (m_logical != VK_NULL_HANDLE) vkDeviceWaitIdle(m_logical);
}

bool VulkanRHIDevice::IsFormatSupported(ETextureFormat format, ETextureUsage usage) {
    // Conservative truth table covering the formats the deferred renderer's
    // GBuffer uses (RGBA8, RGBA16F, D32). Anything unknown returns false so
    // a misuse is loud rather than silently broken at GPU submit time.
    (void)usage;
    switch (format) {
        case ETextureFormat::R8G8B8A8_UNORM:
        case ETextureFormat::R8G8B8A8_SRGB:
        case ETextureFormat::R16G16B16A16_FLOAT:
        case ETextureFormat::R16G16_FLOAT:
        case ETextureFormat::R32_FLOAT:
        case ETextureFormat::R8G8_UNORM:
        case ETextureFormat::R8_UNORM:
        case ETextureFormat::D32_FLOAT:
        case ETextureFormat::D24_UNORM_S8_UINT:
        case ETextureFormat::D16_UNORM:
            return true;
        default:
            return false;
    }
}

ESampleCount VulkanRHIDevice::GetOptimalSampleCount(ETextureFormat format) {
    // The working scene path uses 4x MSAA on the color+depth attachments;
    // for now return 1x so the deferred renderer allocates single-sample
    // GBuffer targets. The next stage will thread this through the limits
    // query.
    (void)format;
    return ESampleCount::Count1;
}

void VulkanRHIDevice::QueryLimits() {
    vkGetPhysicalDeviceProperties(m_physical, &m_props);

    const auto& lim = m_props.limits;
    m_limits.maxTextureDimension1D = lim.maxImageDimension1D;
    m_limits.maxTextureDimension2D = lim.maxImageDimension2D;
    m_limits.maxTextureDimension3D = lim.maxImageDimension3D;
    m_limits.maxTextureDimensionCube = lim.maxImageDimensionCube;
    m_limits.maxTextureArrayLayers = lim.maxImageArrayLayers;
    m_limits.maxColorAttachments = 8;  // Vulkan guarantees at least 4; many GPUs do 8
    m_limits.maxUniformBufferRange = lim.maxUniformBufferRange;
    m_limits.maxPushConstantsSize = 128;
    m_limits.maxPerStageDescriptorSamplers = lim.maxPerStageDescriptorSamplers;
    m_limits.maxPerStageDescriptorUniformBuffers = lim.maxPerStageDescriptorUniformBuffers;
    m_limits.maxPerStageDescriptorStorageBuffers = lim.maxPerStageDescriptorStorageBuffers;
    m_limits.maxPerStageDescriptorSampledImages = lim.maxPerStageDescriptorSampledImages;
    m_limits.maxPerStageDescriptorStorageImages = lim.maxPerStageDescriptorStorageImages;
    m_limits.maxPerStageDescriptorInputAttachments = lim.maxPerStageDescriptorInputAttachments;
    m_limits.maxDescriptorSetSamplers = lim.maxDescriptorSetSamplers;
    m_limits.maxDescriptorSetUniformBuffers = lim.maxDescriptorSetUniformBuffers;
    m_limits.maxDescriptorSetStorageBuffers = lim.maxDescriptorSetStorageBuffers;
    m_limits.maxDescriptorSetSampledImages = lim.maxDescriptorSetSampledImages;
    m_limits.maxDescriptorSetStorageImages = lim.maxDescriptorSetStorageImages;
    m_limits.maxVertexInputAttributes = lim.maxVertexInputAttributes;
    m_limits.maxVertexInputBindings = lim.maxVertexInputBindings;
    m_limits.maxVertexInputAttributeOffset = lim.maxVertexInputAttributeOffset;
    m_limits.maxVertexInputBindingStride = lim.maxVertexInputBindingStride;
    m_limits.minUniformBufferOffsetAlignment =
        static_cast<u32>(lim.minUniformBufferOffsetAlignment);
    m_limits.minStorageBufferOffsetAlignment =
        static_cast<u32>(lim.minStorageBufferOffsetAlignment);
}

std::string VulkanRHIDevice::GetShaderDir() const {
#if defined(LUMA_SHADER_DIR)
    return LUMA_SHADER_DIR;
#else
    return "shaders";
#endif
}

// ============================================================================
// Backend registration
// ============================================================================

namespace {
// Static instance: the RHI backend registry stores function pointers; we
// hand it a trampoline that ignores the RHIInitDesc and constructs a
// VulkanRHIDevice. In practice main.cpp constructs the adapter directly off
// the existing VulkanDevice and never calls CreateRHIDevice — but registering
// keeps the legacy RHI.cpp globals consistent.
RHIDevice* TrampolineCreate(const RHIInitDesc& /*desc*/) {
    LUMA_LOG_WARN("RHI", "CreateRHIDevice('Vulkan') called without a pre-built "
                          "VulkanDevice; the adapter needs VulkanRenderer to "
                          "exist first. Use CreateVulkanRHIDevice(instance, "
                          "device) instead.");
    return nullptr;
}
struct Registrar {
    Registrar() { RegisterRHIBackend("Vulkan", &TrampolineCreate); }
};
static Registrar s_registrar;
}  // namespace

RHIDevice* CreateVulkanRHIDevice(VulkanInstance* instance, VulkanDevice* device) {
    return new VulkanRHIDevice(instance, device);
}

// Public-API overload: bridges the opaque void* handles declared in the public
// header (Luma/RHI/VulkanRHIDevice.h) to the concrete Vulkan types.
RHIDevice* CreateVulkanRHIDevice(VulkanInstanceHandle instance,
                                  VulkanDeviceHandle device) {
    return CreateVulkanRHIDevice(static_cast<VulkanInstance*>(instance),
                                static_cast<VulkanDevice*>(device));
}

}  // namespace RHI
}  // namespace Luma
