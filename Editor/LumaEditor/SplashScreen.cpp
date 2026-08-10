#include "SplashScreen.h"

namespace Luma {
using Slate::Align;
using Slate::Color;
using Slate::Rect;

void SplashScreen::Draw(Slate::Context& ui, f32 width, f32 height, f32 progress,
                        std::string_view message) {
    Slate::Theme& t = ui.theme();
    ui.Panel({0, 0, width, height}, Color::RGB(16, 18, 22));

    // Wordmark centered.
    ui.LabelIn({0, height * 0.5f - 90, width, 60}, "LUMA", t.accentText,
               Align::Center, true);
    ui.LabelIn({0, height * 0.5f - 40, width, 28}, "ENGINE", t.textDim,
               Align::Center);

    // Progress bar.
    f32 barW = width * 0.5f;
    Rect track{(width - barW) * 0.5f, height * 0.5f + 40, barW, 6};
    ui.Panel(track, Color::RGB(38, 42, 50));
    f32 p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    ui.Panel({track.x, track.y, track.w * p, track.h}, t.accent);

    ui.LabelIn({0, track.y + 16, width, 24}, message, t.textDim, Align::Center);
}

}  // namespace Luma
