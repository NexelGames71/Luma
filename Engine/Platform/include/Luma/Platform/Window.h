#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/Event.h"
#include "Luma/Core/Types.h"
#include "Luma/Platform/Cursor.h"

// Abstract window/OS surface. The concrete backend (GLFW today) is created
// through Window::Create and never appears in this header, so the rest of the
// engine depends only on the interface. The window produces Luma events and
// forwards them to the callback set by the application.

namespace Luma {

struct WindowProps {
    std::string title = "Luma Engine";
    u32 width = 1280;
    u32 height = 720;
};

class Window {
public:
    using EventCallback = std::function<void(Event&)>;

    virtual ~Window() = default;

    virtual void PollEvents() = 0;
    virtual bool ShouldClose() const = 0;

    virtual void SetEventCallback(EventCallback callback) = 0;

    virtual u32 Width() const = 0;
    virtual u32 Height() const = 0;

    // Per-monitor content scale (DPI factor). 1.0 on standard 96 DPI displays;
    // ~1.5 / 2.0 / 2.5 on hi-DPI displays. The Slate UI multiplies its design
    // sizes by this value so the product stays crisp on any monitor. Returns
    // 1.0 if the backend can't query it (e.g. headless / unknown platform).
    virtual f32 ContentScale() const = 0;

    // Sets the window/taskbar icon from tightly-packed RGBA8 pixels.
    virtual void SetIcon(u32 width, u32 height, const void* rgba8Pixels) = 0;

    // Sets the mouse cursor shape.
    virtual void SetCursor(CursorShape shape) = 0;

    // Native OS handle (GLFWwindow* today) for interop; nullptr if unavailable.
    virtual void* NativeHandle() const = 0;

    // --- Vulkan surface seam ------------------------------------------------
    // Vulkan types are passed as opaque handles (void*) so no Vulkan header is
    // required by consumers of this interface.

    // Instance extensions the windowing system needs (e.g. VK_KHR_surface).
    virtual std::vector<const char*> RequiredVulkanInstanceExtensions() const = 0;

    // Creates a VkSurfaceKHR for the given VkInstance. `instance` is a VkInstance
    // and the return value is a VkSurfaceKHR, both cast through void*; returns
    // nullptr on failure.
    virtual void* CreateVulkanSurface(void* instance) const = 0;

    // Creates the platform's default window implementation.
    static std::unique_ptr<Window> Create(const WindowProps& props = {});
};

}  // namespace Luma
