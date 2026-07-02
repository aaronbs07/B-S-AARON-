#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <vector>
#include <functional>

namespace KumariEngine::Gameplay {

struct HealthComponent {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float shield = 0.0f;
    bool invulnerable = false;

    // Callbacks for events (can be triggered in C++ or Lua)
    std::function<void(float, float)> onHealthChanged = nullptr; // (oldHealth, newHealth)
    std::function<void()> onDeath = nullptr;
};

struct DamageComponent {
    float damageAmount = 10.0f;
    std::string damageType = "Physical";
    float multiplier = 1.0f;
    float knockbackForce = 0.0f;
};

struct TeamComponent {
    int teamId = 0;
    bool friendlyFire = false;
};

struct InteractionComponent {
    std::string prompt = "Interact";
    float distance = 3.0f;
    bool isInteractable = true;

    // Callbacks
    std::function<void(ECS::Entity)> onInteracted = nullptr; // (instigator)
};

struct SpawnPointComponent {
    std::string spawnGroup = "Default";
    bool isEnabled = true;
};

struct PlayerControllerComponent {
    uint32_t peerId = 0; // 0 represents local player
    ECS::Entity possessedPawn = ECS::NULL_ENTITY;
    ECS::Entity playerStateEntity = ECS::NULL_ENTITY;
    bool isLocal = true;
};

struct PlayerStateComponent {
    std::string playerName = "Player";
    uint32_t peerId = 0;
    float score = 0.0f;
    int teamId = 0;
    float ping = 0.0f;
};

struct GameStateComponent {
    float elapsedTime = 0.0f;
    bool isMatchRunning = false;
    bool isMatchOver = false;
    int winnerTeamId = -1;
};

} // namespace KumariEngine::Gameplay
