#pragma once
#include <glm/glm.hpp>
#include "physics_types.hpp"
#include "physics_components.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Physics {

class Raycast {
public:
    // Convert screen coordinates to world space Ray.
    static Ray ViewportPointToRay(float mouseX, float mouseY, 
                                  float viewportWidth, float viewportHeight,
                                  const glm::mat4& viewMatrix, 
                                  const glm::mat4& projMatrix);

    // Dynamic raycast queries
    static bool RaycastSphere(const Ray& ray, const Sphere& sphere, RaycastHit& outHit);
    static bool RaycastAABB(const Ray& ray, const AABB& aabb, RaycastHit& outHit);
    static bool RaycastCapsule(const Ray& ray, const Capsule& capsule, RaycastHit& outHit);

    // Raycasts against the procedural terrain heightfield
    static bool RaycastTerrain(const Ray& ray, float maxDistance, RaycastHit& outHit);

    // Raycast against all physics entities in the registry
    static bool RaycastWorld(ECS::Registry* registry, const Ray& ray, float maxDistance, 
                             RaycastHit& outHit, uint32_t ignoreEntity = 0);
};

} // namespace KumariEngine::Physics
