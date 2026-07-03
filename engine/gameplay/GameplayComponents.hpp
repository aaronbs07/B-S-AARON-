#pragma once
#include "ecs/ecs.hpp"
#include "reflection/reflection.hpp"
#include <string>
#include <vector>
#include <functional>

namespace KumariEngine::Gameplay {

struct HealthComponent {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float shield = 0.0f;
    bool invulnerable = false;

    std::function<void(float, float)> onHealthChanged = nullptr;
    std::function<void()> onDeath = nullptr;
};

struct DamageComponent {
    float damageAmount = 10.0f;
    std::string damageType = "Physical";
    float multiplier = 1.0f;
    float knockbackForce = 0.0f;
};

struct TeamComponent {
    int32_t teamId = 0;
    bool friendlyFire = false;
};

struct InteractionComponent {
    std::string prompt = "Interact";
    float distance = 3.0f;
    bool isInteractable = true;
    std::function<void(ECS::Entity)> onInteracted = nullptr;
};

struct SpawnPointComponent {
    std::string spawnGroup = "Default";
    bool isEnabled = true;
};

struct PlayerControllerComponent {
    uint32_t peerId = 0;
    ECS::Entity possessedPawn = ECS::NULL_ENTITY;
    ECS::Entity playerStateEntity = ECS::NULL_ENTITY;
    bool isLocal = true;
};

struct PlayerStateComponent {
    std::string playerName = "Player";
    uint32_t peerId = 0;
    float score = 0.0f;
    int32_t teamId = 0;
    float ping = 0.0f;
};

struct GameStateComponent {
    float elapsedTime = 0.0f;
    bool isMatchRunning = false;
    bool isMatchOver = false;
    int32_t winnerTeamId = -1;
};

} // namespace KumariEngine::Gameplay

// ---------------------------------------------------------------------------
// Reflection Registrations  (use REFLECT_PROP_BEGIN / REFLECT_PROP_COMMIT)
// ---------------------------------------------------------------------------

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::HealthComponent, "HealthComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::HealthComponent, currentHealth)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Current Health")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Health")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.0)
        REFLECT_META(KumariEngine::Reflection::Meta::Max,         9999.0)
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::HealthComponent, currentHealth)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::HealthComponent, maxHealth)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Max Health")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Health")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         1.0)
        REFLECT_META(KumariEngine::Reflection::Meta::Max,         9999.0)
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::HealthComponent, maxHealth)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::HealthComponent, shield)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Shield")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Health")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::HealthComponent, shield)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::HealthComponent, invulnerable)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Invulnerable")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Health")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::HealthComponent, invulnerable)
REFLECT_END()

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::DamageComponent, "DamageComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::DamageComponent, damageAmount)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Damage Amount")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.0)
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::DamageComponent, damageAmount)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::DamageComponent, damageType)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Damage Type")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::DamageComponent, damageType)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::DamageComponent, multiplier)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Multiplier")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.0)
        REFLECT_META(KumariEngine::Reflection::Meta::Max,         100.0)
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::DamageComponent, multiplier)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::DamageComponent, knockbackForce)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Knockback Force")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.0)
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::DamageComponent, knockbackForce)
REFLECT_END()

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::TeamComponent, "TeamComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::TeamComponent, teamId)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Team ID")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::TeamComponent, teamId)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::TeamComponent, friendlyFire)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Friendly Fire")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::TeamComponent, friendlyFire)
REFLECT_END()

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::SpawnPointComponent, "SpawnPointComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::SpawnPointComponent, spawnGroup)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Spawn Group")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::SpawnPointComponent, spawnGroup)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::SpawnPointComponent, isEnabled)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Enabled")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::SpawnPointComponent, isEnabled)
REFLECT_END()

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::PlayerStateComponent, "PlayerStateComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::PlayerStateComponent, playerName)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Player Name")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::PlayerStateComponent, playerName)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::PlayerStateComponent, score)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Score")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::PlayerStateComponent, score)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::PlayerStateComponent, teamId)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Team ID")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::PlayerStateComponent, teamId)
    REFLECT_PROP_BEGIN_FLAGS(KumariEngine::Gameplay::PlayerStateComponent, ping,
                             KumariEngine::Reflection::PropertyFlags::ReadOnly)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Ping (ms)")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::PlayerStateComponent, ping)
REFLECT_END()

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::GameStateComponent, "GameStateComponent")
    REFLECT_PROP_BEGIN_FLAGS(KumariEngine::Gameplay::GameStateComponent, elapsedTime,
                             KumariEngine::Reflection::PropertyFlags::ReadOnly)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Elapsed Time")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::GameStateComponent, elapsedTime)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::GameStateComponent, isMatchRunning)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Match Running")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::GameStateComponent, isMatchRunning)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::GameStateComponent, isMatchOver)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Match Over")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::GameStateComponent, isMatchOver)
    REFLECT_PROP_BEGIN(KumariEngine::Gameplay::GameStateComponent, winnerTeamId)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Winner Team")
    REFLECT_PROP_COMMIT(KumariEngine::Gameplay::GameStateComponent, winnerTeamId)
REFLECT_END()
