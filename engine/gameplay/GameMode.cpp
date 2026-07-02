#include "GameMode.hpp"
#include "GameState.hpp"
#include "PlayerController.hpp"
#include "PlayerState.hpp"
#include "GameplayComponents.hpp"
#include "SpawnManager.hpp"
#include "GameInstance.hpp"
#include "World.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Gameplay {

GameMode::GameMode(ECS::Registry* registry)
    : m_registry(registry) {
    if (m_registry) {
        m_registry->RegisterComponent<GameStateComponent>();
        m_registry->RegisterComponent<PlayerControllerComponent>();
        m_registry->RegisterComponent<PlayerStateComponent>();

        // Create global GameState entity if one doesn't exist
        ECS::Entity gameStateEntity = ECS::NULL_ENTITY;
        auto views = m_registry->View<GameStateComponent>();
        if (views.empty()) {
            gameStateEntity = m_registry->CreateEntity();
            m_registry->AddComponent<GameStateComponent>(gameStateEntity);
            Core::Logger::Info("GameMode", "Created global GameState entity.");
        } else {
            gameStateEntity = views[0];
        }
        m_gameState = std::make_shared<GameState>(m_registry, gameStateEntity);
    }
}

void GameMode::StartMatch() {
    if (m_gameState) {
        m_gameState->SetMatchRunning(true);
        m_gameState->SetMatchOver(false);
        m_gameState->SetElapsedTime(0.0f);
        m_gameState->SetWinnerTeamId(-1);
        Core::Logger::Info("GameMode", "Match started!");
    }
}

void GameMode::EndMatch() {
    if (m_gameState) {
        m_gameState->SetMatchRunning(false);
        m_gameState->SetMatchOver(true);
        Core::Logger::Info("GameMode", "Match ended.");
    }
}

void GameMode::OnPlayerConnected(uint32_t peerId) {
    if (!m_registry) return;

    Core::Logger::Info("GameMode", "Player connected: Peer ID %u", peerId);

    // Create player controller entity
    ECS::Entity pcEnt = m_registry->CreateEntity();
    auto& pc = m_registry->AddComponent<PlayerControllerComponent>(pcEnt);
    pc.peerId = peerId;
    pc.isLocal = (peerId == 0);

    // Create player state entity
    ECS::Entity psEnt = m_registry->CreateEntity();
    auto& ps = m_registry->AddComponent<PlayerStateComponent>(psEnt);
    ps.peerId = peerId;
    ps.playerName = "Player_" + std::to_string(peerId);

    // Link controller to state
    pc.playerStateEntity = psEnt;

    // Trigger spawn
    OnPlayerSpawn(pcEnt);
}

void GameMode::OnPlayerDisconnected(uint32_t peerId) {
    if (!m_registry) return;

    Core::Logger::Info("GameMode", "Player disconnected: Peer ID %u", peerId);

    // Destroy player's controller and state entities
    m_registry->Each<PlayerControllerComponent>([&](auto entity, PlayerControllerComponent& pc) {
        if (pc.peerId == peerId) {
            // Destroy possessed pawn
            if (m_registry->IsAlive(pc.possessedPawn)) {
                m_registry->DestroyEntity(pc.possessedPawn);
            }
            // Destroy player state entity
            if (m_registry->IsAlive(pc.playerStateEntity)) {
                m_registry->DestroyEntity(pc.playerStateEntity);
            }
            m_registry->DestroyEntity(entity);
        }
    });
}

void GameMode::OnPlayerSpawn(ECS::Entity playerController) {
    if (!m_registry || !m_registry->IsAlive(playerController)) return;

    Core::Logger::Info("GameMode", "Spawning player pawn.");
    auto world = GameFramework::GameInstance::Get().GetWorld();
    if (world && world->GetSpawnManager()) {
        world->GetSpawnManager()->SpawnPlayer(playerController, "Default");
    } else {
        Core::Logger::Warning("GameMode", "Cannot spawn player pawn: SpawnManager not available.");
    }
}

void GameMode::OnPlayerKilled(ECS::Entity playerController, ECS::Entity killerController) {
    if (!m_registry) return;

    Core::Logger::Info("GameMode", "Player pawn was killed!");

    if (m_registry->IsAlive(playerController) && m_registry->HasComponent<PlayerControllerComponent>(playerController)) {
        auto& pc = m_registry->GetComponent<PlayerControllerComponent>(playerController);
        
        // Destroy old possessed pawn
        if (m_registry->IsAlive(pc.possessedPawn)) {
            m_registry->DestroyEntity(pc.possessedPawn);
            pc.possessedPawn = ECS::NULL_ENTITY;
        }
    }

    if (m_registry->IsAlive(killerController) && m_registry->HasComponent<PlayerControllerComponent>(killerController)) {
        auto& killerPc = m_registry->GetComponent<PlayerControllerComponent>(killerController);
        if (m_registry->IsAlive(killerPc.playerStateEntity) && m_registry->HasComponent<PlayerStateComponent>(killerPc.playerStateEntity)) {
            auto& killerState = m_registry->GetComponent<PlayerStateComponent>(killerPc.playerStateEntity);
            killerState.score += 1.0f; // Increment score
        }
    }
}

void GameMode::Tick(float deltaTime) {
    if (!m_gameState) return;

    if (m_gameState->IsMatchRunning()) {
        float elapsed = m_gameState->GetElapsedTime() + deltaTime;
        m_gameState->SetElapsedTime(elapsed);
        if (elapsed >= matchDuration) {
            EndMatch();
        }
    }
}

std::vector<std::shared_ptr<PlayerController>> GameMode::GetPlayerControllers() const {
    std::vector<std::shared_ptr<PlayerController>> controllers;
    if (!m_registry) return controllers;
    m_registry->Each<PlayerControllerComponent>([&](auto entity, const PlayerControllerComponent&) {
        controllers.push_back(std::make_shared<PlayerController>(m_registry, entity));
    });
    return controllers;
}

std::vector<std::shared_ptr<PlayerState>> GameMode::GetPlayerStates() const {
    std::vector<std::shared_ptr<PlayerState>> states;
    if (!m_registry) return states;
    m_registry->Each<PlayerStateComponent>([&](auto entity, const PlayerStateComponent&) {
        states.push_back(std::make_shared<PlayerState>(m_registry, entity));
    });
    return states;
}

std::shared_ptr<PlayerController> GameMode::GetPlayerController(uint32_t peerId) const {
    if (!m_registry) return nullptr;
    std::shared_ptr<PlayerController> found = nullptr;
    m_registry->Each<PlayerControllerComponent>([&](auto entity, const PlayerControllerComponent& pcc) {
        if (pcc.peerId == peerId) {
            found = std::make_shared<PlayerController>(m_registry, entity);
        }
    });
    return found;
}

std::shared_ptr<PlayerState> GameMode::GetPlayerState(uint32_t peerId) const {
    if (!m_registry) return nullptr;
    std::shared_ptr<PlayerState> found = nullptr;
    m_registry->Each<PlayerStateComponent>([&](auto entity, const PlayerStateComponent& psc) {
        if (psc.peerId == peerId) {
            found = std::make_shared<PlayerState>(m_registry, entity);
        }
    });
    return found;
}

} // namespace KumariEngine::Gameplay
