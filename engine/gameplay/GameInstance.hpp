#pragma once
#include <memory>
#include <string>
#include "ecs/ecs.hpp"

namespace KumariEngine::GameFramework {

class World;

class GameInstance {
public:
    static GameInstance& Get() {
        static GameInstance instance;
        return instance;
    }

    GameInstance(const GameInstance&) = delete;
    GameInstance& operator=(const GameInstance&) = delete;

    // Lifecycle functions
    bool Initialize(ECS::Registry* registry);
    void Shutdown();
    void Update(float deltaTime);
    bool Load();
    bool Unload();

    std::shared_ptr<World> GetWorld() const { return m_world; }

    bool LoadLevel(const std::string& levelName);
    bool UnloadLevel(const std::string& levelName);

    ECS::Registry* GetRegistry() const { return m_registry; }

    bool IsEditorMode() const { return m_isEditorMode; }
    void SetEditorMode(bool isEditor) { m_isEditorMode = isEditor; }

private:
    GameInstance() = default;
    ~GameInstance() = default;

    ECS::Registry* m_registry = nullptr;
    std::shared_ptr<World> m_world;
    bool m_initialized = false;
    bool m_isEditorMode = false;
};

} // namespace KumariEngine::GameFramework

namespace KumariEngine::Gameplay {
    using GameFramework::GameInstance;
}
