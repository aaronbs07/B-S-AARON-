#pragma once
#include "ecs/ecs.hpp"

namespace KumariEngine::Gameplay {

class PlayerController {
public:
    PlayerController(ECS::Registry* registry, ECS::Entity entity);
    virtual ~PlayerController() = default;

    ECS::Entity GetEntity() const { return m_entity; }

    uint32_t GetPeerId() const;
    bool IsLocal() const;

    ECS::Entity GetPossessedPawn() const;
    void Possess(ECS::Entity pawn);
    void Unpossess();

    ECS::Entity GetPlayerState() const;
    void SetPlayerState(ECS::Entity stateEntity);

protected:
    ECS::Registry* m_registry = nullptr;
    ECS::Entity m_entity = ECS::NULL_ENTITY;
};

} // namespace KumariEngine::Gameplay
