#include "SpawnManager.hpp"
#include "GameplayComponents.hpp"
#include "scene/transform_component.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Gameplay {

SpawnManager::SpawnManager(ECS::Registry* registry)
    : m_registry(registry) {
    if (m_registry) {
        m_registry->RegisterComponent<SpawnPointComponent>();
        m_registry->RegisterComponent<Scene::TransformComponent>();
        m_registry->RegisterComponent<HealthComponent>();
        m_registry->RegisterComponent<TeamComponent>();
    }
}

ECS::Entity SpawnManager::CreateSpawnPoint(const std::string& group, const glm::vec3& position, const glm::quat& rotation) {
    if (!m_registry) return ECS::NULL_ENTITY;

    ECS::Entity spawnEnt = m_registry->CreateEntity();
    auto& sp = m_registry->AddComponent<SpawnPointComponent>(spawnEnt);
    sp.spawnGroup = group;
    sp.isEnabled = true;

    m_registry->AddComponent<Scene::TransformComponent>(spawnEnt, position, rotation, glm::vec3(1.0f));
    
    Core::Logger::Info("SpawnManager", "Spawn point created in group '%s' at (%.2f, %.2f, %.2f)", 
                       group.c_str(), position.x, position.y, position.z);
    
    return spawnEnt;
}

ECS::Entity SpawnManager::FindBestSpawnPoint(const std::string& spawnGroup) {
    if (!m_registry) return ECS::NULL_ENTITY;

    ECS::Entity bestPoint = ECS::NULL_ENTITY;
    
    m_registry->Each<SpawnPointComponent, Scene::TransformComponent>([&](auto entity, const SpawnPointComponent& sp, const Scene::TransformComponent&) {
        if (sp.isEnabled && sp.spawnGroup == spawnGroup) {
            bestPoint = entity; // Simple: pick first matching enabled spawn point
        }
    });

    // Fallback: search for any default or enabled spawn point if group not found
    if (bestPoint == ECS::NULL_ENTITY) {
        m_registry->Each<SpawnPointComponent, Scene::TransformComponent>([&](auto entity, const SpawnPointComponent& sp, const Scene::TransformComponent&) {
            if (sp.isEnabled) {
                bestPoint = entity;
            }
        });
    }

    return bestPoint;
}

ECS::Entity SpawnManager::SpawnPlayer(ECS::Entity playerController, const std::string& spawnGroup) {
    if (!m_registry || !m_registry->IsAlive(playerController)) return ECS::NULL_ENTITY;

    auto& pc = m_registry->GetComponent<PlayerControllerComponent>(playerController);

    // 1. Locate spawn point transform
    glm::vec3 spawnPos(0.0f);
    glm::quat spawnRot(1.0f, 0.0f, 0.0f, 0.0f);

    ECS::Entity spawnPoint = FindBestSpawnPoint(spawnGroup);
    if (spawnPoint != ECS::NULL_ENTITY) {
        const auto& tc = m_registry->GetComponent<Scene::TransformComponent>(spawnPoint);
        spawnPos = tc.position;
        spawnRot = tc.rotation;
    } else {
        Core::Logger::Warning("SpawnManager", "No active spawn point found. Spawning player at world origin.");
    }

    // 2. Instantiate player pawn
    ECS::Entity pawn = m_registry->CreateEntity();
    m_registry->AddComponent<Scene::TransformComponent>(pawn, spawnPos, spawnRot, glm::vec3(1.0f));
    
    auto& health = m_registry->AddComponent<HealthComponent>(pawn);
    health.currentHealth = 100.0f;
    health.maxHealth = 100.0f;

    auto& team = m_registry->AddComponent<TeamComponent>(pawn);
    if (m_registry->IsAlive(pc.playerStateEntity) && m_registry->HasComponent<PlayerStateComponent>(pc.playerStateEntity)) {
        auto& ps = m_registry->GetComponent<PlayerStateComponent>(pc.playerStateEntity);
        team.teamId = ps.teamId;
    }

    // Link controller to pawn
    pc.possessedPawn = pawn;

    // Set up death callback to trigger respawning
    health.onDeath = [this, playerController, spawnGroup]() {
        Core::Logger::Info("SpawnManager", "Player pawn death detected. Triggering respawn.");
        this->RespawnPlayer(playerController, spawnGroup);
    };

    Core::Logger::Info("SpawnManager", "Spawned player pawn entity %u for controller %u at (%.2f, %.2f, %.2f)", 
                       pawn, playerController, spawnPos.x, spawnPos.y, spawnPos.z);

    return pawn;
}

ECS::Entity SpawnManager::SpawnAI(const std::string& aiName, const std::string& spawnGroup) {
    if (!m_registry) return ECS::NULL_ENTITY;

    glm::vec3 spawnPos(0.0f);
    glm::quat spawnRot(1.0f, 0.0f, 0.0f, 0.0f);

    ECS::Entity spawnPoint = FindBestSpawnPoint(spawnGroup);
    if (spawnPoint != ECS::NULL_ENTITY) {
        const auto& tc = m_registry->GetComponent<Scene::TransformComponent>(spawnPoint);
        spawnPos = tc.position;
        spawnRot = tc.rotation;
    }

    ECS::Entity aiPawn = m_registry->CreateEntity();
    m_registry->AddComponent<Scene::TransformComponent>(aiPawn, spawnPos, spawnRot, glm::vec3(1.0f));
    
    auto& health = m_registry->AddComponent<HealthComponent>(aiPawn);
    health.currentHealth = 50.0f; // AI has less health
    health.maxHealth = 50.0f;

    auto& team = m_registry->AddComponent<TeamComponent>(aiPawn);
    team.teamId = 99; // AI team

    Core::Logger::Info("SpawnManager", "Spawned AI entity %u ('%s') at (%.2f, %.2f, %.2f)", 
                       aiPawn, aiName.c_str(), spawnPos.x, spawnPos.y, spawnPos.z);

    return aiPawn;
}

void SpawnManager::RespawnPlayer(ECS::Entity playerController, const std::string& spawnGroup) {
    if (!m_registry || !m_registry->IsAlive(playerController)) return;

    // Copy to stack to prevent dangling reference when the possessed pawn (and its callback lambda) is destroyed
    std::string localSpawnGroup = spawnGroup;

    auto& pc = m_registry->GetComponent<PlayerControllerComponent>(playerController);
    
    // 1. Destroy old pawn
    if (pc.possessedPawn != ECS::NULL_ENTITY && m_registry->IsAlive(pc.possessedPawn)) {
        m_registry->DestroyEntity(pc.possessedPawn);
        pc.possessedPawn = ECS::NULL_ENTITY;
    }

    // 2. Spawn new player pawn
    SpawnPlayer(playerController, localSpawnGroup);
}

} // namespace KumariEngine::Gameplay
