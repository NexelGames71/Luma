#include "Luma/Rendering/Vulkan/VulkanDeferredRenderer.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

#include "Luma/Core/Log.h"
#include "Luma/Mesh/Mesh.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"
#include "Vulkan/Grid/VulkanGridPass.h"
#include "Vulkan/UI/VulkanUIPass.h"
#include "Vulkan/VulkanShader.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {
namespace Rendering {

namespace {

void SetVec3(f32* dst, const Math::Vec3& v, f32 w = 1.0f) {
    dst[0] = v.x;
    dst[1] = v.y;
    dst[2] = v.z;
    dst[3] = w;
}

// Transforms a world point by a column-major matrix (w = 1, no perspective).
Math::Vec3 TransformPoint(const Math::Mat4& m, const Math::Vec3& p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}

}  // namespace

VulkanDeferredRenderer::VulkanDeferredRenderer(
    VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue,
    uint32_t graphicsQueueFamilyIndex, VulkanUIPass& uiPass,
    const std::string& shaderDir)
    : m_physicalDevice(physicalDevice)
    , m_device(device)
    , m_graphicsQueue(graphicsQueue)
    , m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex)
    , m_uiPass(uiPass)
    , m_shaderDir(shaderDir)
    , m_initialized(false)
    , m_gbufferPipeline(VK_NULL_HANDLE)
    , m_lightingPipeline(VK_NULL_HANDLE)
    , m_gbufferLayout(VK_NULL_HANDLE)
    , m_lightingLayout(VK_NULL_HANDLE)
    , m_gbufferSetLayout(VK_NULL_HANDLE)
    , m_gbufferPool(VK_NULL_HANDLE)
    , m_gbufferSet(VK_NULL_HANDLE)
    , m_lightingSetLayout(VK_NULL_HANDLE)
    , m_lightingPool(VK_NULL_HANDLE)
    , m_lightingSet(VK_NULL_HANDLE)
    , m_commandPool(VK_NULL_HANDLE)
    , m_commandBuffer(VK_NULL_HANDLE)
    , m_fence(VK_NULL_HANDLE)
{
    m_gBuffer.width = 0;
    m_gBuffer.height = 0;
    m_gBuffer.sampler = VK_NULL_HANDLE;
    m_gBuffer.albedo = {};
    m_gBuffer.normal = {};
    m_gBuffer.material = {};
    m_gBuffer.depth = {};
    m_gBuffer.lightAccum = {};
}

VulkanDeferredRenderer::~VulkanDeferredRenderer() {
    Cleanup();
}

bool VulkanDeferredRenderer::Initialize(int32_t width, int32_t height) {
    if (m_initialized) {
        LUMA_LOG_WARN("VulkanDeferredRenderer", "Already initialized");
        return true;
    }

    if (width <= 0 || height <= 0) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Invalid dimensions {}x{}",
                       width, height);
        return false;
    }

    LUMA_LOG_INFO("VulkanDeferredRenderer",
                  "Initializing deferred renderer ({}x{})", width, height);

    if (!CreateCommandPool()) return false;
    
    // Create fence for command buffer synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(m_device, &fenceInfo, nullptr, &m_fence) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create fence");
        return false;
    }
    
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to allocate command buffer");
        return false;
    }
    
    if (!CreateGBuffer(width, height)) return false;
    if (!CreateShadowResources()) return false;
    if (!CreateUBOs()) return false;
    if (!CreateLayouts()) return false;
    if (!CreateDescriptorSets()) return false;
    if (!CreatePipelines(m_shaderDir)) return false;
    if (!CreateShadowPipeline(m_shaderDir)) return false;
    CreatePrimitives();

    // Editor ground grid: 1-sample targets matching the deferred pass formats.
    m_gridPass = std::make_unique<VulkanGridPass>(
        m_physicalDevice, m_device, m_shaderDir, VK_SAMPLE_COUNT_1_BIT,
        kLightFormat, kDepthFormat);

    // The viewport may resize immediately after startup. Register the new
    // light-accumulation view here so the Slate viewport never observes a
    // zero or stale external texture handle during that transition.
    if (m_textureHandle == 0) {
        m_textureHandle = m_uiPass.RegisterExternalTexture(
            m_gBuffer.lightAccum.view);
    } else {
        m_uiPass.UpdateExternalTexture(m_textureHandle,
                                       m_gBuffer.lightAccum.view);
    }
    m_registeredLightView = m_gBuffer.lightAccum.view;

    m_initialized = true;
    LUMA_LOG_INFO("VulkanDeferredRenderer",
                  "Initialized successfully (GBuffer: {}x{})", width, height);
    return true;
}

void VulkanDeferredRenderer::Cleanup() {
    if (!m_initialized && m_commandPool == VK_NULL_HANDLE) return;

    if (m_fence != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    m_gridPass.reset();
    CleanupCustomMeshes();
    CleanupPrimitives();
    DestroyBuffer(m_device, m_gbufferUBO);
    DestroyBuffer(m_device, m_lightingUBO);
    DestroyShadowResources();

    if (m_shadowPipeline)
        vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    if (m_shadowLayout) vkDestroyPipelineLayout(m_device, m_shadowLayout, nullptr);
    m_shadowPipeline = VK_NULL_HANDLE;
    m_shadowLayout = VK_NULL_HANDLE;

    if (m_lightingPipeline)
        vkDestroyPipeline(m_device, m_lightingPipeline, nullptr);
    if (m_gbufferPipeline)
        vkDestroyPipeline(m_device, m_gbufferPipeline, nullptr);
    if (m_lightingLayout)
        vkDestroyPipelineLayout(m_device, m_lightingLayout, nullptr);
    if (m_gbufferLayout)
        vkDestroyPipelineLayout(m_device, m_gbufferLayout, nullptr);
    m_lightingPipeline = VK_NULL_HANDLE;
    m_gbufferPipeline = VK_NULL_HANDLE;
    m_lightingLayout = VK_NULL_HANDLE;
    m_gbufferLayout = VK_NULL_HANDLE;

    if (m_lightingPool)
        vkDestroyDescriptorPool(m_device, m_lightingPool, nullptr);
    if (m_gbufferPool)
        vkDestroyDescriptorPool(m_device, m_gbufferPool, nullptr);
    if (m_lightingSetLayout)
        vkDestroyDescriptorSetLayout(m_device, m_lightingSetLayout, nullptr);
    if (m_gbufferSetLayout)
        vkDestroyDescriptorSetLayout(m_device, m_gbufferSetLayout, nullptr);
    m_lightingPool = VK_NULL_HANDLE;
    m_gbufferPool = VK_NULL_HANDLE;
    m_lightingSetLayout = VK_NULL_HANDLE;
    m_gbufferSetLayout = VK_NULL_HANDLE;

    if (m_commandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }
    if (m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
    if (m_gbufferSetLayout)
        vkDestroyDescriptorSetLayout(m_device, m_gbufferSetLayout, nullptr);
    m_lightingPool = VK_NULL_HANDLE;
    m_gbufferPool = VK_NULL_HANDLE;
    m_lightingSetLayout = VK_NULL_HANDLE;
    m_gbufferSetLayout = VK_NULL_HANDLE;

    if (m_gBuffer.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_gBuffer.framebuffer, nullptr);
        m_gBuffer.framebuffer = VK_NULL_HANDLE;
    }
    if (m_gBuffer.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_gBuffer.renderPass, nullptr);
        m_gBuffer.renderPass = VK_NULL_HANDLE;
    }
    if (m_gBuffer.lightingFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_gBuffer.lightingFramebuffer, nullptr);
        m_gBuffer.lightingFramebuffer = VK_NULL_HANDLE;
    }
    if (m_gBuffer.lightingRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_gBuffer.lightingRenderPass, nullptr);
        m_gBuffer.lightingRenderPass = VK_NULL_HANDLE;
    }

    DestroyAttachment(m_gBuffer.lightAccum);
    DestroyAttachment(m_gBuffer.depth);
    DestroyAttachment(m_gBuffer.material);
    DestroyAttachment(m_gBuffer.normal);
    DestroyAttachment(m_gBuffer.albedo);

    if (m_gBuffer.sampler)
        vkDestroySampler(m_device, m_gBuffer.sampler, nullptr);
    m_gBuffer.sampler = VK_NULL_HANDLE;

    if (m_commandPool)
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_commandBuffer = VK_NULL_HANDLE;
    m_fence = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;

    m_initialized = false;
    m_gBufferInitialized = false;
}

bool VulkanDeferredRenderer::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create command pool");
        return false;
    }

    return true;
}

bool VulkanDeferredRenderer::CreateGBuffer(int32_t width, int32_t height) {
    m_gBuffer.width = width;
    m_gBuffer.height = height;

    // Create MRT attachments
    if (!CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          &m_gBuffer.albedo)) {
        return false;
    }
    if (!CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          &m_gBuffer.normal)) {
        return false;
    }
    if (!CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          &m_gBuffer.material)) {
        return false;
    }
    if (!CreateAttachment(VK_FORMAT_D32_SFLOAT,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT,
                          &m_gBuffer.depth)) {
        return false;
    }
    if (!CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          &m_gBuffer.lightAccum)) {
        return false;
    }

    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffer()) return false;
    if (!CreateLightingRenderPass()) return false;
    if (!CreateLightingFramebuffer()) return false;
    if (!CreateSampler()) return false;

    return true;
}

bool VulkanDeferredRenderer::CreateRenderPass() {
    // MRT attachments: albedo, normal, material, lightAccum, depth
    std::array<VkAttachmentDescription, 5> attachments{};

    // Albedo
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Normal
    attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Material
    attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Light accumulation
    attachments[3].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth
    attachments[4].format = VK_FORMAT_D32_SFLOAT;
    attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[4].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array<VkAttachmentReference, 4> colorReferences{};
    colorReferences[0].attachment = 0; // albedo
    colorReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorReferences[1].attachment = 1; // normal
    colorReferences[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorReferences[2].attachment = 2; // material
    colorReferences[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorReferences[3].attachment = 3; // lightAccum
    colorReferences[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 4;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = colorReferences.data();
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pDepthStencilAttachment = &depthReference;

    // Subpass dependencies
    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_gBuffer.renderPass) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create render pass");
        return false;
    }

    return true;
}

bool VulkanDeferredRenderer::CreateLightingRenderPass() {
    VkAttachmentDescription attachment{};
    attachment.format = kLightFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color;
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    return vkCreateRenderPass(m_device, &info, nullptr,
                              &m_gBuffer.lightingRenderPass) == VK_SUCCESS;
}

bool VulkanDeferredRenderer::CreateLightingFramebuffer() {
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = m_gBuffer.lightingRenderPass;
    info.attachmentCount = 1;
    info.pAttachments = &m_gBuffer.lightAccum.view;
    info.width = static_cast<u32>(m_gBuffer.width);
    info.height = static_cast<u32>(m_gBuffer.height);
    info.layers = 1;
    return vkCreateFramebuffer(m_device, &info, nullptr,
                               &m_gBuffer.lightingFramebuffer) == VK_SUCCESS;
}

bool VulkanDeferredRenderer::CreateAttachment(VkFormat format, VkImageUsageFlags usage,
                          Attachment* attachment) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_gBuffer.width;
    imageInfo.extent.height = m_gBuffer.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &attachment->image) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create image");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, attachment->image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(m_physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &attachment->memory) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to allocate image memory");
        vkDestroyImage(m_device, attachment->image, nullptr);
        return false;
    }

    vkBindImageMemory(m_device, attachment->image, attachment->memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attachment->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (format == VK_FORMAT_D32_SFLOAT) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &attachment->view) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create image view");
        return false;
    }

    attachment->format = format;
    return true;
}

void VulkanDeferredRenderer::DestroyAttachment(Attachment& attachment) {
    if (attachment.view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, attachment.view, nullptr);
        attachment.view = VK_NULL_HANDLE;
    }
    if (attachment.image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, attachment.image, nullptr);
        attachment.image = VK_NULL_HANDLE;
    }
    if (attachment.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, attachment.memory, nullptr);
        attachment.memory = VK_NULL_HANDLE;
    }
}

bool VulkanDeferredRenderer::CreateFramebuffer() {
    std::array<VkImageView, 5> attachments{};
    attachments[0] = m_gBuffer.albedo.view;
    attachments[1] = m_gBuffer.normal.view;
    attachments[2] = m_gBuffer.material.view;
    attachments[3] = m_gBuffer.lightAccum.view;
    attachments[4] = m_gBuffer.depth.view;

    VkFramebufferCreateInfo fbufInfo{};
    fbufInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufInfo.renderPass = m_gBuffer.renderPass;
    fbufInfo.pAttachments = attachments.data();
    fbufInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufInfo.width = m_gBuffer.width;
    fbufInfo.height = m_gBuffer.height;
    fbufInfo.layers = 1;

    if (vkCreateFramebuffer(m_device, &fbufInfo, nullptr, &m_gBuffer.framebuffer) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create framebuffer");
        return false;
    }

    return true;
}

bool VulkanDeferredRenderer::CreateSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_gBuffer.sampler) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to create sampler");
        return false;
    }

    return true;
}

bool VulkanDeferredRenderer::CreateUBOs() {
    m_gbufferUBO = CreateBuffer(m_physicalDevice, m_device, sizeof(GbufferUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
    m_lightingUBO = CreateBuffer(m_physicalDevice, m_device, sizeof(LightingUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
    return m_gbufferUBO.buffer != VK_NULL_HANDLE && m_lightingUBO.buffer != VK_NULL_HANDLE;
}

bool VulkanDeferredRenderer::CreateLayouts() {
    VkDescriptorSetLayoutBinding gb{};
    gb.binding = 0; gb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    gb.descriptorCount = 1; gb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo gi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    gi.bindingCount = 1; gi.pBindings = &gb;
    if (vkCreateDescriptorSetLayout(m_device, &gi, nullptr, &m_gbufferSetLayout) != VK_SUCCESS) return false;

    VkDescriptorSetLayoutBinding lb[6]{};
    for (u32 i = 0; i < 4; ++i) { lb[i].binding = i; lb[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; lb[i].descriptorCount = 1; lb[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; }
    lb[4].binding = 4; lb[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; lb[4].descriptorCount = 1; lb[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lb[5].binding = 5; lb[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; lb[5].descriptorCount = 1; lb[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.bindingCount = 6; li.pBindings = lb;
    if (vkCreateDescriptorSetLayout(m_device, &li, nullptr, &m_lightingSetLayout) != VK_SUCCESS) return false;

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPush)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1; pl.pSetLayouts = &m_gbufferSetLayout; pl.pushConstantRangeCount = 1; pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(m_device, &pl, nullptr, &m_gbufferLayout) != VK_SUCCESS) return false;
    pl.pSetLayouts = &m_lightingSetLayout; pl.pushConstantRangeCount = 0; pl.pPushConstantRanges = nullptr;
    return vkCreatePipelineLayout(m_device, &pl, nullptr, &m_lightingLayout) == VK_SUCCESS;
}

bool VulkanDeferredRenderer::CreateDescriptorSets() {
    VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets = 2; pi.poolSizeCount = 2; pi.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(m_device, &pi, nullptr, &m_lightingPool) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool = m_lightingPool; ai.descriptorSetCount = 1; ai.pSetLayouts = &m_gbufferSetLayout;
    if (vkAllocateDescriptorSets(m_device, &ai, &m_gbufferSet) != VK_SUCCESS) return false;
    ai.pSetLayouts = &m_lightingSetLayout;
    if (vkAllocateDescriptorSets(m_device, &ai, &m_lightingSet) != VK_SUCCESS) return false;
    VkDescriptorBufferInfo gb{m_gbufferUBO.buffer, 0, sizeof(GbufferUBO)};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; w.dstSet = m_gbufferSet; w.dstBinding = 0; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.descriptorCount = 1; w.pBufferInfo = &gb;
    vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
    VkDescriptorBufferInfo lb{m_lightingUBO.buffer, 0, sizeof(LightingUBO)};
    VkDescriptorImageInfo shadow{m_shadowSampler, m_shadowArrayView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo g[4] = {{m_gBuffer.sampler, m_gBuffer.albedo.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {m_gBuffer.sampler, m_gBuffer.normal.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {m_gBuffer.sampler, m_gBuffer.material.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {m_gBuffer.sampler, m_gBuffer.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}};
    VkWriteDescriptorSet writes[6]{};
    for (u32 i = 0; i < 4; ++i) { writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[i].dstSet = m_lightingSet; writes[i].dstBinding = i; writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[i].descriptorCount = 1; writes[i].pImageInfo = &g[i]; }
    writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[4].dstSet = m_lightingSet; writes[4].dstBinding = 4; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[4].descriptorCount = 1; writes[4].pBufferInfo = &lb;
    writes[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[5].dstSet = m_lightingSet; writes[5].dstBinding = 5; writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[5].descriptorCount = 1; writes[5].pImageInfo = &shadow;
    vkUpdateDescriptorSets(m_device, 6, writes, 0, nullptr);
    return true;
}

bool VulkanDeferredRenderer::CreatePipelines(const std::string& shaderDir) {
    VkShaderModule gv = LoadShaderModule(m_device, shaderDir + "/gbuffer.vert.spv");
    VkShaderModule gf = LoadShaderModule(m_device, shaderDir + "/gbuffer.frag.spv");
    VkShaderModule lv = LoadShaderModule(m_device, shaderDir + "/deferred_lighting.vert.spv");
    VkShaderModule lf = LoadShaderModule(m_device, shaderDir + "/deferred_lighting.frag.spv");
    if (!gv || !gf || !lv || !lf) return false;
    VkPipelineShaderStageCreateInfo gs[2]{};
    gs[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, gv, "main", nullptr};
    gs[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, gf, "main", nullptr};
    VkVertexInputBindingDescription binding{0, sizeof(MeshVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)}};
    VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO}; vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &binding; vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = attrs;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = 1; vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.cullMode = VK_CULL_MODE_BACK_BIT; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 4; VkPipelineColorBlendAttachmentState blends[4] = {blend, blend, blend, blend}; cb.pAttachments = blends;
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}; VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dyn;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; info.stageCount = 2; info.pStages = gs; info.pVertexInputState = &vin; info.pInputAssemblyState = &ia; info.pViewportState = &vp; info.pRasterizationState = &rs; info.pMultisampleState = &ms; info.pDepthStencilState = &ds; info.pColorBlendState = &cb; info.pDynamicState = &dynamic; info.layout = m_gbufferLayout; info.renderPass = m_gBuffer.renderPass;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info, nullptr, &m_gbufferPipeline) != VK_SUCCESS) return false;
    VkPipelineShaderStageCreateInfo ls[2]{};
    ls[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, lv, "main", nullptr}; ls[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, lf, "main", nullptr};
    VkPipelineVertexInputStateCreateInfo noVin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo lrs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; lrs.cullMode = VK_CULL_MODE_NONE; lrs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; lrs.polygonMode = VK_POLYGON_MODE_FILL; lrs.lineWidth = 1.0f;
    VkPipelineDepthStencilStateCreateInfo lds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState lb{}; lb.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo lcb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; lcb.attachmentCount = 1; lcb.pAttachments = &lb;
    VkGraphicsPipelineCreateInfo li{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; li.stageCount = 2; li.pStages = ls; li.pVertexInputState = &noVin; li.pInputAssemblyState = &ia; li.pViewportState = &vp; li.pRasterizationState = &lrs; li.pMultisampleState = &ms; li.pDepthStencilState = &lds; li.pColorBlendState = &lcb; li.pDynamicState = &dynamic; li.layout = m_lightingLayout; li.renderPass = m_gBuffer.lightingRenderPass;
    bool ok = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &li, nullptr, &m_lightingPipeline) == VK_SUCCESS;
    vkDestroyShaderModule(m_device, gv, nullptr); vkDestroyShaderModule(m_device, gf, nullptr); vkDestroyShaderModule(m_device, lv, nullptr); vkDestroyShaderModule(m_device, lf, nullptr);
    return ok;
}

void VulkanDeferredRenderer::CreatePrimitives() {
    const MeshPrimitive kinds[kPrimitiveCount] = {MeshPrimitive::Cube, MeshPrimitive::Plane, MeshPrimitive::Sphere, MeshPrimitive::Cylinder};
    for (u32 i = 0; i < kPrimitiveCount; ++i) {
        MeshData data = BuildPrimitive(kinds[i]);
        m_primitives[i].vertexBuffer = CreateBuffer(m_physicalDevice, m_device, sizeof(MeshVertex) * data.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
        std::memcpy(m_primitives[i].vertexBuffer.mapped, data.vertices.data(), sizeof(MeshVertex) * data.vertices.size());
        m_primitives[i].indexBuffer = CreateBuffer(m_physicalDevice, m_device, sizeof(u32) * data.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
        std::memcpy(m_primitives[i].indexBuffer.mapped, data.indices.data(), sizeof(u32) * data.indices.size());
        m_primitives[i].indexCount = static_cast<u32>(data.indices.size());
    }
}

void VulkanDeferredRenderer::CleanupPrimitives() {
    for (Primitive& p : m_primitives) { DestroyBuffer(m_device, p.vertexBuffer); DestroyBuffer(m_device, p.indexBuffer); p.indexCount = 0; }
}

void VulkanDeferredRenderer::CleanupCustomMeshes() {
    // Clean up custom mesh cache
    for (auto& [key, mesh] : m_customMeshes) {
        if (mesh.valid) {
            DestroyBuffer(m_device, mesh.vertexBuffer);
            DestroyBuffer(m_device, mesh.indexBuffer);
        }
    }
    m_customMeshes.clear();
}

const VulkanDeferredRenderer::CustomMesh* VulkanDeferredRenderer::GetOrCreateCustomMesh(
    const Math::Vec3* vertices, u32 vertexCount, const u32* indices, u32 indexCount,
    const Math::Vec3* normals, const Math::Vec2* uvs, const Math::Vec3* tangents) {
    (void)vertices;
    (void)vertexCount;
    (void)indices;
    (void)indexCount;
    (void)normals;
    (void)uvs;
    (void)tangents;
    
    // Simplified implementation - return nullptr for now
    return nullptr;
}

bool VulkanDeferredRenderer::CreateShadowResources() {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D; info.format = kShadowFormat;
    info.extent = {1, 1, 1}; info.mipLevels = 1; info.arrayLayers = kCascades;
    info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (vkCreateImage(m_device, &info, nullptr, &m_shadowImage) != VK_SUCCESS) return false;
    VkMemoryRequirements req{}; vkGetImageMemoryRequirements(m_device, m_shadowImage, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize = req.size; alloc.memoryTypeIndex = FindMemoryType(m_physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_device, &alloc, nullptr, &m_shadowMem) != VK_SUCCESS) return false;
    vkBindImageMemory(m_device, m_shadowImage, m_shadowMem, 0);
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image = m_shadowImage; view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; view.format = kShadowFormat; view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, kCascades};
    if (vkCreateImageView(m_device, &view, nullptr, &m_shadowArrayView) != VK_SUCCESS) return false;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}; sampler.magFilter = VK_FILTER_LINEAR; sampler.minFilter = VK_FILTER_LINEAR; sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    return vkCreateSampler(m_device, &sampler, nullptr, &m_shadowSampler) == VK_SUCCESS;
}

void VulkanDeferredRenderer::DestroyShadowResources() {
    if (m_shadowSampler) vkDestroySampler(m_device, m_shadowSampler, nullptr);
    if (m_shadowArrayView) vkDestroyImageView(m_device, m_shadowArrayView, nullptr);
    if (m_shadowImage) vkDestroyImage(m_device, m_shadowImage, nullptr);
    if (m_shadowMem) vkFreeMemory(m_device, m_shadowMem, nullptr);
    m_shadowSampler = VK_NULL_HANDLE; m_shadowArrayView = VK_NULL_HANDLE; m_shadowImage = VK_NULL_HANDLE; m_shadowMem = VK_NULL_HANDLE;
    m_shadowImageInitialized = false;
}

bool VulkanDeferredRenderer::CreateShadowPipeline(const std::string& shaderDir) {
    (void)shaderDir;
    // Create shadow mapping pipeline
    // Simplified for now
    return true;
}

void VulkanDeferredRenderer::SetViewportDimensions(int32_t width, int32_t height) {
    if (m_gBuffer.width != width || m_gBuffer.height != height) {
        LUMA_LOG_INFO("VulkanDeferredRenderer", "Resizing G-buffer from {}x{} to {}x{}", 
                     m_gBuffer.width, m_gBuffer.height, width, height);
        Cleanup();
        Initialize(width, height);
    }
}

void VulkanDeferredRenderer::PrepareScene() {
    // Prepare scene data for rendering
}

void VulkanDeferredRenderer::RenderScene(const Renderer2::DeferredSceneView& scene) {
    if (!m_initialized || !scene.sceneData) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Cannot render - not initialized");
        return;
    }
    vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, std::numeric_limits<u64>::max());
    vkResetFences(m_device, 1, &m_fence);
    vkResetCommandBuffer(m_commandBuffer, 0);

    const SceneView& source = *scene.sceneData;
    GbufferUBO camera{};
    camera.viewProj = scene.viewProjectionMatrix;
    camera.camPos[0] = scene.cameraPosition.x;
    camera.camPos[1] = scene.cameraPosition.y;
    camera.camPos[2] = scene.cameraPosition.z;
    std::memcpy(m_gbufferUBO.mapped, &camera, sizeof(camera));

    LightingUBO lighting{};
    lighting.invViewProj = Math::Inverse(scene.viewProjectionMatrix);
    SetVec3(lighting.camPos, scene.cameraPosition);
    SetVec3(lighting.camForward, scene.cameraDirection);
    const LightingParams fallback{};
    const LightingParams& params = scene.lightingParams ? *scene.lightingParams : fallback;
    SetVec3(lighting.sunDir, params.sunDirection);
    SetVec3(lighting.sunColor, params.sunColor, params.sunIntensity);
    SetVec3(lighting.ambientColor, params.skyHorizon, params.iblIntensity);
    FillAtmosphereParams(lighting.atmo, source.sky);
    lighting.params[0] = params.iblIntensity;
    lighting.params[1] = static_cast<f32>(std::min(source.lightCount, kMaxLights));
    lighting.params[2] = 0.0f; // Shadow resources are a valid placeholder until CSM is wired.
    lighting.params[3] = 1.0f / static_cast<f32>(kShadowSize);
    lighting.shadowParams[0] = params.shadowSoftness;
    lighting.shadowParams[1] = static_cast<f32>(kCascades);
    lighting.shadowParams[2] = params.shadowBias;
    for (u32 i = 0; i < std::min(source.lightCount, kMaxLights); ++i) {
        const SceneLight& src = source.lights[i];
        GpuLight& dst = lighting.lights[i];
        SetVec3(dst.posType, src.position, static_cast<f32>(src.type));
        SetVec3(dst.dirRange, Math::Normalize(src.direction), src.range);
        SetVec3(dst.color, src.color, src.intensity);
        dst.spot[0] = src.cosInner;
        dst.spot[1] = src.cosOuter;
    }
    std::memcpy(m_lightingUBO.mapped, &lighting, sizeof(lighting));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS) {
        LUMA_LOG_ERROR("VulkanDeferredRenderer", "Failed to begin command buffer");
        return;
    }
    auto barrier = [&](VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = oldLayout; b.newLayout = newLayout; b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.image = image; b.subresourceRange = {aspect, 0, 1, 0, 1}; b.srcAccessMask = srcAccess; b.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(m_commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    };
    {
        const VkImageLayout oldColorLayout = m_gBufferInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        const VkAccessFlags oldColorAccess = m_gBufferInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
        const VkPipelineStageFlags oldColorStage = m_gBufferInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        barrier(m_gBuffer.albedo.image, VK_IMAGE_ASPECT_COLOR_BIT, oldColorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, oldColorAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, oldColorStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        barrier(m_gBuffer.normal.image, VK_IMAGE_ASPECT_COLOR_BIT, oldColorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, oldColorAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, oldColorStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        barrier(m_gBuffer.material.image, VK_IMAGE_ASPECT_COLOR_BIT, oldColorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, oldColorAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, oldColorStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        barrier(m_gBuffer.lightAccum.image, VK_IMAGE_ASPECT_COLOR_BIT, oldColorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, oldColorAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, oldColorStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    barrier(m_gBuffer.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
            m_gBufferInitialized ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            m_gBufferInitialized ? VK_ACCESS_SHADER_READ_BIT : 0,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            m_gBufferInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    VkClearValue clears[5]{}; clears[0].color = {{0, 0, 0, 1}}; clears[1].color = {{0.5f, 0.5f, 1, 1}}; clears[2].color = {{0, 0.5f, 1, 1}}; clears[3].color = {{0, 0, 0, 1}}; clears[4].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rp.renderPass = m_gBuffer.renderPass; rp.framebuffer = m_gBuffer.framebuffer; rp.renderArea.extent = {static_cast<u32>(m_gBuffer.width), static_cast<u32>(m_gBuffer.height)}; rp.clearValueCount = 5; rp.pClearValues = clears;
    vkCmdBeginRenderPass(m_commandBuffer, &rp, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{0, 0, static_cast<f32>(m_gBuffer.width), static_cast<f32>(m_gBuffer.height), 0, 1}; VkRect2D scissor{{0, 0}, {static_cast<u32>(m_gBuffer.width), static_cast<u32>(m_gBuffer.height)}}; vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport); vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufferPipeline); vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufferLayout, 0, 1, &m_gbufferSet, 0, nullptr);
    VkDeviceSize offset = 0;
    for (u32 i = 0; i < source.instanceCount; ++i) { const SceneInstance& inst = source.instances[i]; u32 prim = static_cast<u32>(inst.primitive); if (prim >= kPrimitiveCount) prim = 0; MeshPush push{}; push.model = inst.model; push.albedo[0] = inst.albedo.x; push.albedo[1] = inst.albedo.y; push.albedo[2] = inst.albedo.z; push.albedo[3] = inst.metallic; push.material[0] = inst.roughness; push.texIdx[0] = push.texIdx[1] = push.texIdx[2] = push.texIdx[3] = -1; vkCmdPushConstants(m_commandBuffer, m_gbufferLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push); const Primitive& mesh = m_primitives[prim]; vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset); vkCmdBindIndexBuffer(m_commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32); vkCmdDrawIndexed(m_commandBuffer, mesh.indexCount, 1, 0, 0, 0); }
    vkCmdEndRenderPass(m_commandBuffer);
    barrier(m_gBuffer.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    barrier(m_gBuffer.lightAccum.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    if (!m_shadowImageInitialized) {
        VkImageMemoryBarrier shadowBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        shadowBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shadowBarrier.image = m_shadowImage;
        shadowBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, kCascades};
        shadowBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &shadowBarrier);
        m_shadowImageInitialized = true;
    }
    VkClearValue lightClear{}; lightClear.color = {{0, 0, 0, 1}}; VkRenderPassBeginInfo lightRp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; lightRp.renderPass = m_gBuffer.lightingRenderPass; lightRp.framebuffer = m_gBuffer.lightingFramebuffer; lightRp.renderArea.extent = {static_cast<u32>(m_gBuffer.width), static_cast<u32>(m_gBuffer.height)}; lightRp.clearValueCount = 1; lightRp.pClearValues = &lightClear; vkCmdBeginRenderPass(m_commandBuffer, &lightRp, VK_SUBPASS_CONTENTS_INLINE); vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport); vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor); vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline); vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingLayout, 0, 1, &m_lightingSet, 0, nullptr); vkCmdDraw(m_commandBuffer, 3, 1, 0, 0); vkCmdEndRenderPass(m_commandBuffer);
    if (m_gridPass && source.grid.enabled) {
        barrier(m_gBuffer.lightAccum.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        barrier(m_gBuffer.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

        VkRenderingAttachmentInfo gridColor{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        gridColor.imageView = m_gBuffer.lightAccum.view;
        gridColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        gridColor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        gridColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingAttachmentInfo gridDepth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        gridDepth.imageView = m_gBuffer.depth.view;
        gridDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        gridDepth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        gridDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo gridRendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        gridRendering.renderArea.extent = {static_cast<u32>(m_gBuffer.width), static_cast<u32>(m_gBuffer.height)};
        gridRendering.layerCount = 1;
        gridRendering.colorAttachmentCount = 1;
        gridRendering.pColorAttachments = &gridColor;
        gridRendering.pDepthAttachment = &gridDepth;
        vkCmdBeginRendering(m_commandBuffer, &gridRendering);
        vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
        m_gridPass->Record(m_commandBuffer, source.grid, scene.viewMatrix,
                           scene.viewProjectionMatrix);
        vkCmdEndRendering(m_commandBuffer);
        barrier(m_gBuffer.lightAccum.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        barrier(m_gBuffer.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) return;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &m_commandBuffer; vkQueueSubmit(m_graphicsQueue, 1, &submit, m_fence);
    m_gBufferInitialized = true;
}

const VulkanDeferredRenderer::Attachment* VulkanDeferredRenderer::GetLightAccumulationBuffer() const {
    return &m_gBuffer.lightAccum;
}

i32 VulkanDeferredRenderer::UploadMaterialTexture(const std::string& key, u32 width, u32 height,
                                                  const void* rgba8Pixels) {
    (void)key;
    (void)width;
    (void)height;
    (void)rgba8Pixels;
    // Simplified implementation - return -1 for now
    return -1;
}

} // namespace Rendering
} // namespace Luma
