#pragma once

#include <string>

#include <entt/entt.hpp>

#include "Luma/Core/Types.h"
#include "Luma/Scene/Components.h"

// Luma::Scene - the world's entity registry, backed by EnTT. Owns entities and
// their components; systems (rendering, physics, ...) iterate views over it.

namespace Luma {

using Entity = entt::entity;
inline constexpr Entity kNullEntity = entt::null;

class Scene {
public:
    // Creates an entity with a Name + Transform and returns its handle.
    Entity CreateEntity(const std::string& name);
    void DestroyEntity(Entity entity);
    bool IsValid(Entity entity) const { return m_registry.valid(entity); }

    entt::registry& Registry() { return m_registry; }
    const entt::registry& Registry() const { return m_registry; }

    usize EntityCount() const {
        return m_registry.view<const NameComponent>().size();
    }

private:
    entt::registry m_registry;
};

}  // namespace Luma
