#include "physics_world.hpp"
#include "terrain/terrain_manager.hpp"
#include "scene/scene_manager.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <variant>
#include <algorithm>
#include <glm/gtx/norm.hpp>

namespace KumariEngine::Physics {

// Helper to compute local AABB converted to World space
static AABB GetWorldAABB(const PhysicsComponent& pc, const glm::vec3& position) {
    AABB worldAABB;
    if (pc.collider.type == ColliderType::AABB) {
        const auto& aabb = std::get<1>(pc.collider.shape);
        worldAABB.min = position + aabb.min;
        worldAABB.max = position + aabb.max;
    } else if (pc.collider.type == ColliderType::Sphere) {
        const auto& sphere = std::get<2>(pc.collider.shape);
        worldAABB.min = position - glm::vec3(sphere.radius);
        worldAABB.max = position + glm::vec3(sphere.radius);
    } else if (pc.collider.type == ColliderType::Capsule) {
        const auto& cap = std::get<3>(pc.collider.shape);
        float totalHalfHeight = cap.halfHeight + cap.radius;
        worldAABB.min = position - glm::vec3(cap.radius, totalHalfHeight, cap.radius);
        worldAABB.max = position + glm::vec3(cap.radius, totalHalfHeight, cap.radius);
    } else {
        worldAABB.min = position - glm::vec3(0.5f);
        worldAABB.max = position + glm::vec3(0.5f);
    }
    return worldAABB;
}

PhysicsWorld::PhysicsWorld(float cellSize)
    : m_spatialGrid(cellSize) {
    m_queryResults.reserve(1024);
}

void PhysicsWorld::Step(ECS::Registry* registry, float dt) {
    // 1. Gather all active entities with PhysicsComponent
    auto entities = registry->View<PhysicsComponent>();
    uint32_t activeEntitiesCount = static_cast<uint32_t>(entities.size());
    if (activeEntitiesCount == 0) return;

    // Ensure broad-phase has sufficient node capacity
    m_spatialGrid.Resize(activeEntitiesCount);
    m_spatialGrid.Clear();

    // 2. Pre-integrate velocities (Apply gravity)
    for (ECS::Entity entity : entities) {
        auto& pc = registry->GetComponent<PhysicsComponent>(entity);
        if (pc.bodyType == BodyType::Dynamic) {
            // Apply gravity and accumulated force
            pc.velocity += (m_gravity + pc.forceAccum * pc.inverseMass) * dt;
            pc.forceAccum = glm::vec3(0.0f);
        }
    }

    // 3. Build Spatial Hash Grid (Broad-phase)
    auto& sceneMgr = Scene::SceneManager::Get();
    auto rootNode = sceneMgr.GetRootNode();

    // Pre-cache entity positions to avoid traversing hierarchy repeatedly
    std::unordered_map<ECS::Entity, glm::vec3> entityPositions;
    entityPositions.reserve(activeEntitiesCount);

    // Helper to find position of entity in Scene Graph
    struct Finder {
        static void CachePositions(Scene::SceneNode* node, std::unordered_map<ECS::Entity, glm::vec3>& outCache) {
            if (node->GetEntity() != ECS::NULL_ENTITY) {
                outCache[node->GetEntity()] = glm::vec3(node->GetWorldMatrix()[3]);
            }
            for (const auto& child : node->GetChildren()) {
                CachePositions(child.get(), outCache);
            }
        }
    };
    if (rootNode) {
        rootNode->UpdateTransforms();
        Finder::CachePositions(rootNode, entityPositions);
    }

    // Insert all colliders into Broad-phase
    for (ECS::Entity entity : entities) {
        const auto& pc = registry->GetComponent<PhysicsComponent>(entity);
        glm::vec3 pos = entityPositions.contains(entity) ? entityPositions[entity] : glm::vec3(0.0f);
        AABB worldAABB = GetWorldAABB(pc, pos);
        m_spatialGrid.Insert(entity, worldAABB);
    }

    // 4. Narrow-phase & Resolution
    for (uint32_t i = 0; i < activeEntitiesCount; ++i) {
        ECS::Entity entityA = entities[i];
        auto& pcA = registry->GetComponent<PhysicsComponent>(entityA);
        glm::vec3& posA = entityPositions[entityA];
        AABB aabbA = GetWorldAABB(pcA, posA);

        // A. Collide against Terrain
        ContactPoint terrainContact;
        if (!pcA.collider.isTrigger && CheckTerrainCollision(pcA.collider, posA, terrainContact)) {
            // Resolve against static terrain (terrain has infinite mass)
            posA += terrainContact.normal * terrainContact.penetration;
            
            // Adjust velocity to stop downward movement
            float velAlongNormal = glm::dot(pcA.velocity, terrainContact.normal);
            if (velAlongNormal < 0.0f) {
                // Apply bounce and friction
                glm::vec3 relativeVel = pcA.velocity;
                float j = -(1.0f + pcA.restitution) * velAlongNormal;
                pcA.velocity += terrainContact.normal * j;

                // Friction
                glm::vec3 tangent = relativeVel - terrainContact.normal * velAlongNormal;
                if (glm::length2(tangent) > 1e-6f) {
                    tangent = glm::normalize(tangent);
                    float jt = -glm::dot(relativeVel, tangent);
                    float maxFriction = j * pcA.friction;
                    jt = std::clamp(jt, -maxFriction, maxFriction);
                    pcA.velocity += tangent * jt;
                }
            }
        }

        // B. Query Broad-phase for other colliders
        m_queryResults.resize(1024);
        uint32_t overlapCount = m_spatialGrid.Query(aabbA, m_queryResults.data(), 1024, entityA);
        m_queryResults.resize(overlapCount);

        for (uint32_t k = 0; k < overlapCount; ++k) {
            ECS::Entity entityB = m_queryResults[k];
            
            // Avoid double processing by ordering
            if (entityA >= entityB) continue;

            auto& pcB = registry->GetComponent<PhysicsComponent>(entityB);
            glm::vec3& posB = entityPositions[entityB];

            // Filtering
            if ((pcA.collisionLayer & pcB.collisionMask) == 0 || 
                (pcB.collisionLayer & pcA.collisionMask) == 0) {
                continue;
            }

            // Execute narrow phase based on shape pairs
            ContactPoint contact;
            bool hasCollision = false;

            if (pcA.collider.type == ColliderType::Sphere && pcB.collider.type == ColliderType::Sphere) {
                Sphere s1 = std::get<2>(pcA.collider.shape);
                s1.center += posA;
                Sphere s2 = std::get<2>(pcB.collider.shape);
                s2.center += posB;
                hasCollision = CheckSphereSphere(s1, s2, contact);
            } else if (pcA.collider.type == ColliderType::AABB && pcB.collider.type == ColliderType::AABB) {
                AABB boxA = std::get<1>(pcA.collider.shape);
                boxA.min += posA; boxA.max += posA;
                AABB boxB = std::get<1>(pcB.collider.shape);
                boxB.min += posB; boxB.max += posB;
                hasCollision = CheckAABBAABB(boxA, boxB, contact);
            } else if (pcA.collider.type == ColliderType::Sphere && pcB.collider.type == ColliderType::AABB) {
                Sphere s = std::get<2>(pcA.collider.shape);
                s.center += posA;
                AABB box = std::get<1>(pcB.collider.shape);
                box.min += posB; box.max += posB;
                hasCollision = CheckSphereAABB(s, box, contact);
            } else if (pcA.collider.type == ColliderType::AABB && pcB.collider.type == ColliderType::Sphere) {
                Sphere s = std::get<2>(pcB.collider.shape);
                s.center += posB;
                AABB box = std::get<1>(pcA.collider.shape);
                box.min += posA; box.max += posA;
                hasCollision = CheckSphereAABB(s, box, contact);
                if (hasCollision) {
                    contact.normal = -contact.normal;
                }
            } else if (pcA.collider.type == ColliderType::Capsule && pcB.collider.type == ColliderType::Sphere) {
                Capsule c = std::get<3>(pcA.collider.shape);
                c.center += posA;
                Sphere s = std::get<2>(pcB.collider.shape);
                s.center += posB;
                hasCollision = CheckCapsuleSphere(c, s, contact);
            } else if (pcA.collider.type == ColliderType::Sphere && pcB.collider.type == ColliderType::Capsule) {
                Capsule c = std::get<3>(pcB.collider.shape);
                c.center += posB;
                Sphere s = std::get<2>(pcA.collider.shape);
                s.center += posA;
                hasCollision = CheckCapsuleSphere(c, s, contact);
                if (hasCollision) {
                    contact.normal = -contact.normal;
                }
            } else if (pcA.collider.type == ColliderType::Capsule && pcB.collider.type == ColliderType::Capsule) {
                Capsule c1 = std::get<3>(pcA.collider.shape);
                c1.center += posA;
                Capsule c2 = std::get<3>(pcB.collider.shape);
                c2.center += posB;
                hasCollision = CheckCapsuleCapsule(c1, c2, contact);
            } else if (pcA.collider.type == ColliderType::Capsule && pcB.collider.type == ColliderType::AABB) {
                Capsule c = std::get<3>(pcA.collider.shape);
                c.center += posA;
                AABB box = std::get<1>(pcB.collider.shape);
                box.min += posB; box.max += posB;
                hasCollision = CheckCapsuleAABB(c, box, contact);
            } else if (pcA.collider.type == ColliderType::AABB && pcB.collider.type == ColliderType::Capsule) {
                Capsule c = std::get<3>(pcB.collider.shape);
                c.center += posB;
                AABB box = std::get<1>(pcA.collider.shape);
                box.min += posA; box.max += posA;
                hasCollision = CheckCapsuleAABB(c, box, contact);
                if (hasCollision) {
                    contact.normal = -contact.normal;
                }
            }

            if (hasCollision) {
                if (pcA.collider.isTrigger || pcB.collider.isTrigger) {
                    // Overlap trigger event log
                    Core::Logger::Info("PhysicsWorld", "Trigger overlapping detected between Entities %d and %d", entityA, entityB);
                } else {
                    ResolveCollision(pcA, pcB, posA, posB, contact, dt);
                }
            }
        }
    }

    // 5. Integrate positions and update transforms in the Scene graph
    struct Applier {
        static void ApplyPositions(Scene::SceneNode* node, const std::unordered_map<ECS::Entity, glm::vec3>& inCache, ECS::Registry* reg, float dt) {
            if (node->GetEntity() != ECS::NULL_ENTITY && inCache.contains(node->GetEntity())) {
                const glm::vec3& cachedPos = inCache.at(node->GetEntity());
                auto& pc = reg->GetComponent<PhysicsComponent>(node->GetEntity());
                
                glm::vec3 nextPos = cachedPos;
                if (pc.bodyType == BodyType::Dynamic) {
                    nextPos = cachedPos + pc.velocity * dt;
                } else if (pc.bodyType == BodyType::Kinematic) {
                    nextPos = cachedPos + pc.velocity * dt;
                }
                
                if (node->GetParent()) {
                    glm::mat4 invParent = glm::inverse(node->GetParent()->GetWorldMatrix());
                    node->SetLocalPosition(glm::vec3(invParent * glm::vec4(nextPos, 1.0f)));
                } else {
                    node->SetLocalPosition(nextPos);
                }
                node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
            }
            for (const auto& child : node->GetChildren()) {
                ApplyPositions(child.get(), inCache, reg, dt);
            }
        }
    };
    if (rootNode) {
        Applier::ApplyPositions(rootNode, entityPositions, registry, dt);
    }
}

void PhysicsWorld::ResolveCollision(PhysicsComponent& a, PhysicsComponent& b, 
                                    glm::vec3& posA, glm::vec3& posB, 
                                    const ContactPoint& contact, float dt) {
    (void)dt;
    // 1. Linear penetration correction (Positional Correction to prevent sinking)
    float totalInvMass = a.inverseMass + b.inverseMass;
    if (totalInvMass <= 0.0f) return;

    constexpr float percent = 0.4f; // penetration percentage to correct per frame (slop correction)
    constexpr float slop = 0.01f;   // penetration threshold
    glm::vec3 correction = glm::max(contact.penetration - slop, 0.0f) / totalInvMass * percent * contact.normal;
    
    if (a.bodyType == BodyType::Dynamic) posA += correction * a.inverseMass;
    if (b.bodyType == BodyType::Dynamic) posB -= correction * b.inverseMass;

    // 2. Apply velocities response impulse
    ApplyImpulse(a, b, contact);
}

void PhysicsWorld::ApplyImpulse(PhysicsComponent& a, PhysicsComponent& b, const ContactPoint& contact) {
    glm::vec3 relativeVel = a.velocity - b.velocity;
    float velAlongNormal = glm::dot(relativeVel, contact.normal);

    // If separating velocities, do nothing
    if (velAlongNormal > 0.0f) return;

    float totalInvMass = a.inverseMass + b.inverseMass;
    float e = std::min(a.restitution, b.restitution);

    // Compute impulse magnitude scalar
    float j = -(1.0f + e) * velAlongNormal;
    j /= totalInvMass;

    // Apply impulse vector
    glm::vec3 impulse = j * contact.normal;
    if (a.bodyType == BodyType::Dynamic) a.velocity += impulse * a.inverseMass;
    if (b.bodyType == BodyType::Dynamic) b.velocity -= impulse * b.inverseMass;

    // Friction impulse
    relativeVel = a.velocity - b.velocity;
    glm::vec3 tangent = relativeVel - glm::dot(relativeVel, contact.normal) * contact.normal;
    if (glm::length2(tangent) > 1e-6f) {
        tangent = glm::normalize(tangent);
        float jt = -glm::dot(relativeVel, tangent);
        jt /= totalInvMass;

        float mu = (a.friction + b.friction) * 0.5f;
        float maxFriction = j * mu;
        jt = std::clamp(jt, -maxFriction, maxFriction);

        glm::vec3 frictionImpulse = jt * tangent;
        if (a.bodyType == BodyType::Dynamic) a.velocity += frictionImpulse * a.inverseMass;
        if (b.bodyType == BodyType::Dynamic) b.velocity -= frictionImpulse * b.inverseMass;
    }
}

void PhysicsWorld::ClosestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, glm::vec3& outPoint) {
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq <= 1e-6f) {
        outPoint = a;
        return;
    }
    float t = glm::dot(p - a, ab) / abLenSq;
    t = glm::clamp(t, 0.0f, 1.0f);
    outPoint = a + t * ab;
}

void PhysicsWorld::ClosestPointBetweenSegments(const glm::vec3& p1, const glm::vec3& q1, 
                                                const glm::vec3& p2, const glm::vec3& q2, 
                                                glm::vec3& outC1, glm::vec3& outC2) {
    glm::vec3 d1 = q1 - p1;
    glm::vec3 d2 = q2 - p2;
    glm::vec3 r = p1 - p2;
    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1e-6f && e <= 1e-6f) {
        outC1 = p1;
        outC2 = p2;
        return;
    }
    if (a <= 1e-6f) {
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(d1, r);
        if (e <= 1e-6f) {
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;
            if (denom != 0.0f) {
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    outC1 = p1 + d1 * s;
    outC2 = p2 + d2 * t;
}

bool PhysicsWorld::CheckSphereSphere(const Sphere& s1, const Sphere& s2, ContactPoint& outContact) {
    glm::vec3 dir = s1.center - s2.center;
    float distSq = glm::dot(dir, dir);
    float sumRadius = s1.radius + s2.radius;
    if (distSq > sumRadius * sumRadius) return false;

    float dist = std::sqrt(distSq);
    if (dist > 1e-6f) {
        outContact.normal = dir / dist;
    } else {
        outContact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    outContact.penetration = sumRadius - dist;
    outContact.position = s2.center + outContact.normal * s2.radius;
    return true;
}

bool PhysicsWorld::CheckAABBAABB(const AABB& a, const AABB& b, ContactPoint& outContact) {
    float overlapX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    if (overlapX <= 0.0f) return false;

    float overlapY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    if (overlapY <= 0.0f) return false;

    float overlapZ = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    if (overlapZ <= 0.0f) return false;

    if (overlapX < overlapY && overlapX < overlapZ) {
        outContact.penetration = overlapX;
        float sign = (a.min.x + a.max.x < b.min.x + b.max.x) ? -1.0f : 1.0f;
        outContact.normal = glm::vec3(sign, 0.0f, 0.0f);
    } else if (overlapY < overlapZ) {
        outContact.penetration = overlapY;
        float sign = (a.min.y + a.max.y < b.min.y + b.max.y) ? -1.0f : 1.0f;
        outContact.normal = glm::vec3(0.0f, sign, 0.0f);
    } else {
        outContact.penetration = overlapZ;
        float sign = (a.min.z + a.max.z < b.min.z + b.max.z) ? -1.0f : 1.0f;
        outContact.normal = glm::vec3(0.0f, 0.0f, sign);
    }

    outContact.position = (glm::max(a.min, b.min) + glm::min(a.max, b.max)) * 0.5f;
    return true;
}

bool PhysicsWorld::CheckSphereAABB(const Sphere& s, const AABB& a, ContactPoint& outContact) {
    glm::vec3 clamped = glm::clamp(s.center, a.min, a.max);
    glm::vec3 dir = s.center - clamped;
    float distSq = glm::dot(dir, dir);

    if (distSq > s.radius * s.radius) return false;

    float dist = std::sqrt(distSq);
    if (dist > 1e-6f) {
        outContact.normal = dir / dist;
        outContact.penetration = s.radius - dist;
    } else {
        float minDist = s.center.x - a.min.x;
        outContact.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
        
        if (a.max.x - s.center.x < minDist) {
            minDist = a.max.x - s.center.x;
            outContact.normal = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        if (s.center.y - a.min.y < minDist) {
            minDist = s.center.y - a.min.y;
            outContact.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        if (a.max.y - s.center.y < minDist) {
            minDist = a.max.y - s.center.y;
            outContact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (s.center.z - a.min.z < minDist) {
            minDist = s.center.z - a.min.z;
            outContact.normal = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        if (a.max.z - s.center.z < minDist) {
            minDist = a.max.z - s.center.z;
            outContact.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        outContact.penetration = s.radius + minDist;
    }
    outContact.position = clamped;
    return true;
}

bool PhysicsWorld::CheckCapsuleSphere(const Capsule& c, const Sphere& s, ContactPoint& outContact) {
    glm::vec3 segmentA = c.center - glm::vec3(0.0f, c.halfHeight, 0.0f);
    glm::vec3 segmentB = c.center + glm::vec3(0.0f, c.halfHeight, 0.0f);
    glm::vec3 closest;
    ClosestPointOnSegment(s.center, segmentA, segmentB, closest);

    Sphere sTemp{closest, c.radius};
    return CheckSphereSphere(sTemp, s, outContact);
}

bool PhysicsWorld::CheckCapsuleCapsule(const Capsule& c1, const Capsule& c2, ContactPoint& outContact) {
    glm::vec3 segmentA1 = c1.center - glm::vec3(0.0f, c1.halfHeight, 0.0f);
    glm::vec3 segmentB1 = c1.center + glm::vec3(0.0f, c1.halfHeight, 0.0f);

    glm::vec3 segmentA2 = c2.center - glm::vec3(0.0f, c2.halfHeight, 0.0f);
    glm::vec3 segmentB2 = c2.center + glm::vec3(0.0f, c2.halfHeight, 0.0f);

    glm::vec3 closest1, closest2;
    ClosestPointBetweenSegments(segmentA1, segmentB1, segmentA2, segmentB2, closest1, closest2);

    Sphere s1{closest1, c1.radius};
    Sphere s2{closest2, c2.radius};
    return CheckSphereSphere(s1, s2, outContact);
}

bool PhysicsWorld::CheckCapsuleAABB(const Capsule& c, const AABB& a, ContactPoint& outContact) {
    glm::vec3 segmentA = c.center - glm::vec3(0.0f, c.halfHeight, 0.0f);
    glm::vec3 segmentB = c.center + glm::vec3(0.0f, c.halfHeight, 0.0f);
    
    glm::vec3 aabbCenter = (a.min + a.max) * 0.5f;
    glm::vec3 closest;
    ClosestPointOnSegment(aabbCenter, segmentA, segmentB, closest);

    Sphere sTemp{closest, c.radius};
    return CheckSphereAABB(sTemp, a, outContact);
}

glm::vec3 PhysicsWorld::GetTerrainNormal(float x, float z) const {
    float eps = 0.1f;
    auto& tm = Terrain::TerrainManager::Get();
    float hL = tm.GetHeightAt(x - eps, z);
    float hR = tm.GetHeightAt(x + eps, z);
    float hD = tm.GetHeightAt(x, z - eps);
    float hU = tm.GetHeightAt(x, z + eps);
    
    // Gradient normal
    return glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
}

bool PhysicsWorld::CheckTerrainCollision(const Collider& col, const glm::vec3& pos, ContactPoint& outContact) const {
    if (col.type == ColliderType::None) return false;
    
    auto& tm = Terrain::TerrainManager::Get();
    float height = tm.GetHeightAt(pos.x, pos.z);

    if (col.type == ColliderType::Sphere) {
        const auto& sphere = std::get<2>(col.shape);
        float bottom = pos.y - sphere.radius;
        if (bottom < height) {
            outContact.penetration = height - bottom;
            outContact.normal = GetTerrainNormal(pos.x, pos.z);
            outContact.position = glm::vec3(pos.x, height, pos.z);
            return true;
        }
    } else if (col.type == ColliderType::Capsule) {
        const auto& cap = std::get<3>(col.shape);
        // Bottom sphere of the capsule
        float bottom = pos.y - cap.halfHeight - cap.radius;
        if (bottom < height) {
            outContact.penetration = height - bottom;
            outContact.normal = GetTerrainNormal(pos.x, pos.z);
            outContact.position = glm::vec3(pos.x, height, pos.z);
            return true;
        }
    } else if (col.type == ColliderType::AABB) {
        const auto& aabb = std::get<1>(col.shape);
        float bottom = pos.y + aabb.min.y;
        if (bottom < height) {
            outContact.penetration = height - bottom;
            outContact.normal = GetTerrainNormal(pos.x, pos.z);
            outContact.position = glm::vec3(pos.x, height, pos.z);
            return true;
        }
    }
    
    return false;
}

} // namespace KumariEngine::Physics
