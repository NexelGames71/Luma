#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Luma/Material/Material.h"

// Compiles a Material's graph into shader code — Luma's counterpart to UE5.8's
// FMaterialCompiler + HLSLMaterialTranslator. The compiler walks each material
// property input, follows the expression connections, and emits GLSL
// expressions plus the uniform/const declarations the emitted code needs.
//
// Phase 1 (this module) targets the deferred PBR fragment shader: each
// property compiles to a GLSL expression string that scene.frag can splice in.
// Phase 2 (documented, not built) lifts this into a real shader-builder that
// generates a complete fragment shader from a material.

namespace Luma::Material {

// A uniform the emitted code depends on (consts are inlined, textures become
// sampler uniforms, scalar constants that the editor marks as parameters
// become uniforms later).
struct MaterialUniform {
    std::string name;   // GLSL identifier, e.g. "uMat0"
    std::string type;   // "sampler2D", "vec3", "float"
};

// Output of a compile: one GLSL expression per material property (indexed by
// MaterialProperty), plus the declarations the expressions reference.
// `ok == false` carries a message.
struct MaterialCompileResult {
    bool ok = false;
    std::string error;

    // Per-property GLSL expressions, indexed by MaterialProperty.
    std::array<std::string, static_cast<usize>(MaterialProperty::Count)>
        property;

    // Convenience accessors for the renderer's current PBR inputs.
    const std::string& baseColor() const {
        return property[static_cast<usize>(MaterialProperty::BaseColor)];
    }
    const std::string& metallic() const {
        return property[static_cast<usize>(MaterialProperty::Metallic)];
    }
    const std::string& roughness() const {
        return property[static_cast<usize>(MaterialProperty::Roughness)];
    }
    const std::string& normal() const {
        return property[static_cast<usize>(MaterialProperty::Normal)];
    }
    const std::string& emissive() const {
        return property[static_cast<usize>(MaterialProperty::EmissiveColor)];
    }
    const std::string& opacity() const {
        return property[static_cast<usize>(MaterialProperty::Opacity)];
    }

    std::vector<MaterialUniform> uniforms;
};

class MaterialCompiler {
public:
    virtual ~MaterialCompiler() = default;

    // Compiles `material`'s property inputs into GLSL expressions.
    virtual MaterialCompileResult Compile(const Material& material) = 0;
};

// GLSL emitter for the deferred PBR fragment stage. Real for the node set in
// ExpressionKind; unsupported kinds produce a clear error (no silent fakes).
std::unique_ptr<MaterialCompiler> CreateGLSLMaterialCompiler();

}  // namespace Luma::Material
