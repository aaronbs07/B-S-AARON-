#include "GameInstance.hpp"
#include "World.hpp"
#include "Level.hpp"
#include "core/logger.hpp"

namespace KumariEngine::GameFramework {

bool GameInstance::Initialize(ECS::Registry* registry) {
    if (m_initialized) {
        Core::Logger::Warning("GameInstance", "Already initialized.");
        return true;
    }
    if (!registry) {
        Core::Logger::Error("GameInstance", "Cannot initialize with null registry.");
        return false;
    }
    m_registry = registry;
    m_world = std::make_shared<World>(m_registry);
    if (!m_world->Initialize()) {
        Core::Logger::Error("GameInstance", "Failed to initialize World.");
        return false;
    }
    m_initialized = true;
    Core::Logger::Info("GameInstance", "GameInstance initialized successfully.");
    return true;
}

void GameInstance::Shutdown() {
    if (!m_initialized) return;

    Core::Logger::Info("GameInstance", "Beginning GameInstance shutdown sequence...");
    if (m_world) {
        m_world->Shutdown();
        m_world.reset();
    }
    m_registry = nullptr;
    m_initialized = false;
    Core::Logger::Info("GameInstance", "GameInstance shut down.");
}

void GameInstance::Update(float deltaTime) {
    if (!m_initialized) return;

    if (m_world) {
        m_world->Update(deltaTime);
    }
}

bool GameInstance::Load() {
    Core::Logger::Info("GameInstance", "Loading GameInstance session.");
    return true;
}

bool GameInstance::Unload() {
    Core::Logger::Info("GameInstance", "Unloading GameInstance session.");
    return true;
}

bool GameInstance::LoadLevel(const std::string& levelName) {
    if (!m_world) return false;
    Core::Logger::Info("GameInstance", "Loading level via GameInstance: %s", levelName.c_str());
    auto level = m_world->CreateSubLevel(levelName);
    if (level) {
        return level->Load();
    }
    return false;
}

bool GameInstance::UnloadLevel(const std::string& levelName) {
    if (!m_world) return false;
    Core::Logger::Info("GameInstance", "Unloading level via GameInstance: %s", levelName.c_str());
    auto level = m_world->GetSubLevel(levelName);
    if (level) {
        level->Unload();
        m_world->DestroySubLevel(levelName);
        return true;
    }
    return false;
}

} // namespace KumariEngine::GameFramework
