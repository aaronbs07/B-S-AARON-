#include "LevelManager.hpp"
#include "Level.hpp"
#include "GameInstance.hpp"
#include "World.hpp"
#include "core/logger.hpp"
#include <chrono>
#include <thread>

namespace KumariEngine::Gameplay {

void LevelManager::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    Core::Logger::Info("LevelManager", "LevelManager initialized.");
}

void LevelManager::Shutdown() {
    m_registry = nullptr;
    Core::Logger::Info("LevelManager", "LevelManager shut down.");
}

std::shared_ptr<GameFramework::Level> LevelManager::LoadLevel(const std::string& levelName) {
    if (!m_registry) {
        Core::Logger::Error("LevelManager", "Cannot load level: Registry is null.");
        return nullptr;
    }

    auto world = GameFramework::GameInstance::Get().GetWorld();
    if (!world) {
        Core::Logger::Error("LevelManager", "Cannot load level: World is null.");
        return nullptr;
    }

    Core::Logger::Info("LevelManager", "Synchronously loading level '%s'...", levelName.c_str());
    
    // Create sublevel in the active World
    auto level = world->CreateSubLevel(levelName);
    if (level) {
        level->Load();
    }

    // Populate level with default placeholder entities (e.g. static geometry / environment meshes)
    // To remain generic, we spawn a placeholder entity representing the level's root node
    ECS::Entity levelRoot = m_registry->CreateEntity();
    if (level) {
        level->AddEntity(levelRoot);
    }

    Core::Logger::Info("LevelManager", "Level '%s' loaded successfully with root entity %u.", levelName.c_str(), levelRoot);
    return level;
}

void LevelManager::UnloadLevel(const std::string& levelName) {
    auto world = GameFramework::GameInstance::Get().GetWorld();
    if (!world) return;

    Core::Logger::Info("LevelManager", "Unloading level '%s'...", levelName.c_str());
    world->DestroySubLevel(levelName);
}

std::future<std::shared_ptr<GameFramework::Level>> LevelManager::LoadLevelAsync(
    const std::string& levelName, std::function<void(float)> progressCallback) {
    
    Core::Logger::Info("LevelManager", "Starting async load for level '%s'...", levelName.c_str());

    // Dispatch loading to standard async background worker
    return std::async(std::launch::async, [this, levelName, progressCallback]() -> std::shared_ptr<GameFramework::Level> {
        const int steps = 10;
        for (int i = 1; i <= steps; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Simulate IO latency
            float progress = static_cast<float>(i) / static_cast<float>(steps);
            if (progressCallback) {
                progressCallback(progress);
            }
        }

        // Complete the load on this thread
        auto level = this->LoadLevel(levelName);
        return level;
    });
}

bool LevelManager::TransitionToLevel(const std::string& targetLevelName) {
    auto world = GameFramework::GameInstance::Get().GetWorld();
    if (!world) return false;

    Core::Logger::Info("LevelManager", "Beginning transition to level '%s'...", targetLevelName.c_str());

    // 1. Unload all current sub-levels
    auto subLevelsCopy = world->GetSubLevels();
    for (const auto& level : subLevelsCopy) {
        UnloadLevel(level->GetName());
    }

    // 2. Clear out persistent level entities (excluding player controllers/states)
    auto persistent = world->GetPersistentLevel();
    if (persistent) {
        persistent->Unload();
    }

    // 3. Load the new level
    auto newLevel = LoadLevel(targetLevelName);
    if (newLevel) {
        Core::Logger::Info("LevelManager", "Transition to level '%s' completed successfully.", targetLevelName.c_str());
        return true;
    }

    Core::Logger::Error("LevelManager", "Failed to transition to level '%s'.", targetLevelName.c_str());
    return false;
}

} // namespace KumariEngine::Gameplay
