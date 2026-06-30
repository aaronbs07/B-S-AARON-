#include "physics_system.hpp"
#include "debug_renderer.hpp"
#include "scene/scene_manager.hpp"
#include <unordered_map>
#include <variant>

namespace KumariEngine::Physics {

PhysicsSystem::PhysicsSystem(float cellSize)
    : m_world(cellSize) {
}

void PhysicsSystem::Update(ECS::Registry* registry, float dt) {
    // 1. Update Kinematic Character Controllers
    registry->Each<PhysicsComponent, CharacterControllerComponent>([&](ECS::Entity entity, PhysicsComponent& pc, CharacterControllerComponent& cc) {
        m_controller.Update(registry, entity, pc, cc, m_world, dt);
    });

    // 2. Advance Physics World Simulation Step
    m_world.Step(registry, dt);

    // 3. Submit debug shapes if visual debugging is enabled
    auto& dbg = PhysicsDebugRenderer::Get();
    if (dbg.IsEnabled()) {
        dbg.Clear();

        auto& sceneMgr = Scene::SceneManager::Get();
        std::unordered_map<ECS::Entity, glm::vec3> entityPositions;
        
        struct CachePositions {
            static void Run(Scene::SceneNode* node, std::unordered_map<ECS::Entity, glm::vec3>& outCache) {
                if (node->GetEntity() != ECS::NULL_ENTITY) {
                    outCache[node->GetEntity()] = glm::vec3(node->GetWorldMatrix()[3]);
                }
                for (const auto& child : node->GetChildren()) {
                    Run(child.get(), outCache);
                }
            }
        };
        if (sceneMgr.GetRootNode()) {
            CachePositions::Run(sceneMgr.GetRootNode(), entityPositions);
        }

        registry->Each<PhysicsComponent>([&](ECS::Entity entity, PhysicsComponent& pc) {
            if (pc.collider.type == ColliderType::None) return;

            glm::vec3 pos = entityPositions.contains(entity) ? entityPositions[entity] : glm::vec3(0.0f);
            
            // Choose color based on rigid body classification
            glm::vec3 color = glm::vec3(0.2f, 0.8f, 0.2f); // Default dynamic: Green
            if (pc.bodyType == BodyType::Static) {
                color = glm::vec3(0.6f, 0.6f, 0.6f); // Static: Grey
            } else if (pc.bodyType == BodyType::Kinematic) {
                color = glm::vec3(0.0f, 0.8f, 0.8f); // Kinematic: Cyan
            }

            if (pc.collider.isTrigger) {
                color = glm::vec3(1.0f, 0.5f, 0.0f); // Trigger: Orange
            }

            if (pc.collider.type == ColliderType::Sphere) {
                if (auto* sphere = std::get_if<Sphere>(&pc.collider.shape)) {
                    dbg.DrawSphere(pos + sphere->center, sphere->radius, color);
                }
            } else if (pc.collider.type == ColliderType::AABB) {
                if (auto* aabb = std::get_if<AABB>(&pc.collider.shape)) {
                    dbg.DrawAABB(pos + aabb->min, pos + aabb->max, color);
                }
            } else if (pc.collider.type == ColliderType::Capsule) {
                if (auto* cap = std::get_if<Capsule>(&pc.collider.shape)) {
                    dbg.DrawCapsule(pos + cap->center, cap->halfHeight, cap->radius, color);
                }
            }
        });
    }
}

} // namespace KumariEngine::Physics
