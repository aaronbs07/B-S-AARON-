#pragma once
#include "ecs/ecs.hpp"

namespace KumariEngine::Gameplay {

class GameState {
public:
    GameState(ECS::Registry* registry, ECS::Entity entity);
    virtual ~GameState() = default;

    ECS::Entity GetEntity() const { return m_entity; }

    float GetElapsedTime() const;
    void SetElapsedTime(float time);

    bool IsMatchRunning() const;
    void SetMatchRunning(bool running);

    bool IsMatchOver() const;
    void SetMatchOver(bool over);

    int GetWinnerTeamId() const;
    void SetWinnerTeamId(int teamId);

protected:
    ECS::Registry* m_registry = nullptr;
    ECS::Entity m_entity = ECS::NULL_ENTITY;
};

} // namespace KumariEngine::Gameplay
