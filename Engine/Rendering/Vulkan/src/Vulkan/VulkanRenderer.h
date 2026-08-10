#pragma once

#include <array>
#include <memory>
#include <vector>

#include "Luma/RHI/Renderer.h"
#include "Vulkan/Scene/VulkanSceneView.h"
#include "Vulkan/UI/VulkanUIPass.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanInstance.h"
#include "Vulkan/VulkanSwapchain.h"

namespace Luma {

class Window;

// Vulkan 1.3 backend: instance + device + swapchain, and a double-buffered
// render loop that clears the frame via dynamic rendering and presents it.
class VulkanRenderer final : public Renderer {
public:
    VulkanRenderer(Window& window, const RendererConfig& config);
    ~VulkanRenderer() override;

    void OnResize(u32 width, u32 height) override;
    void SetClearColor(const ClearColor& color) override { m_clearColor = color; }
    bool BeginFrame() override;
    void EndFrame() override;
    void DrawUI(const UIDrawData& data) override;
    TextureHandle CreateTexture(u32 width, u32 height,
                                const void* rgba8Pixels) override;
    void DestroyTexture(TextureHandle texture) override;
    void CaptureFrame(const std::string& pngPath) override {
        m_capturePath = pngPath;
    }
    TextureHandle RenderSceneView(u32 width, u32 height, f32 dt) override;
    void WaitIdle() override;

private:
    static constexpr u32 kFramesInFlight = 2;

    void CreateCommandResources();
    void CreateFrameSync();
    void CreateRenderFinishedSemaphores();
    void DestroyRenderFinishedSemaphores();
    void RecreateSwapchain();
    bool AcquireOrRecreate();

    Window& m_window;
    RendererConfig m_config;

    std::unique_ptr<VulkanInstance> m_instance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::unique_ptr<VulkanUIPass> m_uiPass;
    std::unique_ptr<VulkanSceneView> m_sceneView;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kFramesInFlight> m_commandBuffers{};
    std::array<VkSemaphore, kFramesInFlight> m_imageAvailable{};
    std::array<VkFence, kFramesInFlight> m_inFlight{};
    std::vector<VkSemaphore> m_renderFinished;  // one per swapchain image

    ClearColor m_clearColor{};
    u32 m_frame = 0;
    u32 m_imageIndex = 0;
    bool m_frameActive = false;
    bool m_resizePending = false;
    std::string m_capturePath;  // non-empty => capture this frame
};

}  // namespace Luma
