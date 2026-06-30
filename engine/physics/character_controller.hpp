#pragma once
#include <glm/glm.hpp>
#include "physics_types.hpp"
#include "physics_components.hpp"
#include "physics_world.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Physics {

class CharacterController {
public:
    CharacterController() = default;
    ~CharacterController() = default;

    // Updates the character position using kinematic sliding and collision rules
    void Update(ECS::Registry* registry, ECS::Entity entity, 
                PhysicsComponent& pc, CharacterControllerComponent& cc, 
                PhysicsWorld& world, float dt);
};

} // namespace KumariEngine::Physics
