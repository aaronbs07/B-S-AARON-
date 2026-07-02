#include "Level.hpp"
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::GameFramework {

Level::Level(const std::string& name, ECS::Registry* registry)
    : m_name(name), m_registry(registry) {}

Level::~Level() {
    Shutdown();
}

bool Level::Initialize() {
    if (m_initialized) return true;
    m_initialized = true;
    Core::Logger::Info("Level", "Level '%s' initialized.", m_name.c_str());
    return true;
}

void Level::Shutdown() {
    if (!m_initialized) return;
    Unload();
    m_initialized = false;
    Core::Logger::Info("Level", "Level '%s' shut down.", m_name.c_str());
}

void Level::Update(float deltaTime) {
    (void)deltaTime;
    // Empty update logic for entities inside the level, or systems execution
}

bool Level::Load() {
    if (m_loaded) return true;
    m_loaded = true;
    m_isActive = true;
    m_isVisible = true;
    Core::Logger::Info("Level", "Level '%s' loaded successfully.", m_name.c_str());
    return true;
}

bool Level::Unload() {
    if (!m_loaded) return true;

    if (m_registry) {
        for (ECS::Entity entity : m_entities) {
            if (m_registry->IsAlive(entity)) {
                m_registry->DestroyEntity(entity);
            }
        }
    }
    m_entities.clear();
    m_isActive = false;
    m_isVisible = false;
    m_loaded = false;
    Core::Logger::Info("Level", "Level '%s' unloaded.", m_name.c_str());
    return true;
}

void Level::AddEntity(ECS::Entity entity) {
    if (entity == ECS::NULL_ENTITY) return;
    auto it = std::find(m_entities.begin(), m_entities.end(), entity);
    if (it == m_entities.end()) {
        m_entities.push_back(entity);
    }
}

void Level::RemoveEntity(ECS::Entity entity) {
    m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
}

} // namespace KumariEngine::GameFramework
