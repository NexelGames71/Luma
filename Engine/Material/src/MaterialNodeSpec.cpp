#include "Luma/Material/MaterialNodeSpec.h"

#include <iterator>

namespace Luma::Material {

// The node registry. Nodes are re-added one at a time: append a
// MaterialNodeSpec entry (kind, name, category, pins, properties) and the
// palette, node factory, and editor drawing all pick it up automatically.
//
//   constexpr NodePinSpec kSomePins[] = {
//       {"A", MCT_Numeric, false},  // input
//       {"", MCT_Unknown, true},    // output
//   };
//   constexpr NodePropertySpec kSomeProps[] = {
//       {"Value", NodePropField::Scalar},
//   };
//   specs.push_back({ExpressionKind::Add, "Add", "Math",
//                    {std::begin(kSomePins), std::end(kSomePins)},
//                    {std::begin(kSomeProps), std::end(kSomeProps)}});
//
namespace {

// --- Input > Constant (Boolean) ---------------------------------------------
// A compile-time Boolean value. No input sockets; one Boolean output; a
// single true/false property.
constexpr NodePinSpec kConstantPins[] = {
    {"Boolean", MCT_StaticBool, true},  // output
};
constexpr NodePropertySpec kConstantProps[] = {
    {"Boolean", NodePropField::Bool},
};

// --- Input > Color -----------------------------------------------------------
// A constant color chosen with the color picker. No input sockets; one
// Color (vec3) output; a color-picker property.
constexpr NodePinSpec kColorPins[] = {
    {"Color", MCT_Float3, true},  // output
};
constexpr NodePropertySpec kColorProps[] = {
    {"Color", NodePropField::Color},
};

// --- Input > Value -----------------------------------------------------------
// A single floating-point value fed to other nodes. No input sockets; one
// Value (float) output; one float property.
constexpr NodePinSpec kValuePins[] = {
    {"Value", MCT_Float1, true},  // output
};
constexpr NodePropertySpec kValueProps[] = {
    {"Value", NodePropField::Scalar},
};

// --- Input > Menu ------------------------------------------------------------
// Outputs a menu value. The option list is defined by the connected menu
// socket (none exist yet, so the node is inert until menu sockets land).
constexpr NodePinSpec kMenuPins[] = {
    {"Menu", MCT_Menu, true},  // output
};

// --- Input > Integer ---------------------------------------------------------
// A single integer value. No input sockets; one Integer output; one integer
// property.
constexpr NodePinSpec kIntegerPins[] = {
    {"Integer", MCT_Int, true},  // output
};
constexpr NodePropertySpec kIntegerProps[] = {
    {"Integer", NodePropField::Int},
};

// --- Input > Vector ----------------------------------------------------------
// A constant vector with 2, 3 or 4 components. No input sockets; one Vector
// output whose arity follows the Dimensions property.
constexpr NodePinSpec kVectorPins[] = {
    {"Vector", MCT_Float3, true},  // output (arity follows vectorDim)
};
constexpr NodePropertySpec kVectorProps[] = {
    {"Dimensions", NodePropField::VectorDim, "2;3;4"},
    {"Value", NodePropField::VectorComponents},
};

// --- Input > Ambient Occlusion -----------------------------------------------
// Computes how much the hemisphere above the shading point is occluded.
// Runtime ray-traced AO is a future renderer feature; the node's pins and
// properties are fully editable in the editor today.
constexpr NodePinSpec kAoPins[] = {
    {"Color", MCT_Float3, false},    // tint for the AO output
    {"Distance", MCT_Float1, false}, // occlusion search distance
    {"Normal", MCT_Float3, false},   // shading normal
    {"Color", MCT_Float3, true},     // AO with color tint
    {"AO", MCT_Float1, true},        // AO factor without tint
};
constexpr NodePropertySpec kAoProps[] = {
    {"Samples", NodePropField::Int, "", 16.0f},
    {"Inside", NodePropField::Bool},
    {"Only Local", NodePropField::Bool2},
};

// --- Input > Curves Info -----------------------------------------------------
// Hair-strand information. No inputs / properties.
constexpr NodePinSpec kCurvesInfoPins[] = {
    {"Is Strand", MCT_Float1, true},
    {"Intercept", MCT_Float1, true},
    {"Length", MCT_Float1, true},
    {"Thickness", MCT_Float1, true},
    {"Tangent Normal", MCT_Float3, true},
    {"Random", MCT_Float1, true},
};

// --- Input > Geometry --------------------------------------------------------
// Geometric information about the current shading point (world space).
constexpr NodePinSpec kGeometryPins[] = {
    {"Position", MCT_Float3, true},
    {"Normal", MCT_Float3, true},
    {"Tangent", MCT_Float3, true},
    {"True Normal", MCT_Float3, true},
    {"Incoming", MCT_Float3, true},
    {"Parametric", MCT_Float2, true},
    {"Backfacing", MCT_Float1, true},
    {"Pointiness", MCT_Float1, true},
    {"Random per Island", MCT_Float1, true},
};

// --- Input > Fresnel ---------------------------------------------------------
// Dielectric Fresnel weight from an IOR + normal (Schlick).
constexpr NodePinSpec kFresnelPins[] = {
    {"IOR", MCT_Float1, false},
    {"Normal", MCT_Float3, false},
    {"Factor", MCT_Float1, true},
};

// --- Input > Camera Data -----------------------------------------------------
// Shading point relative to the camera.
constexpr NodePinSpec kCameraDataPins[] = {
    {"View Vector", MCT_Float3, true},
    {"View Z Depth", MCT_Float1, true},
    {"View Distance", MCT_Float1, true},
};

// --- Input > Bevel -----------------------------------------------------------
// Rounded-corner shading normal (Cycles-style bevel).
constexpr NodePinSpec kBevelPins[] = {
    {"Radius", MCT_Float1, false},
    {"Normal", MCT_Float3, false},
    {"Normal", MCT_Float3, true},
};
constexpr NodePropertySpec kBevelProps[] = {
    {"Samples", NodePropField::Int, "", 4.0f},
};

// --- Input > Layer Weight ----------------------------------------------------
// Weights for layering shaders: a Fresnel-like weight (Blend in the 0..1
// range instead of an IOR) and a facing weight.
constexpr NodePinSpec kLayerWeightPins[] = {
    {"Blend", MCT_Float1, false},
    {"Normal", MCT_Float3, false},
    {"Fresnel", MCT_Float1, true},
    {"Facing", MCT_Float1, true},
};

// --- Input > Object Info -----------------------------------------------------
// Per-object instance data (world location, viewport color, pass indices,
// per-instance random).
constexpr NodePinSpec kObjectInfoPins[] = {
    {"Location", MCT_Float3, true},
    {"Color", MCT_Float3, true},
    {"Alpha", MCT_Float1, true},
    {"Object Index", MCT_Float1, true},
    {"Material Index", MCT_Float1, true},
    {"Random", MCT_Float1, true},
};

// --- Input > Point Info ------------------------------------------------------
// State of an individual point of a point cloud.
constexpr NodePinSpec kPointInfoPins[] = {
    {"Location", MCT_Float3, true},
    {"Radius", MCT_Float1, true},
    {"Random", MCT_Float1, true},
};

// --- Input > Particle Info ---------------------------------------------------
// State of the particle that spawned this object instance.
constexpr NodePinSpec kParticleInfoPins[] = {
    {"Index", MCT_Float1, true},
    {"Random", MCT_Float1, true},
    {"Age", MCT_Float1, true},
    {"Lifetime", MCT_Float1, true},
    {"Location", MCT_Float3, true},
    {"Size", MCT_Float1, true},
    {"Velocity", MCT_Float3, true},
    {"Angular Velocity", MCT_Float3, true},
};

// --- Input > Light Path ------------------------------------------------------
// Incoming-ray classification (is camera / shadow / diffuse / glossy, ray
// length + bounce depths).
constexpr NodePinSpec kLightPathPins[] = {
    {"Is Camera Ray", MCT_Float1, true},
    {"Is Shadow Ray", MCT_Float1, true},
    {"Is Diffuse Ray", MCT_Float1, true},
    {"Is Glossy Ray", MCT_Float1, true},
    {"Is Singular Ray", MCT_Float1, true},
    {"Is Reflection Ray", MCT_Float1, true},
    {"Is Transmission Ray", MCT_Float1, true},
    {"Is Volume Scatter Ray", MCT_Float1, true},
    {"Ray Length", MCT_Float1, true},
    {"Ray Depth", MCT_Float1, true},
    {"Diffuse Depth", MCT_Float1, true},
    {"Glossy Depth", MCT_Float1, true},
    {"Transparent Depth", MCT_Float1, true},
    {"Transmission Depth", MCT_Float1, true},
    {"Portal Depth", MCT_Float1, true},
};

// --- Input > Texture Coordinate ----------------------------------------------
// Outputs various coordinate systems for texturing. No inputs; the Object
// property is a string naming a specific object (unused until scene object
// lookup lands), and From Instancer is a Cycles-only flag (stored, inert
// in the real-time pipeline).
constexpr NodePropertySpec kTexCoordProps[] = {
    {"Object", NodePropField::String},
    {"From Instancer", NodePropField::Bool2},
};
constexpr NodePinSpec kTexCoordPins[] = {
    {"Generated", MCT_Float3, true},
    {"Normal", MCT_Float3, true},
    {"UV", MCT_Float3, true},
    {"Object", MCT_Float3, true},
    {"Camera", MCT_Float3, true},
    {"Window", MCT_Float3, true},
    {"Reflection", MCT_Float3, true},
};

// --- Input > UV Map ----------------------------------------------------------
// Texture coordinates from a named UV map. The engine currently carries one
// UV set per mesh, so an unset UV Map falls back to the active render UV;
// the name is stored for when multi-UV vertex data lands. From Instancer
// is a Cycles-only flag (stored, inert in the real-time pipeline).
constexpr NodePropertySpec kUvMapProps[] = {
    {"From Instancer", NodePropField::Bool2},
    {"UV Map", NodePropField::String},
};
constexpr NodePinSpec kUvMapPins[] = {
    {"UV", MCT_Float3, true},
};

// --- Input > Color Attribute -------------------------------------------------
// Access to the mesh's vertex color attribute. The engine's mesh vertex data
// carries one color set; the Color Attribute name is stored for when
// multi-attribute mesh data lands (the runtime uses the mesh's color set).
constexpr NodePropertySpec kColorAttributeProps[] = {
    {"Color Attribute", NodePropField::String},
};
constexpr NodePinSpec kColorAttributePins[] = {
    {"Color", MCT_Float3, true},
    {"Alpha", MCT_Float1, true},
};

// --- Input > Volume Info -----------------------------------------------------
// Smoke/fluid domain state, fed by the renderer's per-instance volume
// payload (material.frag uInst.volume / volume2). No properties.
constexpr NodePinSpec kVolumeInfoPins[] = {
    {"Color", MCT_Float3, true},
    {"Density", MCT_Float1, true},
    {"Flame", MCT_Float1, true},
    {"Temperature", MCT_Float1, true},
};

// --- Input > Wireframe -------------------------------------------------------
// Edge mask from the mesh topology. Size is the edge-line thickness in
// screen pixels (Pixel Size on) or world units (Pixel Size off).
constexpr NodePropertySpec kWireframeProps[] = {
    {"Pixel Size", NodePropField::Bool},
    {"Size", NodePropField::Scalar, "", 0.01f},
};
constexpr NodePinSpec kWireframePins[] = {
    {"Factor", MCT_Float1, true},
};

// --- Input > Raycast ---------------------------------------------------------
// Traces a ray against the scene and reports the first surface hit. The
// runtime shader (material.frag RaycastScene) intersects the instances the
// renderer uploads each frame; Only Local restricts hits to the shaded
// object itself.
constexpr NodePropertySpec kRaycastProps[] = {
    {"Only Local", NodePropField::Bool},
};
constexpr NodePinSpec kRaycastPins[] = {
    {"Position", MCT_Float3, false},
    {"Direction", MCT_Float3, false},
    {"Length", MCT_Float1, false},
    {"Is Hit", MCT_Float1, true},
    {"Self Hit", MCT_Float1, true},
    {"Hit Distance", MCT_Float1, true},
    {"Hit Position", MCT_Float3, true},
    {"Hit Normal", MCT_Float3, true},
};

// --- Input > Attribute -------------------------------------------------------
// Retrieves an attribute attached to an object / mesh.
constexpr NodePropertySpec kAttributeProps[] = {
    {"Attribute Type", NodePropField::Dropdown,
     "Geometry;Object;Instancer;View Layer"},
    {"Name", NodePropField::String},
};
constexpr NodePinSpec kAttributePins[] = {
    {"Color", MCT_Float3, true},
    {"Vector", MCT_Float3, true},
    {"Factor", MCT_Float1, true},
    {"Alpha", MCT_Float1, true},
};

}  // namespace

const std::vector<MaterialNodeSpec>& MaterialNodeSpecs() {
    static const std::vector<MaterialNodeSpec> kSpecs = {
        {ExpressionKind::Boolean, "Constant", "Input",
         {std::begin(kConstantPins), std::end(kConstantPins)},
         {std::begin(kConstantProps), std::end(kConstantProps)}},
        {ExpressionKind::Constant3, "Color", "Input",
         {std::begin(kColorPins), std::end(kColorPins)},
         {std::begin(kColorProps), std::end(kColorProps)}},
        {ExpressionKind::Constant, "Value", "Input",
         {std::begin(kValuePins), std::end(kValuePins)},
         {std::begin(kValueProps), std::end(kValueProps)}},
        {ExpressionKind::Menu, "Menu", "Input",
         {std::begin(kMenuPins), std::end(kMenuPins)},
         {}},
        {ExpressionKind::Integer, "Integer", "Input",
         {std::begin(kIntegerPins), std::end(kIntegerPins)},
         {std::begin(kIntegerProps), std::end(kIntegerProps)}},
        {ExpressionKind::Vector, "Vector", "Input",
         {std::begin(kVectorPins), std::end(kVectorPins)},
         {std::begin(kVectorProps), std::end(kVectorProps)}},
        {ExpressionKind::AmbientOcclusion, "Ambient Occlusion", "Input",
         {std::begin(kAoPins), std::end(kAoPins)},
         {std::begin(kAoProps), std::end(kAoProps)}},
        {ExpressionKind::CurvesInfo, "Curves Info", "Input",
         {std::begin(kCurvesInfoPins), std::end(kCurvesInfoPins)},
         {}},
        {ExpressionKind::GeometryInfo, "Geometry", "Input",
         {std::begin(kGeometryPins), std::end(kGeometryPins)},
         {}},
        {ExpressionKind::Fresnel, "Fresnel", "Input",
         {std::begin(kFresnelPins), std::end(kFresnelPins)},
         {}},
        {ExpressionKind::CameraData, "Camera Data", "Input",
         {std::begin(kCameraDataPins), std::end(kCameraDataPins)},
         {}},
        {ExpressionKind::Bevel, "Bevel", "Input",
         {std::begin(kBevelPins), std::end(kBevelPins)},
         {std::begin(kBevelProps), std::end(kBevelProps)}},
        {ExpressionKind::Attribute, "Attribute", "Input",
         {std::begin(kAttributePins), std::end(kAttributePins)},
         {std::begin(kAttributeProps), std::end(kAttributeProps)}},
        {ExpressionKind::LayerWeight, "Layer Weight", "Input",
         {std::begin(kLayerWeightPins), std::end(kLayerWeightPins)},
         {}},
        {ExpressionKind::ObjectInfo, "Object Info", "Input",
         {std::begin(kObjectInfoPins), std::end(kObjectInfoPins)},
         {}},
        {ExpressionKind::LightPath, "Light Path", "Input",
         {std::begin(kLightPathPins), std::end(kLightPathPins)},
         {}},
        {ExpressionKind::ParticleInfo, "Particle Info", "Input",
         {std::begin(kParticleInfoPins), std::end(kParticleInfoPins)},
         {}},
        {ExpressionKind::PointInfo, "Point Info", "Input",
         {std::begin(kPointInfoPins), std::end(kPointInfoPins)},
         {}},
        {ExpressionKind::Raycast, "Raycast", "Input",
         {std::begin(kRaycastPins), std::end(kRaycastPins)},
         {std::begin(kRaycastProps), std::end(kRaycastProps)}},
        {ExpressionKind::TextureCoordinateSpaces, "Texture Coordinate", "Input",
         {std::begin(kTexCoordPins), std::end(kTexCoordPins)},
         {std::begin(kTexCoordProps), std::end(kTexCoordProps)}},
        {ExpressionKind::UvMap, "UV Map", "Input",
         {std::begin(kUvMapPins), std::end(kUvMapPins)},
         {std::begin(kUvMapProps), std::end(kUvMapProps)}},
        {ExpressionKind::ColorAttribute, "Color Attribute", "Input",
         {std::begin(kColorAttributePins), std::end(kColorAttributePins)},
         {std::begin(kColorAttributeProps), std::end(kColorAttributeProps)}},
        {ExpressionKind::VolumeInfo, "Volume Info", "Input",
         {std::begin(kVolumeInfoPins), std::end(kVolumeInfoPins)},
         {}},
        {ExpressionKind::Wireframe, "Wireframe", "Input",
         {std::begin(kWireframePins), std::end(kWireframePins)},
         {std::begin(kWireframeProps), std::end(kWireframeProps)}},
    };
    return kSpecs;
}

const MaterialNodeSpec* MaterialNodeSpecFor(ExpressionKind kind) {
    const auto& specs = MaterialNodeSpecs();
    for (const auto& spec : specs) {
        if (spec.kind == kind) return &spec;
    }
    return nullptr;
}

int MaterialNodePropCount(ExpressionKind kind) {
    const MaterialNodeSpec* spec = MaterialNodeSpecFor(kind);
    return spec ? static_cast<int>(spec->props.size()) : 0;
}

}  // namespace Luma::Material
