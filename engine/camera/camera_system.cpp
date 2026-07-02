#include "camera_system.hpp"
#include "camera_component.hpp"
#include "camera_manager.hpp"
#include "scene/transform_component.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Camera {

static Save::EntityGUID ParseGuidString(const std::string& str) {
    Save::EntityGUID guid = Save::NULL_GUID;
    if (str.length() == 32) {
        try {
            guid.high = std::stoull(str.substr(0, 16), nullptr, 16);
            guid.low = std::stoull(str.substr(16, 16), nullptr, 16);
        } catch (...) {}
    }
    return guid;
}

void CameraSystem::Update(ECS::Registry* registry, float deltaTime, const Input::Input* input) {
    if (!registry) return;

    registry->Each<CameraComponent, Scene::TransformComponent>([&](ECS::Entity entity, CameraComponent& comp, Scene::TransformComponent& tc) {
        // Construct unique camera name mapped to this entity
        std::string cameraName = "Camera_" + std::to_string(entity);

        auto cam = CameraManager::Get().GetCamera(cameraName);
        if (!cam) {
            cam = std::make_shared<Camera>();
            CameraManager::Get().RegisterCamera(cameraName, cam);
            
            // Set initial position/rotation from transform
            cam->SetPosition(tc.position);
            cam->SetRotation(tc.rotation);
        }

        // Resolve target entity from GUID string if needed
        if (comp.targetEntity == ECS::NULL_ENTITY && !comp.targetEntityGuid.empty()) {
            Save::EntityGUID targetGuid = ParseGuidString(comp.targetEntityGuid);
            if (!targetGuid.IsNull()) {
                comp.targetEntity = registry->GetEntityByGUID(targetGuid);
            }
        }

        // Apply component settings to Camera object
        cam->SetMode(comp.mode);
        cam->SetFov(comp.fov);
        cam->SetAspect(comp.aspect);
        cam->SetNearClip(comp.nearClip);
        cam->SetFarClip(comp.farClip);
        cam->SetPriority(comp.priority);
        cam->EnableCollision(comp.collisionEnabled);

        // Third-person and orbit configurations
        cam->SetOrbitDistance(comp.orbitDistance);
        cam->SetMinOrbitDistance(comp.minOrbitDistance);
        cam->SetMaxOrbitDistance(comp.maxOrbitDistance);
        cam->SetAutoReposition(comp.autoReposition);
        cam->SetRepositionSpeed(comp.repositionSpeed);
        cam->SetTargetHeightOffset(comp.targetHeightOffset);
        cam->SetShoulderOffset(comp.shoulderOffset);

        // Update target position if tracking target is valid
        if (comp.targetEntity != ECS::NULL_ENTITY && registry->IsAlive(comp.targetEntity)) {
            if (registry->HasComponent<Scene::TransformComponent>(comp.targetEntity)) {
                const auto& targetTc = registry->GetComponent<Scene::TransformComponent>(comp.targetEntity);
                cam->SetTargetPosition(targetTc.position);
                glm::vec3 forward = targetTc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                cam->SetTargetForward(forward);
            }
        }

        // Synchronize and apply camera shake
        if (comp.shakeTimer > 0.0f && cam->GetShakeTimer() <= 0.0f) {
            cam->StartShake(comp.shakeIntensity, comp.shakeDuration, comp.shakeSpeed);
        }
        
        // Sync back shake variables
        comp.shakeTimer = cam->GetShakeTimer();
        comp.shakeIntensity = cam->GetShakeIntensity();

        // Update camera matrices and internal smooth transitions
        cam->Update(deltaTime, input);

        // Sync local entity transform to calculated camera positioning
        tc.position = cam->GetCurrentPosition();
        tc.rotation = cam->GetCurrentRotation();
    });
}

} // namespace KumariEngine::Camera
