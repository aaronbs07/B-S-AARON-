#include "PlayerController.hpp"
#include "GameplayComponents.hpp"

namespace KumariEngine::Gameplay {

PlayerController::PlayerController(ECS::Registry* registry, ECS::Entity entity)
    : m_registry(registry), m_entity(entity) {
}

uint32_t PlayerController::GetPeerId() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerControllerComponent>(m_entity).peerId;
    }
    return 0;
}

bool PlayerController::IsLocal() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerControllerComponent>(m_entity).isLocal;
    }
    return true;
}

ECS::Entity PlayerController::GetPossessedPawn() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerControllerComponent>(m_entity).possessedPawn;
    }
    return ECS::NULL_ENTITY;
}

void PlayerController::Possess(ECS::Entity pawn) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        m_registry->GetComponent<PlayerControllerComponent>(m_entity).possessedPawn = pawn;
    }
}

void PlayerController::Unpossess() {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        m_registry->GetComponent<PlayerControllerComponent>(m_entity).possessedPawn = ECS::NULL_ENTITY;
    }
}

ECS::Entity PlayerController::GetPlayerState() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerControllerComponent>(m_entity).playerStateEntity;
    }
    return ECS::NULL_ENTITY;
}

void PlayerController::SetPlayerState(ECS::Entity stateEntity) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerControllerComponent>(m_entity)) {
        m_registry->GetComponent<PlayerControllerComponent>(m_entity).playerStateEntity = stateEntity;
    }
}

} // namespace KumariEngine::Gameplay
