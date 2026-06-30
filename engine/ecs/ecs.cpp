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
    if (!m_freeEntities.empty()) {
        Entity entity = m_freeEntities.back();
        m_freeEntities.pop_back();
        return entity;
    }
    return m_nextEntity++;
}

void Registry::DestroyEntity(Entity entity) {
    if (entity == NULL_ENTITY || entity >= m_nextEntity) return;

    // Iterate all component pools to release assets bound to this entity
    for (auto& [type, pool] : m_componentPools) {
        pool->EntityDestroyed(entity);
    }

    m_freeEntities.push_back(entity);
}

} // namespace KumariEngine::ECS
