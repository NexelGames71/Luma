#pragma once

#include <string>

#include "Luma/Core/Types.h"

// Rendering Hardware Interface (RHI): the abstract surface the engine renders
// through. Concrete backends (VulkanRenderer today; D3D12/Metal later) implement
// this, keeping graphics-API types out of the rest of the engine.

namespace Luma {

struct ClearColor {
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 1.0f;
};

struct RendererConfig {
    std::string appName = "Luma";
    bool enableValidation = true;  // ignored in Shipping backends
    bool vsync = true;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    // Notify the backend the drawable surface changed size (0x0 = minimized).
    virtual void OnResize(u32 width, u32 height) = 0;

    virtual void SetClearColor(const ClearColor& color) = 0;

    // Acquire the next frame. Returns false if the frame should be skipped
    // (e.g. the surface is out of date / minimized); do not call EndFrame then.
    virtual bool BeginFrame() = 0;

    // Submit and present the frame started by BeginFrame.
    virtual void EndFrame() = 0;

    // Block until the device is idle (use before teardown).
    virtual void WaitIdle() = 0;
};

}  // namespace Luma
