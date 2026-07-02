#include "World.hpp"
#include "Level.hpp"
#include "SpawnManager.hpp"
#include "GameMode.hpp"
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::GameFramework {

World::World(ECS::Registry* registry)
    : m_registry(registry) {
}

World::~World() {
    Shutdown();
}

bool World::Initialize() {
    if (m_initialized) return true;

    if (!m_registry) {
        Core::Logger::Error("World", "Cannot initialize World with null registry.");
        return false;
    }

    m_persistentLevel = std::make_shared<Level>("PersistentLevel", m_registry);
    m_persistentLevel->Initialize();
    m_persistentLevel->SetActive(true);
    m_persistentLevel->SetVisible(true);

    m_spawnManager = std::make_shared<Gameplay::SpawnManager>(m_registry);
    m_gameMode = std::make_shared<Gameplay::GameMode>(m_registry);
    m_initialized = true;

    Core::Logger::Info("World", "World initialized successfully.");
    return true;
}

void World::Shutdown() {
    if (!m_initialized) return;

    Core::Logger::Info("World", "World shutting down...");
    Unload();

    if (m_persistentLevel) {
        m_persistentLevel->Shutdown();
        m_persistentLevel.reset();
    }

    m_spawnManager.reset();
    m_gameMode.reset();
    m_registry = nullptr;
    m_initialized = false;
    Core::Logger::Info("World", "World shut down complete.");
}

void World::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update GameMode
    if (m_gameMode) {
        m_gameMode->Tick(deltaTime);
    }

    // Update the persistent level
    if (m_persistentLevel && m_persistentLevel->IsActive()) {
        m_persistentLevel->Update(deltaTime);
    }

    // Update active sub-levels
    for (auto& level : m_subLevels) {
        if (level && level->IsActive()) {
            level->Update(deltaTime);
        }
    }
}

bool World::Load() {
    Core::Logger::Info("World", "Loading World state.");
    return true;
}

bool World::Unload() {
    Core::Logger::Info("World", "Unloading World. Clearing all sub-levels.");
    for (auto& level : m_subLevels) {
        if (level) {
            level->Unload();
            level->Shutdown();
        }
    }
    m_subLevels.clear();
    return true;
}

std::shared_ptr<Level> World::CreateSubLevel(const std::string& levelName) {
    auto level = GetSubLevel(levelName);
    if (level) {
        return level;
    }
    level = std::make_shared<Level>(levelName, m_registry);
    level->Initialize();
    m_subLevels.push_back(level);
    return level;
}

void World::DestroySubLevel(const std::string& levelName) {
    auto it = std::remove_if(m_subLevels.begin(), m_subLevels.end(), [&](const std::shared_ptr<Level>& level) {
        if (level->GetName() == levelName) {
            level->Unload();
            level->Shutdown();
            return true;
        }
        return false;
    });
    m_subLevels.erase(it, m_subLevels.end());
}

std::shared_ptr<Level> World::GetSubLevel(const std::string& levelName) const {
    for (const auto& level : m_subLevels) {
        if (level->GetName() == levelName) {
            return level;
        }
    }
    return nullptr;
}

} // namespace KumariEngine::GameFramework
