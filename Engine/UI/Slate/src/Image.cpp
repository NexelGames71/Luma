#include "Luma/Slate/Image.h"

#include "Luma/Core/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

namespace Luma::Slate {

bool LoadImagePixels(const std::string& path, u32& outWidth, u32& outHeight,
                     std::vector<u8>& outPixels) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        LUMA_LOG_ERROR("Slate", "failed to load image: {}", path);
        return false;
    }
    outWidth = static_cast<u32>(w);
    outHeight = static_cast<u32>(h);
    outPixels.assign(data, data + (static_cast<usize>(w) * h * 4));
    stbi_image_free(data);
    return true;
}

Image LoadImage(Renderer& renderer, const std::string& path) {
    Image image;
    std::vector<u8> pixels;
    if (!LoadImagePixels(path, image.width, image.height, pixels)) {
        return image;
    }

    // Compute the tight bounding box of non-transparent pixels for nice cropping.
    u32 minX = image.width, minY = image.height, maxX = 0, maxY = 0;
    bool any = false;
    for (u32 y = 0; y < image.height; ++y) {
        for (u32 x = 0; x < image.width; ++x) {
            if (pixels[(static_cast<usize>(y) * image.width + x) * 4 + 3] > 16) {
                any = true;
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (any) {
        image.contentUV = {static_cast<f32>(minX) / image.width,
                           static_cast<f32>(minY) / image.height,
                           static_cast<f32>(maxX - minX + 1) / image.width,
                           static_cast<f32>(maxY - minY + 1) / image.height};
    }

    image.texture = renderer.CreateTexture(image.width, image.height,
                                           pixels.data());
    LUMA_LOG_INFO("Slate", "loaded image '{}' ({}x{})", path, image.width,
                  image.height);
    return image;
}

}  // namespace Luma::Slate
