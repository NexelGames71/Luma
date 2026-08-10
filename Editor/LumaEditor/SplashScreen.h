#pragma once

#include <string_view>

#include "Luma/Slate/Context.h"
#include "Luma/Slate/Image.h"

namespace Luma {

// Boot/loading splash drawn while the editor initializes. `progress` is 0..1.
class SplashScreen {
public:
    void Draw(Slate::Context& ui, f32 width, f32 height, f32 progress,
              std::string_view message, const Slate::Image& logo);
};

}  // namespace Luma
