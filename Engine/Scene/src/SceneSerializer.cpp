#include "Luma/Scene/SceneSerializer.h"

#include <fstream>
#include <sstream>

#include "Luma/Scene/ComponentReflection.h"
#include "Luma/Serialization/Json.h"
#include "Luma/Serialization/Reflection.h"

namespace Luma {

namespace {

// Writes component `C` of entity `e` into `comps[key]` if the entity has one.
template <class C>
void WriteComponent(const entt::registry& reg, entt::entity e, SerialValue& comps,
                    const char* key) {
    if (const C* c = reg.try_get<C>(e)) {
        comps[key] = SerializeObject(*c);
    }
}

// Reads component `C` from `comps[key]` onto entity `e` if the member is present.
template <class C>
void ReadComponent(entt::registry& reg, entt::entity e, const SerialValue& comps,
                   const char* key) {
    if (const SerialValue* field = comps.Find(key)) {
        C comp{};
        DeserializeObject(*field, comp);
        reg.emplace_or_replace<C>(e, std::move(comp));
    }
}

}  // namespace

std::string SceneSerializer::SaveToString(const Scene& scene, bool pretty) {
    SerialValue root = SerialValue::MakeObject();
    root["version"] = kVersion;

    SerialValue entities = SerialValue::MakeArray();
    const entt::registry& reg = scene.Registry();
    // NameComponent marks the canonical scene objects; iterate those.
    auto view = reg.view<const NameComponent>();
    for (auto e : view) {
        SerialValue entity = SerialValue::MakeObject();
        SerialValue comps = SerialValue::MakeObject();
        WriteComponent<NameComponent>(reg, e, comps, "Name");
        WriteComponent<TransformComponent>(reg, e, comps, "Transform");
        WriteComponent<MeshRendererComponent>(reg, e, comps, "MeshRenderer");
        WriteComponent<CameraComponent>(reg, e, comps, "Camera");
        WriteComponent<LightComponent>(reg, e, comps, "Light");
        WriteComponent<EnvironmentComponent>(reg, e, comps, "Environment");
        entity["components"] = std::move(comps);
        entities.PushBack(std::move(entity));
    }
    root["entities"] = std::move(entities);
    return WriteJson(root, pretty);
}

bool SceneSerializer::LoadFromString(Scene& scene, std::string_view json,
                                     std::string* outError) {
    auto parsed = ParseJson(json, outError);
    if (!parsed) return false;

    const SerialValue* entities = parsed->Find("entities");
    if (!entities || !entities->IsArray()) {
        if (outError) *outError = "scene document has no 'entities' array";
        return false;
    }

    entt::registry& reg = scene.Registry();
    reg.clear();  // replace any existing contents

    for (const SerialValue& entity : entities->Elements()) {
        const SerialValue* comps = entity.Find("components");
        if (!comps) continue;
        entt::entity e = reg.create();
        ReadComponent<NameComponent>(reg, e, *comps, "Name");
        ReadComponent<TransformComponent>(reg, e, *comps, "Transform");
        ReadComponent<MeshRendererComponent>(reg, e, *comps, "MeshRenderer");
        ReadComponent<CameraComponent>(reg, e, *comps, "Camera");
        ReadComponent<LightComponent>(reg, e, *comps, "Light");
        ReadComponent<EnvironmentComponent>(reg, e, *comps, "Environment");
    }
    return true;
}

bool SceneSerializer::SaveToFile(const Scene& scene,
                                 const std::filesystem::path& path,
                                 std::string* outError) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (outError) *outError = "could not open file for writing";
        return false;
    }
    out << SaveToString(scene);
    return true;
}

bool SceneSerializer::LoadFromFile(Scene& scene,
                                   const std::filesystem::path& path,
                                   std::string* outError) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (outError) *outError = "could not open file for reading";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return LoadFromString(scene, ss.str(), outError);
}

}  // namespace Luma
