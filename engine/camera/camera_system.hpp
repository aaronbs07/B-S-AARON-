#pragma once
#include "ecs/ecs.hpp"
#include "input/input.hpp"

namespace KumariEngine::Camera {

class CameraSystem {
public:
    static CameraSystem& Get() {
        static CameraSystem instance;
        return instance;
    }

    void Update(ECS::Registry* registry, float deltaTime, const Input::Input* input);

private:
    CameraSystem() = default;
    ~CameraSystem() = default;
};

} // namespace KumariEngine::Camera
