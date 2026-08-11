#pragma once

#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

// Luma::Mesh - generates geometry for the built-in primitive shapes as
// position+normal vertices and 32-bit indices. Pure CPU geometry; the renderer
// uploads it. Real mesh-asset import plugs in alongside this later.

namespace Luma {

struct MeshVertex {
    Math::Vec3 position;
    Math::Vec3 normal;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
};

// Builds a unit-sized primitive centered at the origin (cube/sphere/cylinder
// ~1 unit, plane 1x1 on the XZ plane).
MeshData BuildPrimitive(MeshPrimitive primitive);

}  // namespace Luma
