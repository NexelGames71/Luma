#pragma once

// Procedural-icon enum. Lives in its own header so it can be included without
// pulling in Context.h (which would otherwise create a circular include when
// Context.h exposes Icon in its member signatures).

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

}  // namespace Luma::Slate
