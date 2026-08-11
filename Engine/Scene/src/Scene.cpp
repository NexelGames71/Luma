#include "Luma/Scene/Scene.h"

namespace Luma {

Entity Scene::CreateEntity(const std::string& name) {
    Entity e = m_registry.create();
    m_registry.emplace<NameComponent>(e, name);
    m_registry.emplace<TransformComponent>(e);
    return e;
}

void Scene::DestroyEntity(Entity entity) {
    if (m_registry.valid(entity)) m_registry.destroy(entity);
}

}  // namespace Luma
