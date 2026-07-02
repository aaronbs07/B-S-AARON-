#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <memory>
#include <vector>

namespace KumariEngine::Gameplay {

class GameState;
class PlayerController;
class PlayerState;

class GameMode {
public:
    GameMode(ECS::Registry* registry);
    virtual ~GameMode() = default;

    virtual void StartMatch();
    virtual void EndMatch();

    virtual void OnPlayerConnected(uint32_t peerId);
    virtual void OnPlayerDisconnected(uint32_t peerId);

    virtual void OnPlayerSpawn(ECS::Entity playerController);
    virtual void OnPlayerKilled(ECS::Entity playerController, ECS::Entity killerController);

    virtual void Tick(float deltaTime);

    std::shared_ptr<GameState> GetGameState() const { return m_gameState; }

    std::vector<std::shared_ptr<PlayerController>> GetPlayerControllers() const;
    std::vector<std::shared_ptr<PlayerState>> GetPlayerStates() const;

    std::shared_ptr<PlayerController> GetPlayerController(uint32_t peerId) const;
    std::shared_ptr<PlayerState> GetPlayerState(uint32_t peerId) const;

    // Config parameters
    float matchDuration = 300.0f; // 5 minutes default
    int maxPlayers = 8;
    bool bFriendlyFire = false;

protected:
    ECS::Registry* m_registry = nullptr;
    std::shared_ptr<GameState> m_gameState;
};

} // namespace KumariEngine::Gameplay
