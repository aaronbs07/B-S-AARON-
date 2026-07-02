#pragma once
#include "ecs/ecs.hpp"

namespace KumariEngine::Timeline {

class CinematicSystem {
public:
    static CinematicSystem& Get() {
        static CinematicSystem instance;
        return instance;
    }

    void Update(ECS::Registry* registry, float deltaTime);

    // Timeline actions
    void Play(ECS::Registry* registry, ECS::Entity playerEntity);
    void Pause(ECS::Registry* registry, ECS::Entity playerEntity);
    void Resume(ECS::Registry* registry, ECS::Entity playerEntity);
    void Stop(ECS::Registry* registry, ECS::Entity playerEntity);
    void Skip(ECS::Registry* registry, ECS::Entity playerEntity);

private:
    CinematicSystem() = default;
    ~CinematicSystem() = default;

    void ProcessTracks(ECS::Registry* registry, ECS::Entity playerEntity, float previousTime, float currentTime);
};

} // namespace KumariEngine::Timeline
