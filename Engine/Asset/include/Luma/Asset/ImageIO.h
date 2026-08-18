#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Luma/Core/Types.h"

// Small public image-loading helper. Keeps stb_image confined to the Asset
// module: consumers (editor, MaterialEditor preview) ask for RGBA8 pixels
// without depending on the underlying decoder.

namespace Luma {

// Loads an image file (png/jpg/jpeg/tga/bmp/webp/ktx2 via stb_image) into
// tightly packed RGBA8 pixels, row 0 = top. Returns false on failure.
bool LoadImageRGBA(const std::filesystem::path& path, u32& outWidth,
                   u32& outHeight, std::vector<u8>& outPixels);

}  // namespace Luma
