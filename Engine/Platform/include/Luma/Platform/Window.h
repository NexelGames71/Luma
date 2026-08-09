#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Luma/Core/Event.h"
#include "Luma/Core/Types.h"

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

    // Native OS handle (HWND on Windows) for interop; nullptr if unavailable.
    virtual void* NativeHandle() const = 0;

    // Creates the platform's default window implementation.
    static std::unique_ptr<Window> Create(const WindowProps& props = {});
};

}  // namespace Luma
