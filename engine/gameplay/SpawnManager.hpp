#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace KumariEngine::Gameplay {

class SpawnManager {
public:
    SpawnManager(ECS::Registry* registry);
    ~SpawnManager() = default;

    // Creates and configures a spawn point entity in the world
    ECS::Entity CreateSpawnPoint(const std::string& group, const glm::vec3& position, const glm::quat& rotation);

    // Spawns a player pawn entity and links it to the player controller
    ECS::Entity SpawnPlayer(ECS::Entity playerController, const std::string& spawnGroup = "Default");

    // Spawns an AI entity
    ECS::Entity SpawnAI(const std::string& aiName, const std::string& spawnGroup = "Default");

    // Triggers respawning of a player's pawn
    void RespawnPlayer(ECS::Entity playerController, const std::string& spawnGroup = "Default");

    // Retrieves the best available spawn point entity matching group criteria
    ECS::Entity FindBestSpawnPoint(const std::string& spawnGroup);

private:
    ECS::Registry* m_registry = nullptr;
};

} // namespace KumariEngine::Gameplay
