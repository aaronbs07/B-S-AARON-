#include "PlayerState.hpp"
#include "GameplayComponents.hpp"

namespace KumariEngine::Gameplay {

PlayerState::PlayerState(ECS::Registry* registry, ECS::Entity entity)
    : m_registry(registry), m_entity(entity) {
}

std::string PlayerState::GetPlayerName() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerStateComponent>(m_entity).playerName;
    }
    return "Player";
}

void PlayerState::SetPlayerName(const std::string& name) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).playerName = name;
    }
}

uint32_t PlayerState::GetPeerId() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerStateComponent>(m_entity).peerId;
    }
    return 0;
}

void PlayerState::SetPeerId(uint32_t peerId) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).peerId = peerId;
    }
}

float PlayerState::GetScore() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerStateComponent>(m_entity).score;
    }
    return 0.0f;
}

void PlayerState::SetScore(float score) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).score = score;
    }
}

void PlayerState::AddScore(float amount) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).score += amount;
    }
}

int PlayerState::GetTeamId() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerStateComponent>(m_entity).teamId;
    }
    return 0;
}

void PlayerState::SetTeamId(int teamId) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).teamId = teamId;
    }
}

float PlayerState::GetPing() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        return m_registry->GetComponent<PlayerStateComponent>(m_entity).ping;
    }
    return 0.0f;
}

void PlayerState::SetPing(float ping) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<PlayerStateComponent>(m_entity)) {
        m_registry->GetComponent<PlayerStateComponent>(m_entity).ping = ping;
    }
}

} // namespace KumariEngine::Gameplay
