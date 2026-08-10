#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Types.h"

namespace Luma::Slate {

// A baked bitmap font. Rasterizes an ASCII atlas from a TrueType file via
// stb_truetype and uploads it as an RGBA8 texture (white with glyph coverage in
// alpha) through the RHI.
class Font {
public:
    // Loads `ttfPath` at `pixelHeight`, uploading the atlas via `renderer`.
    bool LoadFromFile(Renderer& renderer, const std::string& ttfPath,
                      f32 pixelHeight);

    bool Valid() const { return m_atlas != 0; }
    TextureHandle Atlas() const { return m_atlas; }
    f32 PixelHeight() const { return m_pixelHeight; }
    f32 LineHeight() const { return m_lineHeight; }
    f32 Ascent() const { return m_ascent; }

    // Total advance width and line height of a single-line string.
    Vec2 Measure(std::string_view text) const;

    // Emits (destRect, uvRect) for each glyph starting at top-left `pos`.
    // `emit` receives pixel-space dest rects and 0..1 atlas UV rects.
    void Layout(Vec2 pos, std::string_view text,
                const std::function<void(const Rect&, const Rect&)>& emit) const;

private:
    TextureHandle m_atlas = 0;
    u32 m_atlasWidth = 0;
    u32 m_atlasHeight = 0;
    f32 m_pixelHeight = 0.0f;
    f32 m_lineHeight = 0.0f;
    f32 m_ascent = 0.0f;

    // stbtt_bakedchar for ASCII 32..126, stored as raw floats/shorts.
    static constexpr int kFirstChar = 32;
    static constexpr int kCharCount = 95;
    struct BakedChar {
        u16 x0, y0, x1, y1;
        f32 xoff, yoff, xadvance;
    };
    std::vector<BakedChar> m_chars;
};

}  // namespace Luma::Slate
