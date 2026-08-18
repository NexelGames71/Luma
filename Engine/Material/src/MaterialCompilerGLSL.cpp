#include "Luma/Material/MaterialCompiler.h"

#include <cstdio>
#include <memory>
#include <unordered_set>

namespace Luma::Material {

namespace {

// Emits `v` as a GLSL float literal with enough precision to round-trip.
std::string FloatLit(f32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(v));
    return buf;
}

std::string Vec2Lit(const Math::Vec2& v) {
    return "vec2(" + FloatLit(v.x) + ", " + FloatLit(v.y) + ")";
}

std::string Vec3Lit(const Math::Vec3& v) {
    return "vec3(" + FloatLit(v.x) + ", " + FloatLit(v.y) + ", " +
           FloatLit(v.z) + ")";
}

std::string Vec4Lit(const Math::Vec4& v) {
    return "vec4(" + FloatLit(v.x) + ", " + FloatLit(v.y) + ", " +
           FloatLit(v.z) + ", " + FloatLit(v.w) + ")";
}

class GLSLMaterialCompiler : public MaterialCompiler {
public:
    MaterialCompileResult Compile(const Material& material) override {
        m_material = &material;
        m_result = MaterialCompileResult{};
        m_result.ok = true;
        m_textureCounter = 0;
        m_hasTime = false;

        // Compile every property input off the shared MaterialInputs() table
        // (UE's FMaterialInputInfo loop in HLSLMaterialTranslator).
        for (const auto& info : MaterialInputs()) {
            const ExpressionInput& in = material.GetExpressionInput(
                info.property);
            m_result.property[static_cast<usize>(info.property)] =
                Property(in, MaterialPropertyValueType(info.property),
                         ConstantFallback(material, info.property));
        }
        return m_result;
    }

private:
    const Material* m_material = nullptr;
    MaterialCompileResult m_result;
    u32 m_textureCounter = 0;
    bool m_hasTime = false;

    void Fail(const std::string& msg) {
        if (m_result.ok) {
            m_result.ok = false;
            m_result.error = msg;
        }
    }

    // GLSL literal for a property's constant fallback.
    std::string ConstantFallback(const Material& m, MaterialProperty p) const {
        switch (MaterialPropertyValueType(p)) {
            case MCT_Float3:
                return Vec3Lit(m.VectorValue(p));
            case MCT_Float2:
                return Vec2Lit({m.ScalarValue(p), 0.0f});
            default:
                return FloatLit(m.ScalarValue(p));
        }
    }

    // Registers the uTime uniform once per compile (Panner / Rotator / Time).
    void EnsureTime() {
        if (!m_hasTime) {
            m_hasTime = true;
            m_result.uniforms.push_back({"uTime", "float"});
        }
    }

    // Compiles a material property input (the graph sink): the connected
    // expression's GLSL, or the constant fallback. Enforces/promotes types.
    std::string Property(const ExpressionInput& in, MaterialValueType expected,
                         const std::string& fallback) {
        if (!in.IsConnected()) return fallback;
        const MaterialExpression* src = m_material->graph.Find(in.expression);
        if (!src) {
            Fail("property input references a missing expression");
            return fallback;
        }
        m_path.clear();
        std::string e = CompileExpr(*src, in.outputIndex);
        if (!m_result.ok) return fallback;
        MaterialValueType outType = src->ResolveOutputType(m_material->graph,
                                                           in.outputIndex);
        if (outType == MCT_Unknown) outType = expected;
        if (outType != expected) {
            if (expected == MCT_Float3 && outType == MCT_Float1) {
                return "vec3(" + e + ")";  // splat scalar into vec3
            }
            if (expected == MCT_Float3 && outType == MCT_Float4) {
                return "(" + e + ").rgb";
            }
            if (expected == MCT_Float2 && outType == MCT_Float4) {
                return "(" + e + ").rg";
            }
            if (expected == MCT_Float2 && outType == MCT_Float1) {
                return "vec2(" + e + ")";
            }
            if (expected == MCT_Float1 && outType == MCT_Float4) {
                return "(" + e + ").r";
            }
            if (expected == MCT_Float1 && outType == MCT_Float3) {
                return "(" + e + ").r";
            }
            if (expected == MCT_Float1 && outType == MCT_Float2) {
                return "(" + e + ").r";
            }
            Fail("type mismatch: property expects a " +
                 std::string(ValueTypeName(expected)) +
                 " but the connected node outputs " +
                 std::string(ValueTypeName(outType)));
            return fallback;
        }
        return e;
    }

    // Inlines an input's connected expression (or a default literal).
    std::string Input(const MaterialExpression& node, i32 index,
                      const std::string& def) {
        const ExpressionInput* in = node.Input(index);
        if (!in || !in->IsConnected()) return def;
        const MaterialExpression* src = m_material->graph.Find(in->expression);
        if (!src) {
            Fail("input on '" + node.title + "' references a missing expression");
            return def;
        }
        return CompileExpr(*src, in->outputIndex);
    }

    // Emits a GLSL expression for a node's output pin, inlining inputs
    // recursively. Detects cycles via the current DFS path.
    std::string CompileExpr(const MaterialExpression& node, i32 outputIndex) {
        if (!m_result.ok) return "0.0";
        if (node.IsComment()) {
            Fail("comment node is not compilable");
            return "0.0";
        }
        if (!m_path.insert(node.id).second) {
            Fail("cycle detected in material graph at '" + node.title + "'");
            return "0.0";
        }
        std::string out = Emit(node, outputIndex);
        m_path.erase(node.id);
        return out;
    }

    std::string Emit(const MaterialExpression& node, i32 outputIndex) {
        switch (node.kind) {
            // --- Constants / parameters (params emit their defaults until the
            // shader-builder lifts them into real uniforms) ---
            case ExpressionKind::Boolean:
                return node.constBool ? "true" : "false";
            case ExpressionKind::AmbientOcclusion:
                // Ray-traced AO is a renderer feature that hasn't landed yet;
                // fail loudly instead of emitting a wrong value.
                Fail("ambient occlusion is not supported by the runtime "
                     "shader yet");
                return "1.0";
            case ExpressionKind::Integer:
                return FloatLit(static_cast<f32>(node.constInt));
            case ExpressionKind::Menu:
                // Menu values have no numeric meaning in a material.
                Fail("menu value cannot be used in a material");
                return "0.0";
            case ExpressionKind::Constant:
            case ExpressionKind::ScalarParameter:
                return FloatLit(node.constScalar);
            case ExpressionKind::Constant2:
                return Vec2Lit(node.constVec2);
            case ExpressionKind::Constant3:
            case ExpressionKind::VectorParameter:
                return Vec3Lit(node.constVec3);
            case ExpressionKind::Constant4:
                return Vec4Lit(node.constVec4);
            case ExpressionKind::Vector: {
                // Arity follows the node's Dimensions property.
                if (node.vectorDim <= 2) {
                    return Vec2Lit(
                        {node.vectorValue.x, node.vectorValue.y});
                }
                if (node.vectorDim == 3) {
                    return Vec3Lit({node.vectorValue.x, node.vectorValue.y,
                                    node.vectorValue.z});
                }
                return Vec4Lit(node.vectorValue);
            }

            // --- Coordinates ---
            case ExpressionKind::TextureCoordinate:
                // A single UV set is plumbed through the scene shader today;
                // the index is editor-facing until multi-UV lands.
                return "vUV";
            case ExpressionKind::Time: {
                EnsureTime();
                return "uTime";
            }
            case ExpressionKind::Panner: {
                EnsureTime();
                return "(" + Input(node, 0, "vUV") + " + vec2(" +
                       Input(node, 1, "0.0") + ", " + Input(node, 2, "0.0") +
                       ") * uTime)";
            }
            case ExpressionKind::Rotator: {
                EnsureTime();
                std::string uv = Input(node, 0, "vUV");
                std::string t = Input(node, 1, "uTime");
                std::string speed =
                    Input(node, 2, FloatLit(node.rotatorSpeed));
                std::string c = Vec2Lit(node.rotatorCenter);
                return "((" + uv + " - " + c + ") * mat2(cos(" + speed +
                       " * " + t + "), -sin(" + speed + " * " + t + "), sin(" +
                       speed + " * " + t + "), cos(" + speed + " * " + t +
                       ")) + " + c + ")";
            }

            // --- Texture ---
            case ExpressionKind::TextureSample: {
                std::string texName =
                    "uMatTex" + std::to_string(m_textureCounter++);
                m_result.uniforms.push_back({texName, "sampler2D"});
                std::string uv = Input(node, 0, "vUV");
                return "texture(" + texName + ", " + uv + ")";
            }

            // --- Math: binary ---
            case ExpressionKind::Add:
                return "(" + Input(node, 0, "0.0") + " + " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::Subtract:
                return "(" + Input(node, 0, "0.0") + " - " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::Multiply:
                return "(" + Input(node, 0, "1.0") + " * " +
                       Input(node, 1, "1.0") + ")";
            case ExpressionKind::Divide:
                return "(" + Input(node, 0, "1.0") + " / " +
                       Input(node, 1, "1.0") + ")";
            case ExpressionKind::Power:
                return "pow(" + Input(node, 0, "1.0") + ", " +
                       Input(node, 1, "1.0") + ")";
            case ExpressionKind::Min:
                return "min(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::Max:
                return "max(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::Fmod:
                return "mod(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "1.0") + ")";
            case ExpressionKind::Lerp: {
                std::string a = Input(node, 0, "0.0");
                std::string b = Input(node, 1, "0.0");
                const ExpressionInput* alpha = node.Input(2);
                std::string t = (alpha && alpha->IsConnected())
                                    ? Input(node, 2, "0.0")
                                    : FloatLit(node.lerpAlpha);
                return "mix(" + a + ", " + b + ", " + t + ")";
            }
            case ExpressionKind::InverseLerp: {
                std::string a = Input(node, 0, "0.0");
                std::string b = Input(node, 1, "1.0");
                std::string v = Input(node, 2, "0.0");
                return "((" + v + " - " + a + ") / (" + b + " - " + a + "))";
            }

            // --- Math: unary pass-through ---
            case ExpressionKind::SquareRoot:
                return "sqrt(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Abs:
                return "abs(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Sine:
                return "sin(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Cosine:
                return "cos(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Tangent:
                return "tan(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Arctangent:
                return "atan(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Arctangent2:
                return "atan(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::Floor:
                return "floor(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Ceil:
                return "ceil(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Round:
                return "round(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Truncate:
                return "trunc(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Frac:
                return "fract(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Sign:
                return "sign(" + Input(node, 0, "0.0") + ")";
            case ExpressionKind::OneMinus:
                return "(1.0 - " + Input(node, 0, "0.0") + ")";
            case ExpressionKind::Step:
                // UE: step(X = edge, Y = value) -> step(X, Y)
                return "step(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::SmoothStep:
                return "smoothstep(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "1.0") + ", " + Input(node, 2, "0.0") +
                       ")";

            // --- Math: vector ops ---
            case ExpressionKind::DotProduct:
                return "dot(" + Input(node, 0, "vec3(0.0)") + ", " +
                       Input(node, 1, "vec3(0.0)") + ")";
            case ExpressionKind::CrossProduct:
                return "cross(" + Input(node, 0, "vec3(0.0)") + ", " +
                       Input(node, 1, "vec3(0.0)") + ")";
            case ExpressionKind::Normalize:
                return "normalize(" + Input(node, 0, "vec3(0.0, 0.0, 1.0)") +
                       ")";
            case ExpressionKind::Length:
                return "length(" + Input(node, 0, "vec3(0.0)") + ")";
            case ExpressionKind::Distance:
                return "distance(" + Input(node, 0, "vec3(0.0)") + ", " +
                       Input(node, 1, "vec3(0.0)") + ")";
            case ExpressionKind::AppendVector:
                return "vec2(" + Input(node, 0, "0.0") + ", " +
                       Input(node, 1, "0.0") + ")";
            case ExpressionKind::ComponentMask: {
                std::string in = Input(node, 0, "vec4(0.0)");
                std::string r = "(" + in + ").r";
                std::string g = "(" + in + ").g";
                std::string b = "(" + in + ").b";
                std::string a = "(" + in + ").a";
                int n = (node.maskR ? 1 : 0) + (node.maskG ? 1 : 0) +
                        (node.maskB ? 1 : 0) + (node.maskA ? 1 : 0);
                if (n >= 4) return in;
                if (n == 3) {
                    return "vec3(" + (node.maskR ? r : "0.0") + ", " +
                           (node.maskG ? g : "0.0") + ", " +
                           (node.maskB ? b : "0.0") + ")";
                }
                if (n == 2) {
                    std::string first, second;
                    bool done = false;
                    if (node.maskR) { first = r; done = true; }
                    if (node.maskG) { if (!done) { first = g; done = true; } else second = g; }
                    if (node.maskB) { if (!done) { first = b; done = true; } else if (second.empty()) second = b; }
                    if (node.maskA) { if (!done) { first = a; done = true; } else if (second.empty()) second = a; }
                    return "vec2(" + first + ", " + second + ")";
                }
                // Single channel (or none -> R).
                return node.maskR ? r : node.maskG ? g : node.maskB ? b : a;
            }
            case ExpressionKind::BreakOutVectorComponents: {
                std::string in = Input(node, 0, "vec3(0.0)");
                switch (outputIndex) {
                    case 1: return "(" + in + ").g";
                    case 2: return "(" + in + ").b";
                    default: return "(" + in + ").r";
                }
            }

            // --- Math: lighting / utility ---
            case ExpressionKind::Fresnel: {
                // Schlick: F0 = ((1-IOR)/(1+IOR))^2, F = F0 + (1-F0)*(1-dot)^5.
                std::string ior = Input(node, 0, "1.45");
                std::string nrm = Input(node, 1, "normalize(vNormal)");
                std::string f0 = "pow((1.0 - " + ior +
                                 ") / (1.0 + " + ior + "), 2.0)";
                std::string v = "normalize(u.camPos.xyz - vWorldPos)";
                return "((" + f0 + ") + (1.0 - (" + f0 +
                       ")) * pow(1.0 - max(dot(normalize(" + nrm + "), " +
                       v + "), 0.0), 5.0))";
            }
            case ExpressionKind::ObjectInfo: {
                // Per-object instance data (fed by the renderer's push
                // constants + varyings; see scene.vert / material.frag).
                switch (outputIndex) {
                    case 0: return "vObjPos";            // Location
                    case 1: return "vObjColor.rgb";      // Color
                    case 2: return "vObjColor.a";        // Alpha
                    case 3: return "float(vObjIndex)";   // Object Index
                    case 4: return "float(vObjMatIndex)";// Material Index
                    default: return "vObjRandom";        // Random
                }
            }
            case ExpressionKind::PointInfo: {
                // Point-cloud point state (tail of the material pipeline's
                // push block; see material.frag's pt0/ptLoc members).
                switch (outputIndex) {
                    case 0: return "pc.ptLoc.xyz";  // Location
                    case 1: return "pc.pt0.x";      // Radius
                    default: return "pc.pt0.y";     // Random
                }
            }
            case ExpressionKind::ParticleInfo: {
                // Per-particle state (tail of the material pipeline's push
                // block; see material.frag's Push block).
                switch (outputIndex) {
                    case 0: return "pc.p0.x";          // Index
                    case 1: return "pc.p0.y";          // Random
                    case 2: return "pc.p0.z";          // Age
                    case 3: return "pc.p0.w";          // Lifetime
                    case 4: return "pc.location.xyz";  // Location
                    case 5: return "pc.p1.x";          // Size
                    case 6: return "pc.velocity.xyz";  // Velocity
                    default: return "pc.angular.xyz";  // Angular Velocity
                }
            }
            case ExpressionKind::LightPath: {
                // Ray classification for the pass this shader runs in (the
                // camera pass: see the renderer's ray-state UBO fields).
                switch (outputIndex) {
                    case 0: return "u.rayFlags.x";   // Is Camera Ray
                    case 1: return "u.rayFlags.y";   // Is Shadow Ray
                    case 2: return "u.rayFlags.z";   // Is Diffuse Ray
                    case 3: return "u.rayFlags.w";   // Is Glossy Ray
                    case 4: return "u.rayFlags2.x";  // Is Singular Ray
                    case 5: return "u.rayFlags2.y";  // Is Reflection Ray
                    case 6: return "u.rayFlags2.z";  // Is Transmission Ray
                    case 7: return "u.rayFlags2.w";  // Is Volume Scatter Ray
                    case 8: return "length(u.camPos.xyz - vWorldPos)";  // Ray Length
                    case 9: return "u.rayDepths.x";  // Ray Depth
                    case 10: return "u.rayDepths.y"; // Diffuse Depth
                    case 11: return "u.rayDepths.z"; // Glossy Depth
                    case 12: return "u.rayDepths.w"; // Transparent Depth
                    case 13: return "u.rayDepths2.x";// Transmission Depth
                    default: return "u.rayDepths2.y";// Portal Depth
                }
            }
            case ExpressionKind::Wireframe: {
                // Edge mask from the per-corner barycentric coordinate the
                // renderer stamps into every triangle (material.frag
                // WireframeFactor). Pixel Size = screen-space thickness;
                // otherwise Size is in world units.
                return "WireframeFactor(vBarycentric, " +
                       FloatLit(node.constScalar) + ", " +
                       (node.constBool ? "1.0" : "0.0") + ")";
            }
            case ExpressionKind::VolumeInfo: {
                // Smoke/fluid domain state, fed per instance by the renderer
                // (material.frag uInst.volume / volume2, indexed by the same
                // self index the Raycast node uses).
                std::string selfIdx = "int(pc.pad + 0.5)";
                switch (outputIndex) {
                    case 0: return "uInst.volume[" + selfIdx + "].xyz";  // Color
                    case 1: return "uInst.volume[" + selfIdx + "].w";    // Density
                    case 2: return "uInst.volume2[" + selfIdx + "].x";   // Flame
                    default: return "uInst.volume2[" + selfIdx + "].y";  // Temperature
                }
            }
            case ExpressionKind::ColorAttribute: {
                // Mesh vertex color (passed through scene.vert). The engine's
                // mesh data carries one color set, so Color/Alpha read it
                // directly; the named Color Attribute property is stored for
                // when multi-attribute mesh data lands.
                if (outputIndex == 1) return "vVertexColor.a";
                return "vVertexColor.rgb";
            }
            case ExpressionKind::UvMap: {
                // Texture coordinates from a named UV map. The engine's mesh
                // vertex data currently carries one UV set, so the node
                // outputs the active render UV (Blender's behavior when the
                // UV Map property is unset).
                return "vec3(vUV, 0.0)";
            }
            case ExpressionKind::TextureCoordinateSpaces: {
                // Coordinate systems for texturing. Generated normalizes the
                // object-space position over the instance's bounding box (the
                // renderer uploads per-instance bbox in uInst, indexed by the
                // same self index the Raycast node uses). UV is a vec2 that
                // Blender exposes as vec3 (z = 0).
                std::string selfIdx = "int(pc.pad + 0.5)";
                switch (outputIndex) {
                    case 0:  // Generated
                        return "((vLocalPos - uInst.bboxMin[" + selfIdx +
                               "].xyz) / max(uInst.bboxMax[" + selfIdx +
                               "].xyz - uInst.bboxMin[" + selfIdx +
                               "].xyz, vec3(1e-6)))";
                    case 1:  // Normal (object space)
                        return "normalize(vLocalNormal)";
                    case 2:  // UV
                        return "vec3(vUV, 0.0)";
                    case 3:  // Object (object-space position)
                        return "vLocalPos";
                    case 4:  // Camera (camera-space position)
                        return "(u.view * vec4(vWorldPos, 1.0)).xyz";
                    case 5:  // Window (screen coords, 0..1)
                        return "vec3(gl_FragCoord.xy / u.viewSize.xy, 0.0)";
                    default:  // Reflection (world-space reflection vector)
                        return "reflect(-normalize(u.camPos.xyz - vWorldPos), "
                               "vNormal)";
                }
            }
            case ExpressionKind::Raycast: {
                // Traces a ray against the scene's uploaded instances (the
                // material.frag RaycastScene helper). Position/Direction/
                // Length default to the shading point when unconnected;
                // Only Local restricts hits to the shaded object itself.
                std::string origin = Input(node, 0, "vWorldPos");
                std::string dir = Input(node, 1, "normalize(vNormal)");
                std::string len = Input(node, 2, "1.0");
                std::string selfOnly = node.constBool ? "1.0" : "0.0";
                std::string ray = "RaycastScene(" + origin + ", normalize(" +
                                  dir + "), " + len + ", " + selfOnly + ")";
                switch (outputIndex) {
                    case 0: return "(" + ray + ").isHit";
                    case 1: return "(" + ray + ").selfHit";
                    case 2: return "(" + ray + ").distance";
                    case 3: return "(" + ray + ").position";
                    default: return "(" + ray + ").normal";
                }
            }
            case ExpressionKind::LayerWeight: {
                std::string nrm = Input(node, 1, "normalize(vNormal)");
                std::string v = "normalize(u.camPos.xyz - vWorldPos)";
                std::string ndv =
                    "max(dot(normalize(" + nrm + "), " + v + "), 0.0)";
                if (outputIndex == 1) {
                    // Facing: 1 looking straight on, 0 at grazing angles.
                    return "(1.0 - (" + ndv + "))";
                }
                // Fresnel: pow(1 - dot, Blend); Blend is the convenient
                // 0..1 weight instead of an IOR.
                return "pow(1.0 - (" + ndv + "), " + Input(node, 0, "0.5") +
                       ")";
            }
            case ExpressionKind::CurvesInfo:
            case ExpressionKind::GeometryInfo:
            case ExpressionKind::CameraData:
            case ExpressionKind::Bevel:
            case ExpressionKind::Attribute:
                // Geometry/hair/camera nodes describe the shading point;
                // their runtime plumbing hasn't landed in the material
                // compiler yet. Fail loudly instead of emitting a wrong value.
                Fail("node '" + node.title +
                     "' is not supported by the runtime shader yet");
                return "0.0";
            case ExpressionKind::Desaturation: {
                std::string in = Input(node, 0, "vec3(0.0)");
                return "mix(vec3(dot(" + in +
                       ", vec3(0.299, 0.587, 0.114))), " + in + ", " +
                       FloatLit(node.fraction) + ")";
            }
            case ExpressionKind::SphereMask: {
                std::string a = Input(node, 0, "vec3(0.0)");
                std::string b = Input(node, 1, "vec3(0.0)");
                std::string radius = Input(node, 2, FloatLit(node.radius));
                std::string hard =
                    Input(node, 3, FloatLit(node.hardness));
                return "(1.0 - clamp((distance(" + a + ", " + b + ") - " +
                       radius + ") / max(" + hard + ", 1e-4), 0.0, 1.0))";
            }
            case ExpressionKind::If: {
                std::string a = Input(node, 0, "0.0");
                std::string b = Input(node, 1, "0.0");
                std::string gt = Input(node, 2, "0.0");
                std::string eq = Input(node, 3, "0.0");
                std::string lt = Input(node, 4, "0.0");
                return "((" + a + " > " + b + ") ? " + gt + " : ((" + a +
                       " < " + b + ") ? " + lt + " : " + eq + "))";
            }
            case ExpressionKind::VectorNoise: {
                std::string p = Input(node, 0, "vWorldPos");
                std::string s = FloatLit(node.noiseScale);
                const char* kH1 = "vec3(12.9898, 78.233, 37.719)";
                return "vec3("
                       "fract(sin(dot(floor(" + p + " * " + s +
                       ") + vec3(0.0, 0.0, 0.0), " + kH1 + ")) * 43758.5453), "
                       "fract(sin(dot(floor(" + p + " * " + s +
                       ") + vec3(1.0, 1.0, 1.0), " + kH1 + ")) * 43758.5453), "
                       "fract(sin(dot(floor(" + p + " * " + s +
                       ") + vec3(2.0, 2.0, 2.0), " + kH1 + ")) * 43758.5453))";
            }

            // --- Existing core (kept verbatim) ---
            case ExpressionKind::Clamp:
                return "clamp(" + Input(node, 0, "0.0") + ", " +
                       FloatLit(node.clampMin) + ", " + FloatLit(node.clampMax) +
                       ")";
            case ExpressionKind::Saturate:
                return "clamp(" + Input(node, 0, "0.0") + ", 0.0, 1.0)";

            default:
                Fail("unsupported expression kind '" + node.title + "'");
                return "0.0";
        }
    }

    std::unordered_set<ExpressionId> m_path;
};

}  // namespace

std::unique_ptr<MaterialCompiler> CreateGLSLMaterialCompiler() {
    return std::make_unique<GLSLMaterialCompiler>();
}

}  // namespace Luma::Material
