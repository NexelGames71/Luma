#include "Vulkan/VulkanRenderer.h"

#include <limits>

#include "Luma/Platform/Window.h"
#include "Luma/RHI/VulkanRenderer.h"

namespace Luma {
namespace {

// Records a color-image layout transition into `cmd`.
void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
                     VkImageLayout newLayout, VkAccessFlags srcAccess,
                     VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
                     VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

}  // namespace

std::unique_ptr<Renderer> CreateVulkanRenderer(Window& window,
                                               const RendererConfig& config) {
    return std::make_unique<VulkanRenderer>(window, config);
}

VulkanRenderer::VulkanRenderer(Window& window, const RendererConfig& config)
    : m_window(window), m_config(config) {
    bool wantValidation = config.enableValidation;
#if defined(LUMA_CONFIG_SHIPPING)
    wantValidation = false;
#endif

    m_instance = std::make_unique<VulkanInstance>(
        config.appName, window.RequiredVulkanInstanceExtensions(),
        wantValidation);

    m_surface = reinterpret_cast<VkSurfaceKHR>(
        window.CreateVulkanSurface(reinterpret_cast<void*>(m_instance->Handle())));
    LUMA_ASSERT(m_surface != VK_NULL_HANDLE, "failed to create Vulkan surface");

    m_device = std::make_unique<VulkanDevice>(m_instance->Handle(), m_surface);
    m_swapchain = std::make_unique<VulkanSwapchain>(
        *m_device, m_surface, window.Width(), window.Height(), config.vsync);

    CreateCommandResources();
    CreateFrameSync();
    CreateRenderFinishedSemaphores();

#if defined(LUMA_SHADER_DIR)
    const std::string shaderDir = LUMA_SHADER_DIR;
    m_trianglePipeline = std::make_unique<VulkanPipeline>(
        m_device->Logical(), m_swapchain->Format(),
        shaderDir + "/triangle.vert.spv", shaderDir + "/triangle.frag.spv");
#endif

    LUMA_LOG_INFO("Vulkan", "renderer ready");
}

VulkanRenderer::~VulkanRenderer() {
    if (m_device) vkDeviceWaitIdle(m_device->Logical());
    VkDevice device = m_device ? m_device->Logical() : VK_NULL_HANDLE;

    m_trianglePipeline.reset();  // uses the device; destroy before it
    DestroyRenderFinishedSemaphores();
    for (auto& sem : m_imageAvailable) {
        if (sem) vkDestroySemaphore(device, sem, nullptr);
    }
    for (auto& fence : m_inFlight) {
        if (fence) vkDestroyFence(device, fence, nullptr);
    }
    if (m_commandPool) vkDestroyCommandPool(device, m_commandPool, nullptr);

    m_swapchain.reset();
    if (m_surface) vkDestroySurfaceKHR(m_instance->Handle(), m_surface, nullptr);
    m_device.reset();
    m_instance.reset();
}

void VulkanRenderer::CreateCommandResources() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = *m_device->Queues().graphics;
    VK_CHECK(vkCreateCommandPool(m_device->Logical(), &poolInfo, nullptr,
                                 &m_commandPool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kFramesInFlight;
    VK_CHECK(vkAllocateCommandBuffers(m_device->Logical(), &allocInfo,
                                      m_commandBuffers.data()));
}

void VulkanRenderer::CreateFrameSync() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        VK_CHECK(vkCreateSemaphore(m_device->Logical(), &semInfo, nullptr,
                                   &m_imageAvailable[i]));
        VK_CHECK(vkCreateFence(m_device->Logical(), &fenceInfo, nullptr,
                               &m_inFlight[i]));
    }
}

void VulkanRenderer::CreateRenderFinishedSemaphores() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_renderFinished.resize(m_swapchain->ImageCount());
    for (auto& sem : m_renderFinished) {
        VK_CHECK(vkCreateSemaphore(m_device->Logical(), &semInfo, nullptr,
                                   &sem));
    }
}

void VulkanRenderer::DestroyRenderFinishedSemaphores() {
    for (auto& sem : m_renderFinished) {
        if (sem) vkDestroySemaphore(m_device->Logical(), sem, nullptr);
    }
    m_renderFinished.clear();
}

void VulkanRenderer::OnResize(u32 width, u32 height) {
    (void)width;
    (void)height;
    m_resizePending = true;
}

void VulkanRenderer::RecreateSwapchain() {
    u32 width = m_window.Width();
    u32 height = m_window.Height();
    if (width == 0 || height == 0) return;  // minimized; try again later

    vkDeviceWaitIdle(m_device->Logical());
    m_swapchain->Recreate(width, height);
    DestroyRenderFinishedSemaphores();
    CreateRenderFinishedSemaphores();
    m_resizePending = false;
}

bool VulkanRenderer::AcquireOrRecreate() {
    if (m_resizePending) RecreateSwapchain();
    if (m_window.Width() == 0 || m_window.Height() == 0) return false;

    VkResult result = vkAcquireNextImageKHR(
        m_device->Logical(), m_swapchain->Handle(),
        std::numeric_limits<u64>::max(), m_imageAvailable[m_frame],
        VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LUMA_LOG_ERROR("Vulkan", "acquire failed: {}", VkResultString(result));
        return false;
    }
    return true;
}

bool VulkanRenderer::BeginFrame() {
    vkWaitForFences(m_device->Logical(), 1, &m_inFlight[m_frame], VK_TRUE,
                    std::numeric_limits<u64>::max());

    if (!AcquireOrRecreate()) return false;

    vkResetFences(m_device->Logical(), 1, &m_inFlight[m_frame]);

    VkCommandBuffer cmd = m_commandBuffers[m_frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    VkImage image = m_swapchain->Image(m_imageIndex);
    TransitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clear{};
    clear.color = {{m_clearColor.r, m_clearColor.g, m_clearColor.b,
                    m_clearColor.a}};

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_swapchain->ImageView(m_imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clear;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.offset = {0, 0};
    rendering.renderArea.extent = m_swapchain->Extent();
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &rendering);
    if (m_trianglePipeline) {
        VkExtent2D extent = m_swapchain->Extent();
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<f32>(extent.width);
        viewport.height = static_cast<f32>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_trianglePipeline->Handle());
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    vkCmdEndRendering(cmd);

    TransitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd));
    m_frameActive = true;
    return true;
}

void VulkanRenderer::EndFrame() {
    if (!m_frameActive) return;
    m_frameActive = false;

    VkSemaphore waitSem = m_imageAvailable[m_frame];
    VkSemaphore signalSem = m_renderFinished[m_imageIndex];
    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &waitSem;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffers[m_frame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signalSem;
    VK_CHECK(vkQueueSubmit(m_device->GraphicsQueue(), 1, &submit,
                           m_inFlight[m_frame]));

    VkSwapchainKHR swapchain = m_swapchain->Handle();
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &signalSem;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_device->PresentQueue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        m_resizePending) {
        RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        LUMA_LOG_ERROR("Vulkan", "present failed: {}", VkResultString(result));
    }

    m_frame = (m_frame + 1) % kFramesInFlight;
}

void VulkanRenderer::WaitIdle() {
    if (m_device) vkDeviceWaitIdle(m_device->Logical());
}

}  // namespace Luma
