#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include "physics_types.hpp"
#include "physics_components.hpp"
#include "spatial_hash_grid.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Physics {

struct CollisionPair {
    ECS::Entity entityA;
    ECS::Entity entityB;

    bool operator==(const CollisionPair& other) const {
        return entityA == other.entityA && entityB == other.entityB;
    }
};

struct CollisionPairHash {
    size_t operator()(const CollisionPair& pair) const {
        return (static_cast<size_t>(pair.entityA) << 16) ^ static_cast<size_t>(pair.entityB);
    }
};

struct CollisionContactInfo {
    glm::vec3 normal;
    float penetration;
};

struct TriggerOverlapInfo {
    ECS::Entity triggerEntity;
    ECS::Entity otherEntity;
};

class PhysicsWorld {
public:
    PhysicsWorld(float cellSize = 2.0f);
    ~PhysicsWorld() = default;

    void Step(ECS::Registry* registry, float dt);

    void SetGravity(const glm::vec3& g) { m_gravity = g; }
    const glm::vec3& GetGravity() const { return m_gravity; }

    SpatialHashGrid& GetSpatialGrid() { return m_spatialGrid; }
    const SpatialHashGrid& GetSpatialGrid() const { return m_spatialGrid; }

    // Static narrow-phase helpers
    static bool CheckSphereSphere(const Sphere& s1, const Sphere& s2, ContactPoint& outContact);
    static bool CheckAABBAABB(const AABB& a, const AABB& b, ContactPoint& outContact);
    static bool CheckSphereAABB(const Sphere& s, const AABB& a, ContactPoint& outContact);
    static bool CheckCapsuleSphere(const Capsule& c, const Sphere& s, ContactPoint& outContact);
    static bool CheckCapsuleCapsule(const Capsule& c1, const Capsule& c2, ContactPoint& outContact);
    static bool CheckCapsuleAABB(const Capsule& c, const AABB& a, ContactPoint& outContact);

    // Segment helper
    static void ClosestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, glm::vec3& outPoint);
    static void ClosestPointBetweenSegments(const glm::vec3& p1, const glm::vec3& q1, 
                                            const glm::vec3& p2, const glm::vec3& q2, 
                                            glm::vec3& outC1, glm::vec3& outC2);

    // Terrain helpers
    glm::vec3 GetTerrainNormal(float x, float z) const;
    bool CheckTerrainCollision(const Collider& col, const glm::vec3& pos, ContactPoint& outContact) const;

private:
    void ResolveCollision(PhysicsComponent& a, PhysicsComponent& b, 
                          glm::vec3& posA, glm::vec3& posB, 
                          const ContactPoint& contact, float dt);
    
    void ApplyImpulse(PhysicsComponent& a, PhysicsComponent& b, const ContactPoint& contact);

    glm::vec3 m_gravity{0.0f, -9.81f, 0.0f};
    SpatialHashGrid m_spatialGrid;

    std::unordered_map<CollisionPair, CollisionContactInfo, CollisionPairHash> m_currentCollisions;
    std::unordered_map<CollisionPair, CollisionContactInfo, CollisionPairHash> m_prevCollisions;
    std::unordered_map<CollisionPair, TriggerOverlapInfo, CollisionPairHash> m_currentTriggers;
    std::unordered_map<CollisionPair, TriggerOverlapInfo, CollisionPairHash> m_prevTriggers;

    // Zero-allocation thread-local result caches
    mutable std::vector<uint32_t> m_queryResults;
};

} // namespace KumariEngine::Physics
