#pragma once
#include <memory>
#include "physics_world.hpp"
#include "character_controller.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Physics {

class PhysicsSystem {
public:
    PhysicsSystem(float cellSize = 2.0f);
    ~PhysicsSystem() = default;

    void Update(ECS::Registry* registry, float dt);

    PhysicsWorld& GetWorld() { return m_world; }
    const PhysicsWorld& GetWorld() const { return m_world; }

private:
    PhysicsWorld m_world;
    CharacterController m_controller;
};

} // namespace KumariEngine::Physics
