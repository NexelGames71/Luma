#pragma once

#include <string>
#include <vector>

#include "Luma/RHI/Renderer.h"
#include "Luma/Core/Types.h"
#include "Luma/Slate/Types.h"

namespace Luma::Slate {

// A GPU texture loaded from an image file, with its pixel dimensions and the
// tight UV bounds of its non-transparent content (so padded logos crop nicely).
struct Image {
    TextureHandle texture = 0;
    u32 width = 0;
    u32 height = 0;
    Rect contentUV{0.0f, 0.0f, 1.0f, 1.0f};
    bool Valid() const { return texture != 0; }
    f32 Aspect() const {
        return height ? static_cast<f32>(width) / static_cast<f32>(height)
                      : 1.0f;
    }
    // Aspect ratio of just the non-transparent content.
    f32 ContentAspect() const {
        f32 cw = contentUV.w * static_cast<f32>(width);
        f32 ch = contentUV.h * static_cast<f32>(height);
        return ch > 0.0f ? cw / ch : 1.0f;
    }
};

// Loads a PNG/JPG/etc. (via stb_image) and uploads it as an RGBA8 texture.
Image LoadImage(Renderer& renderer, const std::string& path);

// Loads raw RGBA8 pixels from an image file (for e.g. window icons). Returns
// false on failure; on success `outPixels` has width*height*4 bytes.
bool LoadImagePixels(const std::string& path, u32& outWidth, u32& outHeight,
                     std::vector<u8>& outPixels);

}  // namespace Luma::Slate
