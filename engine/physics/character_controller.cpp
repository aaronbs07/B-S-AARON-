#include "character_controller.hpp"
#include "terrain/terrain_manager.hpp"
#include "scene/scene_manager.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <variant>
#include <glm/gtx/norm.hpp>

namespace KumariEngine::Physics {

void CharacterController::Update(ECS::Registry* registry, ECS::Entity entity, 
                                 PhysicsComponent& pc, CharacterControllerComponent& cc, 
                                 PhysicsWorld& world, float dt) {
    (void)registry;
    
    // 1. Get current position from SceneNode
    auto& sceneMgr = Scene::SceneManager::Get();
    Scene::SceneNode* playerNode = nullptr;
    
    // Find player node matching entity
    struct NodeFinder {
        static Scene::SceneNode* Find(Scene::SceneNode* node, ECS::Entity ent) {
            if (node->GetEntity() == ent) return node;
            for (const auto& child : node->GetChildren()) {
                auto found = Find(child.get(), ent);
                if (found) return found;
            }
            return nullptr;
        }
    };
    if (sceneMgr.GetRootNode()) {
        sceneMgr.GetRootNode()->UpdateTransforms();
        playerNode = NodeFinder::Find(sceneMgr.GetRootNode(), entity);
    }
    
    if (!playerNode) return;
    glm::vec3 currentPos = glm::vec3(playerNode->GetWorldMatrix()[3]);

    // Ensure collider is Capsule
    if (pc.collider.type != ColliderType::Capsule) return;
    auto* cap = std::get_if<Capsule>(&pc.collider.shape);
    if (!cap) return;

    // 2. Compute kinematic movement velocities
    // Walk vs Run vs Sprint state selection
    float speed = cc.walkSpeed;
    if (cc.state == MovementState::Running) speed = cc.runSpeed;
    if (cc.state == MovementState::Sprinting) speed = cc.sprintSpeed;

    // Apply gravity
    if (cc.isGrounded) {
        cc.verticalVelocity = 0.0f;
        if (cc.requestJump) {
            cc.verticalVelocity = cc.jumpForce;
            cc.isGrounded = false;
            cc.requestJump = false;
            cc.state = MovementState::Jumping;
        } else {
            if (glm::length2(cc.moveDirection) > 1e-4f) {
                cc.state = (speed == cc.sprintSpeed) ? MovementState::Sprinting :
                           (speed == cc.runSpeed) ? MovementState::Running : MovementState::Walking;
            } else {
                cc.state = MovementState::Idle;
            }
        }
    } else {
        cc.verticalVelocity += world.GetGravity().y * cc.gravityMultiplier * dt;
        cc.state = (cc.verticalVelocity > 0.0f) ? MovementState::Jumping : MovementState::Falling;
    }

    // Combine movement vectors
    glm::vec3 velocity = cc.moveDirection * speed;
    velocity.y = cc.verticalVelocity;
    pc.velocity = velocity;

    glm::vec3 displacement = velocity * dt;

    // 3. Sliding plane collision resolution loop (Up to 4 iterations)
    cc.isGrounded = false; // Query again this frame
    constexpr int maxIterations = 4;
    
    for (int iter = 0; iter < maxIterations; ++iter) {
        if (glm::length2(displacement) < 1e-6f) break;

        glm::vec3 nextPos = currentPos + displacement;

        // Perform spatial query
        AABB capAABB;
        float totalHalf = cap->halfHeight + cap->radius;
        capAABB.min = nextPos - glm::vec3(cap->radius, totalHalf, cap->radius);
        capAABB.max = nextPos + glm::vec3(cap->radius, totalHalf, cap->radius);

        uint32_t neighbors[128];
        uint32_t neighborCount = world.GetSpatialGrid().Query(capAABB, neighbors, 128, entity);

        // Find the deepest collision contact
        ContactPoint deepestContact;
        bool hasCollision = false;
        deepestContact.penetration = -1.0f;

        // Check Terrain collision
        ContactPoint terrainContact;
        Capsule worldCap = *cap;
        worldCap.center = nextPos;
        
        if (world.CheckTerrainCollision(pc.collider, nextPos, terrainContact)) {
            deepestContact = terrainContact;
            hasCollision = true;
        }

        // Check other rigidbodies
        for (uint32_t k = 0; k < neighborCount; ++k) {
            ECS::Entity neighborEnt = neighbors[k];
            auto& npc = registry->GetComponent<PhysicsComponent>(neighborEnt);
            
            // Resolve node position
            struct NodeFinder {
                static Scene::SceneNode* Find(Scene::SceneNode* node, ECS::Entity ent) {
                    if (node->GetEntity() == ent) return node;
                    for (const auto& child : node->GetChildren()) {
                        auto found = Find(child.get(), ent);
                        if (found) return found;
                    }
                    return nullptr;
                }
            };
            Scene::SceneNode* neighborNode = NodeFinder::Find(sceneMgr.GetRootNode(), neighborEnt);
            glm::vec3 neighborPos = neighborNode ? glm::vec3(neighborNode->GetWorldMatrix()[3]) : glm::vec3(0.0f);

            // Filter layers
            if ((pc.collisionLayer & npc.collisionMask) == 0 || (npc.collisionLayer & pc.collisionMask) == 0) continue;

            ContactPoint colContact;
            bool hit = false;
            
            if (npc.collider.type == ColliderType::Sphere) {
                if (auto* s = std::get_if<Sphere>(&npc.collider.shape)) {
                    Sphere worldS = *s;
                    worldS.center += neighborPos;
                    hit = PhysicsWorld::CheckCapsuleSphere(worldCap, worldS, colContact);
                }
            } else if (npc.collider.type == ColliderType::Capsule) {
                if (auto* c = std::get_if<Capsule>(&npc.collider.shape)) {
                    Capsule worldC = *c;
                    worldC.center += neighborPos;
                    hit = PhysicsWorld::CheckCapsuleCapsule(worldCap, worldC, colContact);
                }
            } else if (npc.collider.type == ColliderType::AABB) {
                if (auto* a = std::get_if<AABB>(&npc.collider.shape)) {
                    AABB worldA = *a;
                    worldA.min += neighborPos;
                    worldA.max += neighborPos;
                    hit = PhysicsWorld::CheckCapsuleAABB(worldCap, worldA, colContact);
                }
            }

            if (hit && colContact.penetration > deepestContact.penetration) {
                deepestContact = colContact;
                hasCollision = true;
            }
        }

        if (!hasCollision) {
            currentPos = nextPos;
            break;
        }

        // Resolve penetration
        currentPos = nextPos + deepestContact.normal * (deepestContact.penetration + 0.001f);

        // Ground detection (Walkable slopes)
        if (deepestContact.normal.y > 0.707f) { // normal.y > cos(45 deg)
            cc.isGrounded = true;
            cc.verticalVelocity = 0.0f;
        }
        
        // Ceiling detection
        if (deepestContact.normal.y < -0.707f) {
            cc.verticalVelocity = 0.0f;
        }

        // Slide remaining displacement along the contact tangent plane
        displacement = displacement - deepestContact.normal * glm::dot(displacement, deepestContact.normal);
    }

    // 4. Force ground snap to procedural terrain
    float terrainH = Terrain::TerrainManager::Get().GetHeightAt(currentPos.x, currentPos.z);
    float capsuleBottom = currentPos.y - cap->halfHeight - cap->radius;
    
    if (capsuleBottom <= terrainH + 0.02f) {
        currentPos.y = terrainH + cap->halfHeight + cap->radius;
        cc.isGrounded = true;
        cc.verticalVelocity = 0.0f;
    }

    // 5. Update SceneNode position
    if (playerNode->GetParent()) {
        glm::mat4 invParent = glm::inverse(playerNode->GetParent()->GetWorldMatrix());
        playerNode->SetLocalPosition(glm::vec3(invParent * glm::vec4(currentPos, 1.0f)));
    } else {
        playerNode->SetLocalPosition(currentPos);
    }
    playerNode->UpdateTransforms(playerNode->GetParent() ? playerNode->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
}

} // namespace KumariEngine::Physics
