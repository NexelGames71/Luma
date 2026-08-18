#include "Luma/Asset/ImageIO.h"

#include <stb_image.h>

#include "Luma/Core/Log.h"

namespace Luma {

bool LoadImageRGBA(const std::filesystem::path& path, u32& outWidth,
                   u32& outHeight, std::vector<u8>& outPixels) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) {
        LUMA_LOG_ERROR("ImageIO", "failed to load image '{}'",
                       path.string());
        return false;
    }
    outWidth = static_cast<u32>(w);
    outHeight = static_cast<u32>(h);
    outPixels.assign(data, data + static_cast<usize>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

}  // namespace Luma
