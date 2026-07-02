#pragma once
#include "camera/camera.hpp"

namespace KumariEngine::Camera {

struct CameraComponent {
    CameraMode mode = CameraMode::Free;
    float fov = 60.0f;
    float aspect = 1.333f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;
    int priority = 0;
    bool collisionEnabled = true;

    // Follow / Orbit / Target tracking fields
    ECS::Entity targetEntity = ECS::NULL_ENTITY;
    std::string targetEntityGuid; // Guid for stable serialization
    glm::vec3 targetHeightOffset = glm::vec3(0.0f, 1.8f, 0.0f);
    glm::vec3 shoulderOffset = glm::vec3(0.0f);
    float orbitDistance = 5.0f;
    float minOrbitDistance = 1.0f;
    float maxOrbitDistance = 20.0f;
    bool autoReposition = false;
    float repositionSpeed = 2.0f;

    // Camera Shake fields
    float shakeIntensity = 0.0f;
    float shakeDuration = 0.0f;
    float shakeTimer = 0.0f;
    float shakeSpeed = 25.0f; // frequency

    CameraComponent() = default;
};

} // namespace KumariEngine::Camera
