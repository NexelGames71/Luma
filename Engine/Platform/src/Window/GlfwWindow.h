#pragma once

#include <string>

#include "Luma/Platform/Window.h"

// Forward declaration only — the GLFW header is included solely in the .cpp so
// no GLFW type leaks into any public (or engine-facing) header.
struct GLFWwindow;
struct GLFWcursor;

namespace Luma {

class GlfwWindow final : public Window {
public:
    explicit GlfwWindow(const WindowProps& props);
    ~GlfwWindow() override;

    void PollEvents() override;
    bool ShouldClose() const override;

    void SetEventCallback(EventCallback callback) override {
        m_data.callback = std::move(callback);
    }

    u32 Width() const override { return m_data.width; }
    u32 Height() const override { return m_data.height; }
    void SetIcon(u32 width, u32 height, const void* rgba8Pixels) override;
    void SetCursor(CursorShape shape) override;
    void* NativeHandle() const override;

    std::vector<const char*> RequiredVulkanInstanceExtensions() const override;
    void* CreateVulkanSurface(void* instance) const override;

    // Stored behind the GLFW user pointer so static callbacks can reach it.
    // Public because the (file-local) GLFW callbacks retrieve it via the user
    // pointer; it carries no invariants worth hiding.
    struct WindowData {
        std::string title;
        u32 width = 0;
        u32 height = 0;
        EventCallback callback;
    };

private:
    void InstallCallbacks();

    GLFWwindow* m_window = nullptr;
    WindowData m_data;

    GLFWcursor* m_cursors[5] = {};  // indexed by CursorShape
    CursorShape m_currentCursor = CursorShape::Arrow;
};

}  // namespace Luma
