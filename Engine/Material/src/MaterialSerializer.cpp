#include "Luma/Material/MaterialSerializer.h"

#include <fstream>
#include <sstream>

#include "Luma/Core/Log.h"
#include "Luma/Serialization/Json.h"

namespace Luma::Material {

namespace {

SerialValue Vec3Value(const Math::Vec3& v) {
    SerialValue arr = SerialValue::MakeArray();
    arr.PushBack(SerialValue(v.x));
    arr.PushBack(SerialValue(v.y));
    arr.PushBack(SerialValue(v.z));
    return arr;
}

SerialValue Vec4Value(const Math::Vec4& v) {
    SerialValue arr = SerialValue::MakeArray();
    arr.PushBack(SerialValue(v.x));
    arr.PushBack(SerialValue(v.y));
    arr.PushBack(SerialValue(v.z));
    arr.PushBack(SerialValue(v.w));
    return arr;
}

bool ReadVec3(const SerialValue& v, Math::Vec3& out) {
    if (!v.IsArray() || v.Size() < 3) return false;
    out.x = static_cast<f32>(v.At(0).AsFloat(0.0));
    out.y = static_cast<f32>(v.At(1).AsFloat(0.0));
    out.z = static_cast<f32>(v.At(2).AsFloat(0.0));
    return true;
}

bool ReadVec4(const SerialValue& v, Math::Vec4& out) {
    if (!v.IsArray() || v.Size() < 4) return false;
    out.x = static_cast<f32>(v.At(0).AsFloat(0.0));
    out.y = static_cast<f32>(v.At(1).AsFloat(0.0));
    out.z = static_cast<f32>(v.At(2).AsFloat(0.0));
    out.w = static_cast<f32>(v.At(3).AsFloat(0.0));
    return true;
}

const char* PropertyKey(MaterialProperty p) {
    switch (p) {
        case MaterialProperty::BaseColor: return "baseColor";
        case MaterialProperty::Metallic: return "metallic";
        case MaterialProperty::Specular: return "specular";
        case MaterialProperty::Roughness: return "roughness";
        case MaterialProperty::Normal: return "normal";
        case MaterialProperty::Tangent: return "tangent";
        case MaterialProperty::EmissiveColor: return "emissive";
        case MaterialProperty::Opacity: return "opacity";
        case MaterialProperty::OpacityMask: return "opacityMask";
        case MaterialProperty::WorldPositionOffset: return "worldPositionOffset";
        case MaterialProperty::SubsurfaceColor: return "subsurfaceColor";
        case MaterialProperty::AmbientOcclusion: return "ambientOcclusion";
        case MaterialProperty::Refraction: return "refraction";
        case MaterialProperty::PixelDepthOffset: return "pixelDepthOffset";
        case MaterialProperty::VolumeColor: return "volumeColor";
        case MaterialProperty::VolumeDensity: return "volumeDensity";
        case MaterialProperty::VolumeFlame: return "volumeFlame";
        case MaterialProperty::VolumeTemperature: return "volumeTemperature";
        case MaterialProperty::Thickness: return "thickness";
        default: return "";
    }
}

}  // namespace

SerialValue MaterialSerializer::ToJson(const Material& material) {
    SerialValue root = SerialValue::MakeObject();
    root["version"] = SerialValue(2);
    root["name"] = SerialValue(material.name);
    root["blendMode"] = SerialValue(static_cast<i64>(material.blendMode));
    root["opacityThreshold"] = SerialValue(material.opacityThreshold);

    // Property values (flat PBR constants).
    SerialValue constants = SerialValue::MakeObject();
    constexpr usize kPropCount = static_cast<usize>(MaterialProperty::Count);
    for (usize i = 0; i < kPropCount; ++i) {
        auto p = static_cast<MaterialProperty>(i);
        const char* key = PropertyKey(p);
        if (key[0] == '\0') continue;
        constants[key] = Vec4Value(material.propertyValues[i]);
    }
    root["constants"] = constants;

    // Texture map slots.
    SerialValue maps = SerialValue::MakeObject();
    auto putMap = [&](const char* key, const AssetId& id) {
        if (id.IsValid()) maps[key] = SerialValue(ToString(id));
    };
    putMap("baseColor", material.baseColorMap);
    putMap("normal", material.normalMap);
    putMap("roughness", material.roughnessMap);
    putMap("metallic", material.metallicMap);
    root["maps"] = maps;

    return root;
}

bool MaterialSerializer::FromJson(Material& out, const SerialValue& root,
                                  std::string* outError) {
    auto fail = [&](const std::string& msg) {
        if (outError) *outError = msg;
        return false;
    };
    if (!root.IsObject()) return fail("material file is not a JSON object");

    out = Material{};
    out.name = root.Find("name") ? root.Find("name")->AsString() : "Material";

    const SerialValue* blend = root.Find("blendMode");
    if (blend && blend->IsNumber()) {
        out.blendMode = static_cast<BlendMode>(blend->AsInt(0));
    }
    const SerialValue* thr = root.Find("opacityThreshold");
    if (thr) out.opacityThreshold = static_cast<f32>(thr->AsFloat(0.5));

    // Texture map slots.
    const SerialValue* maps = root.Find("maps");
    if (maps && maps->IsObject()) {
        auto getMap = [&](const char* key, AssetId& dst) {
            const SerialValue* v = maps->Find(key);
            if (v && v->IsString()) dst = FromString(v->AsString());
        };
        getMap("baseColor", out.baseColorMap);
        getMap("normal", out.normalMap);
        getMap("roughness", out.roughnessMap);
        getMap("metallic", out.metallicMap);
    }

    // Property values.
    const SerialValue* constants = root.Find("constants");
    if (constants && constants->IsObject()) {
        constexpr usize kPropCount = static_cast<usize>(MaterialProperty::Count);
        for (usize i = 0; i < kPropCount; ++i) {
            auto p = static_cast<MaterialProperty>(i);
            const char* key = PropertyKey(p);
            if (key[0] == '\0') continue;
            const SerialValue* v = constants->Find(key);
            if (v && v->IsArray() && v->Size() >= 3) {
                ReadVec4(*v, out.propertyValues[i]);
            }
        }
    }

    return true;
}

bool MaterialSerializer::SaveToFile(const Material& material,
                                    const std::filesystem::path& path,
                                    std::string* outError) {
    std::string json = WriteJson(ToJson(material), true);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (outError) *outError = "cannot open file for writing: " + path.string();
        return false;
    }
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    file.close();
    if (!file) {
        if (outError) *outError = "failed writing file: " + path.string();
        return false;
    }
    return true;
}

bool MaterialSerializer::LoadFromFile(Material& out,
                                      const std::filesystem::path& path,
                                      std::string* outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (outError) *outError = "cannot open file for reading: " + path.string();
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    auto parsed = ParseJson(ss.str(), outError);
    if (!parsed) return false;
    return FromJson(out, *parsed, outError);
}

}  // namespace Luma::Material
