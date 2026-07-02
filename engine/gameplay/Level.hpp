#pragma once
#include <string>
#include <vector>
#include "ecs/ecs.hpp"

namespace KumariEngine::GameFramework {

class Level {
public:
    Level(const std::string& name, ECS::Registry* registry);
    virtual ~Level();

    // Lifecycle functions
    virtual bool Initialize();
    virtual void Shutdown();
    virtual void Update(float deltaTime);
    virtual bool Load();
    virtual bool Unload();

    const std::string& GetName() const { return m_name; }

    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const { return m_isActive; }

    void SetVisible(bool visible) { m_isVisible = visible; }
    bool IsVisible() const { return m_isVisible; }

    void AddEntity(ECS::Entity entity);
    void RemoveEntity(ECS::Entity entity);
    const std::vector<ECS::Entity>& GetEntities() const { return m_entities; }

private:
    std::string m_name;
    ECS::Registry* m_registry = nullptr;
    bool m_isActive = false;
    bool m_isVisible = false;
    bool m_initialized = false;
    bool m_loaded = false;
    std::vector<ECS::Entity> m_entities;
};

} // namespace KumariEngine::GameFramework

namespace KumariEngine::Gameplay {
    using GameFramework::Level;
}
