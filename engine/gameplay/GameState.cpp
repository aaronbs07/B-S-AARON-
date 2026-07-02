#include "GameState.hpp"
#include "GameplayComponents.hpp"

namespace KumariEngine::Gameplay {

GameState::GameState(ECS::Registry* registry, ECS::Entity entity)
    : m_registry(registry), m_entity(entity) {
}

float GameState::GetElapsedTime() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        return m_registry->GetComponent<GameStateComponent>(m_entity).elapsedTime;
    }
    return 0.0f;
}

void GameState::SetElapsedTime(float time) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        m_registry->GetComponent<GameStateComponent>(m_entity).elapsedTime = time;
    }
}

bool GameState::IsMatchRunning() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        return m_registry->GetComponent<GameStateComponent>(m_entity).isMatchRunning;
    }
    return false;
}

void GameState::SetMatchRunning(bool running) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        m_registry->GetComponent<GameStateComponent>(m_entity).isMatchRunning = running;
    }
}

bool GameState::IsMatchOver() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        return m_registry->GetComponent<GameStateComponent>(m_entity).isMatchOver;
    }
    return false;
}

void GameState::SetMatchOver(bool over) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        m_registry->GetComponent<GameStateComponent>(m_entity).isMatchOver = over;
    }
}

int GameState::GetWinnerTeamId() const {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        return m_registry->GetComponent<GameStateComponent>(m_entity).winnerTeamId;
    }
    return -1;
}

void GameState::SetWinnerTeamId(int teamId) {
    if (m_registry && m_registry->IsAlive(m_entity) && m_registry->HasComponent<GameStateComponent>(m_entity)) {
        m_registry->GetComponent<GameStateComponent>(m_entity).winnerTeamId = teamId;
    }
}

} // namespace KumariEngine::Gameplay
