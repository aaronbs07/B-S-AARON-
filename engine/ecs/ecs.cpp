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

Entity Registry::CreateEntityWithID(Entity entity) {
    if (IsAlive(entity)) return entity;

    auto it = std::find(m_freeEntities.begin(), m_freeEntities.end(), entity);
    if (it != m_freeEntities.end()) {
        m_freeEntities.erase(it);
    }

    if (entity >= m_nextEntity) {
        m_nextEntity = entity + 1;
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

    // Erase GUID mappings if they exist
    auto it = m_entityToGUID.find(entity);
    if (it != m_entityToGUID.end()) {
        m_guidToEntity.erase(it->second);
        m_entityToGUID.erase(it);
    }

    // Iterate all component pools to release assets bound to this entity
    for (auto& [type, pool] : m_componentPools) {
        pool->EntityDestroyed(entity);
    }

    m_freeEntities.push_back(entity);
}

Save::EntityGUID Registry::GetGUID(Entity entity) const {
    auto it = m_entityToGUID.find(entity);
    if (it != m_entityToGUID.end()) {
        return it->second;
    }
    return Save::NULL_GUID;
}

Entity Registry::GetEntityByGUID(const Save::EntityGUID& guid) const {
    auto it = m_guidToEntity.find(guid);
    if (it != m_guidToEntity.end()) {
        return it->second;
    }
    return NULL_ENTITY;
}

void Registry::AssignGUID(Entity entity, const Save::EntityGUID& guid) {
    assert(IsAlive(entity) && "Cannot assign GUID to dead entity");
    assert(!guid.IsNull() && "Cannot assign null GUID");

    // Clean up old mapping if any
    auto oldIt = m_entityToGUID.find(entity);
    if (oldIt != m_entityToGUID.end()) {
        m_guidToEntity.erase(oldIt->second);
    }

    m_entityToGUID[entity] = guid;
    m_guidToEntity[guid] = entity;
}

Save::EntityGUID Registry::CreateGUID(Entity entity) {
    Save::EntityGUID guid = Save::GUIDGenerator::Generate();
    while (m_guidToEntity.find(guid) != m_guidToEntity.end()) {
        guid = Save::GUIDGenerator::Generate();
    }
    AssignGUID(entity, guid);
    return guid;
}

std::vector<Entity> Registry::GetAliveEntities() const {
    std::vector<Entity> alive;
    for (Entity e = 1; e < m_nextEntity; ++e) {
        if (IsAlive(e)) {
            alive.push_back(e);
        }
    }
    return alive;
}

} // namespace KumariEngine::ECS
