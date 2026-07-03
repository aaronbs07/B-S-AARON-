#pragma once
#include "camera/camera.hpp"
#include "ecs/ecs.hpp"
#include "reflection/reflection.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace KumariEngine::Camera {

struct CameraComponent {
    CameraMode  mode             = CameraMode::Free;
    float       fov              = 60.0f;
    float       aspect           = 1.333f;
    float       nearClip         = 0.1f;
    float       farClip          = 1000.0f;
    int32_t     priority         = 0;
    bool        collisionEnabled = true;

    // Follow / Orbit / Target tracking
    ECS::Entity targetEntity     = ECS::NULL_ENTITY;
    std::string targetEntityGuid; // stable serialization key
    glm::vec3   targetHeightOffset = glm::vec3(0.0f, 1.8f, 0.0f);
    glm::vec3   shoulderOffset     = glm::vec3(0.0f);
    float       orbitDistance      = 5.0f;
    float       minOrbitDistance   = 1.0f;
    float       maxOrbitDistance   = 20.0f;
    bool        autoReposition     = false;
    float       repositionSpeed    = 2.0f;

    // Camera Shake (runtime-only — Hidden from editor)
    float shakeIntensity = 0.0f;
    float shakeDuration  = 0.0f;
    float shakeTimer     = 0.0f;
    float shakeSpeed     = 25.0f;

    CameraComponent() = default;
};

} // namespace KumariEngine::Camera

// ---------------------------------------------------------------------------
// Reflection Registration
// ---------------------------------------------------------------------------
REFLECT_COMPONENT_BEGIN(KumariEngine::Camera::CameraComponent, "CameraComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, fov)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Field of View")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Camera")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         10.0)
        REFLECT_META(KumariEngine::Reflection::Meta::Max,         180.0)
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, fov)

    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, nearClip)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Near Clip")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Camera")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.001)
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, nearClip)

    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, farClip)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Far Clip")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Camera")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         1.0)
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, farClip)

    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, priority)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Priority")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Camera")
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, priority)

    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, orbitDistance)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Orbit Distance")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Orbit")
        REFLECT_META(KumariEngine::Reflection::Meta::Min,         0.1)
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, orbitDistance)

    REFLECT_PROP_BEGIN(KumariEngine::Camera::CameraComponent, targetEntityGuid)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Target GUID")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Targeting")
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, targetEntityGuid)

    REFLECT_PROP_BEGIN_FLAGS(KumariEngine::Camera::CameraComponent, shakeTimer,
                             KumariEngine::Reflection::PropertyFlags::ReadOnly |
                             KumariEngine::Reflection::PropertyFlags::Hidden)
    REFLECT_PROP_COMMIT(KumariEngine::Camera::CameraComponent, shakeTimer)
REFLECT_END()
