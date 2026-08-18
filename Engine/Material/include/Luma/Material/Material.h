#pragma once

#include <array>
#include <string>
#include <vector>

#include "Luma/Asset/AssetId.h"
#include "Luma/Math/Math.h"
#include "Luma/Material/MaterialTypes.h"

// The Material asset — flat PBR properties only.  Node graph removed;
// to be rebuilt later.

namespace Luma::Material {

enum class BlendMode : u8 {
    Opaque = 0,
    Masked,
    Translucent,
};

struct Material {
    std::string name;

    BlendMode blendMode = BlendMode::Opaque;
    f32 opacityThreshold = 0.5f;

    // --- Texture map slots (classic PBR workflow) ---
    AssetId baseColorMap;
    AssetId normalMap;
    AssetId roughnessMap;
    AssetId metallicMap;

    // --- Property values, indexed by MaterialProperty ---
    std::array<Math::Vec4, static_cast<usize>(MaterialProperty::Count)>
        propertyValues{};

    // Constant fallback for a property.
    f32& ScalarValue(MaterialProperty p) {
        return propertyValues[static_cast<usize>(p)].x;
    }
    f32 ScalarValue(MaterialProperty p) const {
        return propertyValues[static_cast<usize>(p)].x;
    }
    void SetScalarValue(MaterialProperty p, f32 v) {
        propertyValues[static_cast<usize>(p)].x = v;
    }
    Math::Vec3 VectorValue(MaterialProperty p) const {
        const Math::Vec4& v = propertyValues[static_cast<usize>(p)];
        return {v.x, v.y, v.z};
    }
    void SetVectorValue(MaterialProperty p, const Math::Vec3& v) {
        Math::Vec4& d = propertyValues[static_cast<usize>(p)];
        d.x = v.x;
        d.y = v.y;
        d.z = v.z;
    }
};

}  // namespace Luma::Material
