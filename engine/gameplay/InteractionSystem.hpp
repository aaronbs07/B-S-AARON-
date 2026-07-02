#pragma once
#include "ecs/ecs.hpp"
#include "physics/physics_types.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

namespace KumariEngine::Gameplay {

struct InteractableComponent {
    std::string prompt = "Interact";
    float distance = 3.0f;
    bool isInteractable = true;
    std::string interactionType = "Use"; // "Use", "Pickup"
    std::string targetData;             // e.g. itemId for Pickups, or generic parameters
    std::string onInteractLua;          // Lua function name or code to execute on interaction
    std::function<void(ECS::Entity)> onInteracted = nullptr; // C++ callback: (instigator)
};

struct TriggerVolumeComponent {
    Physics::ColliderType type = Physics::ColliderType::None;
    Physics::Collider collider;
    std::unordered_set<ECS::Entity> overlappingEntities;
    std::string onEnterLua;
    std::string onExitLua;
};

class InteractionSystem {
public:
    static InteractionSystem& Get() {
        static InteractionSystem instance;
        return instance;
    }

    InteractionSystem(const InteractionSystem&) = delete;
    InteractionSystem& operator=(const InteractionSystem&) = delete;

    void Tick(ECS::Registry* registry, ECS::Entity playerEntity, float deltaTime);
    bool Interact(ECS::Registry* registry, ECS::Entity player, ECS::Entity target);

private:
    InteractionSystem() = default;
    ~InteractionSystem() = default;
};

} // namespace KumariEngine::Gameplay
