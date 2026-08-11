#pragma once

namespace Luma {

// Standard mouse cursor shapes the window can display.
enum class CursorShape {
    Arrow,
    ResizeEW,  // horizontal resize (vertical splitter)
    ResizeNS,  // vertical resize (horizontal splitter)
    Hand,      // clickable
    IBeam,     // text field
};

}  // namespace Luma
