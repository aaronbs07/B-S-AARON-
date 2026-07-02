#include "ai_controller.hpp"
#include "ai_components.hpp"
#include "scene/transform_component.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace KumariEngine::AI {

void AIController::MoveTo(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& target) {
    if (registry && registry->HasComponent<NavigationAgentComponent>(entity)) {
        auto& agent = registry->GetComponent<NavigationAgentComponent>(entity);
        agent.agentTarget = target;
    }
}

void AIController::Stop(ECS::Registry* registry, ECS::Entity entity) {
    if (registry && registry->HasComponent<NavigationAgentComponent>(entity)) {
        auto& agent = registry->GetComponent<NavigationAgentComponent>(entity);
        agent.currentPath.clear();
        agent.pathIndex = 0;
        if (registry->HasComponent<Scene::TransformComponent>(entity)) {
            agent.agentTarget = registry->GetComponent<Scene::TransformComponent>(entity).position;
        }
    }
}

void AIController::FollowPath(ECS::Registry* registry, ECS::Entity entity, const std::vector<glm::vec3>& path) {
    if (registry && registry->HasComponent<NavigationAgentComponent>(entity)) {
        auto& agent = registry->GetComponent<NavigationAgentComponent>(entity);
        agent.currentPath = path;
        agent.pathIndex = 0;
        if (!path.empty()) {
            agent.agentTarget = path.back();
        }
    }
}

void AIController::RotateTowardsTarget(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& target, float deltaTime) {
    if (!registry || !registry->HasComponent<Scene::TransformComponent>(entity)) return;
    auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
    
    glm::vec3 dir = target - tc.position;
    dir.y = 0.0f; // only rotate on Y axis
    if (glm::length(dir) > 1e-4f) {
        dir = glm::normalize(dir);
        float angle = glm::atan(dir.x, -dir.z); // angle around Y axis
        glm::quat targetRot = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
        float rotationSpeed = 5.0f; // radians/seconds
        tc.rotation = glm::slerp(tc.rotation, targetRot, glm::clamp(rotationSpeed * deltaTime, 0.0f, 1.0f));
    }
}

ECS::Entity AIController::AcquireTarget(ECS::Registry* registry, ECS::Entity entity) {
    if (!registry || !registry->HasComponent<PerceptionComponent>(entity)) return ECS::NULL_ENTITY;
    auto& perception = registry->GetComponent<PerceptionComponent>(entity);
    
    if (perception.perceivedStimuli.empty()) return ECS::NULL_ENTITY;
    
    ECS::Entity closest = ECS::NULL_ENTITY;
    float minDist = 1e9f;
    
    if (registry->HasComponent<Scene::TransformComponent>(entity)) {
        const auto& myTc = registry->GetComponent<Scene::TransformComponent>(entity);
        for (const auto& stim : perception.perceivedStimuli) {
            if (stim.isVisible) {
                float dist = glm::distance(myTc.position, stim.position);
                if (dist < minDist) {
                    minDist = dist;
                    closest = stim.entity;
                }
            }
        }
    }
    return closest;
}

} // namespace KumariEngine::AI
