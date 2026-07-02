#include "gameplay/InteractionSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/InventorySystem.hpp"
#include "scene/transform_component.hpp"
#include "scripting/script_engine.hpp"
#include "core/logger.hpp"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace KumariEngine::Gameplay {

static void ExecuteLuaCallback(const std::string& code, ECS::Entity entity, ECS::Entity instigator) {
    if (code.empty()) return;
    lua_State* L = Scripting::ScriptEngine::Get().GetLuaState();
    if (!L) return;

    int top = lua_gettop(L);

    // Set globals for entity/instigator context
    lua_pushinteger(L, entity);
    lua_setglobal(L, "interaction_entity");
    lua_pushinteger(L, instigator);
    lua_setglobal(L, "interaction_instigator");

    int status = luaL_dostring(L, code.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        Core::Logger::Warning("InteractionSystem", "Lua interaction callback error: %s", err ? err : "unknown");
    }

    // Reset globals
    lua_pushnil(L);
    lua_setglobal(L, "interaction_entity");
    lua_pushnil(L);
    lua_setglobal(L, "interaction_instigator");

    lua_settop(L, top);
}

void InteractionSystem::Tick(ECS::Registry* registry, ECS::Entity playerEntity, float deltaTime) {
    (void)deltaTime;
    if (!registry || !registry->IsAlive(playerEntity) || !registry->HasComponent<Scene::TransformComponent>(playerEntity)) {
        return;
    }

    const auto& playerTrans = registry->GetComponent<Scene::TransformComponent>(playerEntity);
    glm::vec3 playerPos = playerTrans.position;

    // View trigger volumes
    auto triggerEntities = registry->View<TriggerVolumeComponent, Scene::TransformComponent>();
    for (auto ent : triggerEntities) {
        auto& trigger = registry->GetComponent<TriggerVolumeComponent>(ent);
        const auto& trans = registry->GetComponent<Scene::TransformComponent>(ent);
        glm::vec3 triggerWorldPos = trans.position;

        bool overlaps = false;
        
        // Overlap checks
        if (trigger.collider.type == Physics::ColliderType::Sphere) {
            float radius = 0.5f;
            if (std::holds_alternative<Physics::Sphere>(trigger.collider.shape)) {
                radius = std::get<Physics::Sphere>(trigger.collider.shape).radius;
            }
            overlaps = glm::distance(playerPos, triggerWorldPos) <= radius;
        } else if (trigger.collider.type == Physics::ColliderType::AABB) {
            Physics::AABB aabb;
            if (std::holds_alternative<Physics::AABB>(trigger.collider.shape)) {
                aabb = std::get<Physics::AABB>(trigger.collider.shape);
            }
            glm::vec3 worldMin = triggerWorldPos + aabb.min;
            glm::vec3 worldMax = triggerWorldPos + aabb.max;
            overlaps = (playerPos.x >= worldMin.x && playerPos.x <= worldMax.x &&
                        playerPos.y >= worldMin.y && playerPos.y <= worldMax.y &&
                        playerPos.z >= worldMin.z && playerPos.z <= worldMax.z);
        } else if (trigger.collider.type == Physics::ColliderType::Capsule) {
            Physics::Capsule cap;
            if (std::holds_alternative<Physics::Capsule>(trigger.collider.shape)) {
                cap = std::get<Physics::Capsule>(trigger.collider.shape);
            }
            glm::vec3 worldCenter = triggerWorldPos + cap.center;
            float distXZ = glm::distance(glm::vec2(playerPos.x, playerPos.z), glm::vec2(worldCenter.x, worldCenter.z));
            overlaps = (distXZ <= cap.radius && std::abs(playerPos.y - worldCenter.y) <= (cap.halfHeight + cap.radius));
        }

        if (overlaps) {
            if (trigger.overlappingEntities.find(playerEntity) == trigger.overlappingEntities.end()) {
                trigger.overlappingEntities.insert(playerEntity);
                ExecuteLuaCallback(trigger.onEnterLua, ent, playerEntity);
                Core::Logger::Info("InteractionSystem", "Player entered trigger zone %u", ent);
            }
        } else {
            if (trigger.overlappingEntities.find(playerEntity) != trigger.overlappingEntities.end()) {
                trigger.overlappingEntities.erase(playerEntity);
                ExecuteLuaCallback(trigger.onExitLua, ent, playerEntity);
                Core::Logger::Info("InteractionSystem", "Player exited trigger zone %u", ent);
            }
        }
    }
}

bool InteractionSystem::Interact(ECS::Registry* registry, ECS::Entity player, ECS::Entity target) {
    if (!registry || !registry->IsAlive(player) || !registry->IsAlive(target)) return false;
    if (!registry->HasComponent<InteractableComponent>(target)) return false;

    auto& interactable = registry->GetComponent<InteractableComponent>(target);
    if (!interactable.isInteractable) return false;

    // Check distance if both have transform components
    if (registry->HasComponent<Scene::TransformComponent>(player) && registry->HasComponent<Scene::TransformComponent>(target)) {
        glm::vec3 pPos = registry->GetComponent<Scene::TransformComponent>(player).position;
        glm::vec3 tPos = registry->GetComponent<Scene::TransformComponent>(target).position;
        if (glm::distance(pPos, tPos) > interactable.distance) {
            Core::Logger::Warning("InteractionSystem", "Interactable %u too far (distance %.2f > %.2f)", 
                                  target, glm::distance(pPos, tPos), interactable.distance);
            return false;
        }
    }

    if (interactable.interactionType == "Pickup") {
        if (!registry->HasComponent<InventoryComponent>(player)) return false;

        std::string itemId;
        uint32_t quantity = 1;

        if (registry->HasComponent<ItemComponent>(target)) {
            const auto& ic = registry->GetComponent<ItemComponent>(target);
            itemId = ic.itemId;
            quantity = ic.quantity;
        } else {
            itemId = interactable.targetData;
        }

        if (itemId.empty()) return false;

        auto& inv = registry->GetComponent<InventoryComponent>(player);
        if (inv.AddItem(itemId, quantity)) {
            // Track quest progress: Collect itemId
            QuestManager::Get().ProgressObjective(registry, player, "Collect", itemId, quantity);
            
            // Destroy target entity
            registry->DestroyEntity(target);
            Core::Logger::Info("InteractionSystem", "Player picked up item: %s x%d", itemId.c_str(), quantity);
            return true;
        } else {
            Core::Logger::Warning("InteractionSystem", "Pickup failed: Inventory is full.");
            return false;
        }
    } else if (interactable.interactionType == "Use") {
        // Execute C++ callback
        if (interactable.onInteracted) {
            interactable.onInteracted(player);
        }

        // Execute Lua callback
        ExecuteLuaCallback(interactable.onInteractLua, target, player);

        // Track quest progress: Interact target
        std::string targetId = interactable.targetData;
        if (targetId.empty()) {
            targetId = "Entity_" + std::to_string(target);
        }
        QuestManager::Get().ProgressObjective(registry, player, "Interact", targetId, 1);

        Core::Logger::Info("InteractionSystem", "Player interacted with entity %u", target);
        return true;
    }

    return false;
}

} // namespace KumariEngine::Gameplay
