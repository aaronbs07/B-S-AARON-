#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ecs/ecs.hpp"

// Forward declaration of SpawnManager from Gameplay namespace
namespace KumariEngine::Gameplay {
class SpawnManager;
class GameMode;
}

namespace KumariEngine::GameFramework {

class Level;

class World {
public:
    World(ECS::Registry* registry);
    virtual ~World();

    // Lifecycle functions
    virtual bool Initialize();
    virtual void Shutdown();
    virtual void Update(float deltaTime);
    virtual bool Load();
    virtual bool Unload();

    std::shared_ptr<Level> GetPersistentLevel() const { return m_persistentLevel; }
    const std::vector<std::shared_ptr<Level>>& GetSubLevels() const { return m_subLevels; }

    std::shared_ptr<Level> CreateSubLevel(const std::string& levelName);
    void DestroySubLevel(const std::string& levelName);
    std::shared_ptr<Level> GetSubLevel(const std::string& levelName) const;

    ECS::Registry* GetRegistry() const { return m_registry; }
    std::shared_ptr<Gameplay::SpawnManager> GetSpawnManager() const { return m_spawnManager; }
    std::shared_ptr<Gameplay::GameMode> GetGameMode() const { return m_gameMode; }
    void SetGameMode(std::shared_ptr<Gameplay::GameMode> gameMode) { m_gameMode = gameMode; }

private:
    ECS::Registry* m_registry = nullptr;
    std::shared_ptr<Level> m_persistentLevel;
    std::vector<std::shared_ptr<Level>> m_subLevels;
    std::shared_ptr<Gameplay::SpawnManager> m_spawnManager;
    std::shared_ptr<Gameplay::GameMode> m_gameMode;
    bool m_initialized = false;
};

} // namespace KumariEngine::GameFramework

namespace KumariEngine::Gameplay {
    using GameFramework::World;
}
