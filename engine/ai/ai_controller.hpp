#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::AI {

class AIController {
public:
    static void MoveTo(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& target);
    static void Stop(ECS::Registry* registry, ECS::Entity entity);
    static void FollowPath(ECS::Registry* registry, ECS::Entity entity, const std::vector<glm::vec3>& path);
    static void RotateTowardsTarget(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& target, float deltaTime);
    
    // Target acquisition scanning framework
    static ECS::Entity AcquireTarget(ECS::Registry* registry, ECS::Entity entity);
};

} // namespace KumariEngine::AI
