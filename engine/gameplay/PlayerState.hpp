#pragma once
#include "ecs/ecs.hpp"
#include <string>

namespace KumariEngine::Gameplay {

class PlayerState {
public:
    PlayerState(ECS::Registry* registry, ECS::Entity entity);
    virtual ~PlayerState() = default;

    ECS::Entity GetEntity() const { return m_entity; }

    std::string GetPlayerName() const;
    void SetPlayerName(const std::string& name);

    uint32_t GetPeerId() const;
    void SetPeerId(uint32_t peerId);

    float GetScore() const;
    void SetScore(float score);
    void AddScore(float amount);

    int GetTeamId() const;
    void SetTeamId(int teamId);

    float GetPing() const;
    void SetPing(float ping);

protected:
    ECS::Registry* m_registry = nullptr;
    ECS::Entity m_entity = ECS::NULL_ENTITY;
};

} // namespace KumariEngine::Gameplay
