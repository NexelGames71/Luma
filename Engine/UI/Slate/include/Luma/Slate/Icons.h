#pragma once

#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Procedural icon set drawn with lines, tris, and circles (token-styled, DPI-
// crisp, no PNGs). Used everywhere we need a glyph (toolbars, panels, content
// browser). Adding a new icon is a single enum + draw block in Icons.cpp.

namespace Luma::Slate {

enum class Icon {
    None,
    ChevronRight,
    ChevronDown,
    ChevronUp,
    Search,
    Gear,
    Folder,
    FolderOpen,
    Eye,
    EyeOff,
    Lock,
    Plus,
    Close,
    Check,
    Save,
    Play,
    Pause,
    Stop,
    Grip,
    Dot,
    Trash,
    Camera,
    Light,
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Refresh,
};

class Context;

// Draws `icon` centered inside `box`, tinted with `color`. Uses theme tokens
// for any multi-tone pieces (e.g. a chevron's stem vs. arrowhead). Honors
// the context's dpiScale so icons stay crisp.
void DrawIcon(Context& ctx, const Rect& box, Icon icon,
              Color color = Color::RGB(255, 255, 255));

}  // namespace Luma::Slate
