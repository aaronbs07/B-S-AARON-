#pragma once
#include <string>
#include <memory>
#include <functional>
#include <future>
#include "ecs/ecs.hpp"

namespace KumariEngine::GameFramework {
class Level;
}

namespace KumariEngine::Gameplay {

class LevelManager {
public:
    static LevelManager& Get() {
        static LevelManager instance;
        return instance;
    }

    LevelManager(const LevelManager&) = delete;
    LevelManager& operator=(const LevelManager&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Synchronous operations
    std::shared_ptr<GameFramework::Level> LoadLevel(const std::string& levelName);
    void UnloadLevel(const std::string& levelName);

    // Asynchronous operations
    // Triggers loading on a background thread using std::async, invoking progressCallback if set
    std::future<std::shared_ptr<GameFramework::Level>> LoadLevelAsync(const std::string& levelName, std::function<void(float)> progressCallback = nullptr);

    // Level Transitions
    bool TransitionToLevel(const std::string& targetLevelName);

    ECS::Registry* GetRegistry() const { return m_registry; }

private:
    LevelManager() = default;
    ~LevelManager() = default;

    ECS::Registry* m_registry = nullptr;
};

} // namespace KumariEngine::Gameplay
