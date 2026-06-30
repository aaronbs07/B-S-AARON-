#include "ecs.hpp"

namespace KumariEngine::ECS {

Registry::Registry() {
    m_freeEntities.reserve(65536);
}

Registry::~Registry() {
    KumariEngine::Core::Logger::Info("ECS_Perf", "Before Registry destructor pool cleanup");
    m_componentPools.clear();
    KumariEngine::Core::Logger::Info("ECS_Perf", "After Registry destructor pool cleanup");
}

Entity Registry::CreateEntity() {
    Entity entity;
    if (!m_freeEntities.empty()) {
        entity = m_freeEntities.back();
        m_freeEntities.pop_back();
    } else {
        entity = m_nextEntity++;
    }
    if (entity >= m_activeEntities.size()) {
        m_activeEntities.resize(entity + 1, false);
    }
    m_activeEntities[entity] = true;
    return entity;
}

void Registry::DestroyEntity(Entity entity) {
    assert(entity != NULL_ENTITY && "Cannot destroy NULL_ENTITY");
    assert(entity < m_nextEntity && "Cannot destroy an entity ID that has never been created");
    assert(entity < m_activeEntities.size() && m_activeEntities[entity] && "Entity is already destroyed (double destruction)");

    if (entity == NULL_ENTITY || entity >= m_nextEntity || entity >= m_activeEntities.size() || !m_activeEntities[entity]) {
        return;
    }

    m_activeEntities[entity] = false;

    // Iterate all component pools to release assets bound to this entity
    for (auto& [type, pool] : m_componentPools) {
        pool->EntityDestroyed(entity);
    }

    m_freeEntities.push_back(entity);
}

} // namespace KumariEngine::ECS
