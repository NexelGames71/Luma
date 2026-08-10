#include "Luma/Slate/Font.h"

#include <fstream>
#include <vector>

#include "Luma/Core/Log.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace Luma::Slate {

bool Font::LoadFromFile(Renderer& renderer, const std::string& ttfPath,
                        f32 pixelHeight) {
    std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
    if (!file) {
        LUMA_LOG_ERROR("Slate", "font not found: {}", ttfPath);
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0);
    std::vector<u8> ttf(static_cast<usize>(size));
    file.read(reinterpret_cast<char*>(ttf.data()), size);

    m_pixelHeight = pixelHeight;
    m_atlasWidth = 512;
    m_atlasHeight = 512;

    std::vector<u8> alpha(static_cast<usize>(m_atlasWidth) * m_atlasHeight, 0);
    std::vector<stbtt_bakedchar> baked(kCharCount);
    int result = stbtt_BakeFontBitmap(
        ttf.data(), 0, pixelHeight, alpha.data(),
        static_cast<int>(m_atlasWidth), static_cast<int>(m_atlasHeight),
        kFirstChar, kCharCount, baked.data());
    if (result == 0) {
        LUMA_LOG_ERROR("Slate", "failed to bake font atlas: {}", ttfPath);
        return false;
    }

    m_chars.resize(kCharCount);
    for (int i = 0; i < kCharCount; ++i) {
        m_chars[i] = BakedChar{baked[i].x0, baked[i].y0, baked[i].x1,
                               baked[i].y1, baked[i].xoff, baked[i].yoff,
                               baked[i].xadvance};
    }

    // Vertical metrics for baseline placement and line height.
    stbtt_fontinfo info{};
    stbtt_InitFont(&info, ttf.data(),
                   stbtt_GetFontOffsetForIndex(ttf.data(), 0));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    m_ascent = ascent * scale;
    m_lineHeight = (ascent - descent + lineGap) * scale;

    // Expand single-channel coverage to RGBA8 (white, alpha = coverage).
    std::vector<u32> rgba(static_cast<usize>(m_atlasWidth) * m_atlasHeight);
    for (usize i = 0; i < rgba.size(); ++i) {
        u32 cov = alpha[i];
        rgba[i] = 0x00FFFFFFu | (cov << 24);
    }
    m_atlas = renderer.CreateTexture(m_atlasWidth, m_atlasHeight, rgba.data());
    LUMA_LOG_INFO("Slate", "font '{}' baked @ {}px (atlas {}x{})", ttfPath,
                  pixelHeight, m_atlasWidth, m_atlasHeight);
    return m_atlas != 0;
}

Vec2 Font::Measure(std::string_view text) const {
    f32 width = 0.0f;
    for (char c : text) {
        int idx = static_cast<int>(c) - kFirstChar;
        if (idx < 0 || idx >= kCharCount) continue;
        width += m_chars[static_cast<usize>(idx)].xadvance;
    }
    return Vec2{width, m_lineHeight};
}

void Font::Layout(
    Vec2 pos, std::string_view text,
    const std::function<void(const Rect&, const Rect&)>& emit) const {
    f32 penX = pos.x;
    f32 baseline = pos.y + m_ascent;
    const f32 invW = 1.0f / static_cast<f32>(m_atlasWidth);
    const f32 invH = 1.0f / static_cast<f32>(m_atlasHeight);

    for (char c : text) {
        int idx = static_cast<int>(c) - kFirstChar;
        if (idx < 0 || idx >= kCharCount) {
            continue;
        }
        const BakedChar& bc = m_chars[static_cast<usize>(idx)];
        f32 x0 = penX + bc.xoff;
        f32 y0 = baseline + bc.yoff;
        f32 w = static_cast<f32>(bc.x1 - bc.x0);
        f32 h = static_cast<f32>(bc.y1 - bc.y0);

        Rect dst{x0, y0, w, h};
        Rect uv{bc.x0 * invW, bc.y0 * invH, (bc.x1 - bc.x0) * invW,
                (bc.y1 - bc.y0) * invH};
        emit(dst, uv);
        penX += bc.xadvance;
    }
}

}  // namespace Luma::Slate
