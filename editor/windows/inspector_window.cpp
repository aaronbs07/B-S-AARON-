#include "inspector_window.hpp"
#include "editor/selection_system.hpp"
#include "editor/undo_redo.hpp"
#include "scene/scene_manager.hpp"
#include "scene/transform_component.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "camera/camera_component.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void InspectorWindow::Initialize() {
    Core::Logger::Info("Editor", "Inspector Window Initialized");
}

void InspectorWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void InspectorWindow::RenderUI() {
    ECS::Entity selected = SelectionSystem::Get().GetSelected();
    if (selected != ECS::NULL_ENTITY) {
        RenderComponentReadout(selected);
    }
}

void InspectorWindow::RenderComponentReadout(ECS::Entity entity) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (!registry || !registry->IsAlive(entity)) return;

    // 1. TransformComponent
    if (registry->HasComponent<Scene::TransformComponent>(entity)) {
        const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Transform");
        Core::Logger::Info("Inspector", "  Position: (%.2f, %.2f, %.2f)", tc.position.x, tc.position.y, tc.position.z);
        Core::Logger::Info("Inspector", "  Scale: (%.2f, %.2f, %.2f)", tc.scale.x, tc.scale.y, tc.scale.z);
    }

    // 2. CameraComponent
    if (registry->HasComponent<Camera::CameraComponent>(entity)) {
        const auto& cc = registry->GetComponent<Camera::CameraComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Camera");
        Core::Logger::Info("Inspector", "  FOV: %.2f", cc.fov);
        Core::Logger::Info("Inspector", "  Near Clip: %.2f", cc.nearClip);
        Core::Logger::Info("Inspector", "  Far Clip: %.2f", cc.farClip);
        Core::Logger::Info("Inspector", "  Priority: %d", cc.priority);
        Core::Logger::Info("Inspector", "  Collision Enabled: %s", cc.collisionEnabled ? "True" : "False");
        Core::Logger::Info("Inspector", "  Target Entity GUID: %s", cc.targetEntityGuid.c_str());
        Core::Logger::Info("Inspector", "  Shoulder Offset: (%.2f, %.2f, %.2f)", cc.shoulderOffset.x, cc.shoulderOffset.y, cc.shoulderOffset.z);
        Core::Logger::Info("Inspector", "  Orbit Distance: %.2f", cc.orbitDistance);
        Core::Logger::Info("Inspector", "  Auto Reposition: %s", cc.autoReposition ? "True" : "False");
        Core::Logger::Info("Inspector", "  Shake Intensity: %.2f | Duration: %.2fs", cc.shakeIntensity, cc.shakeDuration);
    }

    // 3. LightComponent
    if (registry->HasComponent<Lighting::LightComponent>(entity)) {
        const auto& lc = registry->GetComponent<Lighting::LightComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Light");
        std::string typeStr = "Directional";
        if (lc.type == Lighting::LightType::Point) typeStr = "Point";
        else if (lc.type == Lighting::LightType::Spot) typeStr = "Spot";
        Core::Logger::Info("Inspector", "  Type: %s", typeStr.c_str());
        Core::Logger::Info("Inspector", "  Color: (%.2f, %.2f, %.2f)", lc.color.x, lc.color.y, lc.color.z);
        Core::Logger::Info("Inspector", "  Intensity: %.2f", lc.intensity);
    }

    // 4. MeshRendererComponent
    if (registry->HasComponent<Renderer::MeshRendererComponent>(entity)) {
        const auto& mrc = registry->GetComponent<Renderer::MeshRendererComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Mesh Renderer");
        Core::Logger::Info("Inspector", "  Mesh: %s", mrc.meshPath.c_str());
        Core::Logger::Info("Inspector", "  Material: %s", mrc.materialPath.c_str());
        Core::Logger::Info("Inspector", "  Visible: %s", mrc.visible ? "True" : "False");
    }

    // 5. PhysicsComponent
    if (registry->HasComponent<Physics::PhysicsComponent>(entity)) {
        const auto& pc = registry->GetComponent<Physics::PhysicsComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Physics");
        std::string bodyStr = "Static";
        if (pc.bodyType == Physics::BodyType::Dynamic) bodyStr = "Dynamic";
        else if (pc.bodyType == Physics::BodyType::Kinematic) bodyStr = "Kinematic";
        Core::Logger::Info("Inspector", "  Body Type: %s", bodyStr.c_str());
        Core::Logger::Info("Inspector", "  Mass: %.2f", pc.mass);
        Core::Logger::Info("Inspector", "  Restitution: %.2f", pc.restitution);
    }

    // 6. ScriptComponent
    if (registry->HasComponent<ECS::ScriptComponent>(entity)) {
        const auto& sc = registry->GetComponent<ECS::ScriptComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Script");
        Core::Logger::Info("Inspector", "  Path: %s", sc.scriptPath.c_str());
        Core::Logger::Info("Inspector", "  Initialized: %s", sc.initialized ? "True" : "False");
    }

    // 7. HealthComponent
    if (registry->HasComponent<Gameplay::HealthComponent>(entity)) {
        const auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Health");
        Core::Logger::Info("Inspector", "  Current Health: %.1f", hc.currentHealth);
        Core::Logger::Info("Inspector", "  Max Health: %.1f", hc.maxHealth);
        Core::Logger::Info("Inspector", "  Shield: %.1f", hc.shield);
        Core::Logger::Info("Inspector", "  Invulnerable: %s", hc.invulnerable ? "True" : "False");
    }

    // 8. DamageComponent
    if (registry->HasComponent<Gameplay::DamageComponent>(entity)) {
        const auto& dc = registry->GetComponent<Gameplay::DamageComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Damage");
        Core::Logger::Info("Inspector", "  Damage Amount: %.1f", dc.damageAmount);
        Core::Logger::Info("Inspector", "  Damage Type: %s", dc.damageType.c_str());
        Core::Logger::Info("Inspector", "  Multiplier: %.1f", dc.multiplier);
        Core::Logger::Info("Inspector", "  Knockback: %.1f", dc.knockbackForce);
    }

    // 9. TeamComponent
    if (registry->HasComponent<Gameplay::TeamComponent>(entity)) {
        const auto& tc = registry->GetComponent<Gameplay::TeamComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Team");
        Core::Logger::Info("Inspector", "  Team ID: %d", tc.teamId);
        Core::Logger::Info("Inspector", "  Friendly Fire: %s", tc.friendlyFire ? "True" : "False");
    }

    // 10. InteractionComponent
    if (registry->HasComponent<Gameplay::InteractionComponent>(entity)) {
        const auto& ic = registry->GetComponent<Gameplay::InteractionComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Interaction");
        Core::Logger::Info("Inspector", "  Prompt: %s", ic.prompt.c_str());
        Core::Logger::Info("Inspector", "  Interact Distance: %.1f", ic.distance);
        Core::Logger::Info("Inspector", "  Is Interactable: %s", ic.isInteractable ? "True" : "False");
    }

    // 11. GameplayTagsComponent
    if (registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
        const auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Gameplay Tags");
        auto rawTags = gtc.GetRawTags();
        std::string tagsStr;
        for (size_t i = 0; i < rawTags.size(); ++i) {
            if (i > 0) tagsStr += ", ";
            tagsStr += rawTags[i];
        }
        Core::Logger::Info("Inspector", "  Tags: [%s]", tagsStr.c_str());
    }

    // 12. SpawnPointComponent
    if (registry->HasComponent<Gameplay::SpawnPointComponent>(entity)) {
        const auto& sp = registry->GetComponent<Gameplay::SpawnPointComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Spawn Point");
        Core::Logger::Info("Inspector", "  Spawn Group: %s", sp.spawnGroup.c_str());
        Core::Logger::Info("Inspector", "  Is Enabled: %s", sp.isEnabled ? "True" : "False");
    }

    // 13. AudioSourceComponent
    if (registry->HasComponent<Audio::AudioSourceComponent>(entity)) {
        const auto& asc = registry->GetComponent<Audio::AudioSourceComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Audio Source");
        Core::Logger::Info("Inspector", "  Path/Event: %s", asc.eventOrPath.c_str());
        Core::Logger::Info("Inspector", "  Is Playing: %s | Loop: %s | Spatial 3D: %s", 
                           asc.isPlaying ? "True" : "False", 
                           asc.loop ? "True" : "False", 
                           asc.spatial3D ? "True" : "False");
        Core::Logger::Info("Inspector", "  Volume: %.2f | Pitch: %.2f | Pan: %.2f", asc.volume, asc.pitch, asc.pan);
        Core::Logger::Info("Inspector", "  Min Distance: %.1f | Max Distance: %.1f", asc.minDistance, asc.maxDistance);
    }

    // 14. AudioListenerComponent
    if (registry->HasComponent<Audio::AudioListenerComponent>(entity)) {
        const auto& alc = registry->GetComponent<Audio::AudioListenerComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Audio Listener");
        Core::Logger::Info("Inspector", "  Position: (%.2f, %.2f, %.2f)", alc.position.x, alc.position.y, alc.position.z);
        Core::Logger::Info("Inspector", "  Forward: (%.2f, %.2f, %.2f)", alc.forward.x, alc.forward.y, alc.forward.z);
        Core::Logger::Info("Inspector", "  Up: (%.2f, %.2f, %.2f)", alc.up.x, alc.up.y, alc.up.z);
    }

    // 15. CinematicPlayerComponent
    if (registry->HasComponent<Timeline::CinematicPlayerComponent>(entity)) {
        const auto& cpc = registry->GetComponent<Timeline::CinematicPlayerComponent>(entity);
        Core::Logger::Info("Inspector", "Component: Cinematic Player");
        Core::Logger::Info("Inspector", "  Timeline Name: %s", cpc.timelineName.c_str());
        Core::Logger::Info("Inspector", "  Playing: %s | Paused: %s | Loop: %s", 
                           cpc.isPlaying ? "True" : "False", 
                           cpc.isPaused ? "True" : "False", 
                           cpc.loop ? "True" : "False");
        Core::Logger::Info("Inspector", "  Current Time: %.2fs | Playback Speed: %.2fx", cpc.currentTime, cpc.playbackSpeed);
    }
}

void InspectorWindow::EditTransform(ECS::Entity entity, const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (!registry || !registry->IsAlive(entity)) return;

    glm::vec3 oldPos{0.0f}, oldScale{1.0f};
    glm::quat oldRot{1.0f, 0.0f, 0.0f, 0.0f};

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        oldPos = node->GetLocalPosition();
        oldRot = node->GetLocalRotation();
        oldScale = node->GetLocalScale();
    } else if (registry->HasComponent<Scene::TransformComponent>(entity)) {
        const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
        oldPos = tc.position;
        oldRot = tc.rotation;
        oldScale = tc.scale;
    }

    auto cmd = std::make_shared<ModifyTransformCommand>(registry, entity, oldPos, oldRot, oldScale, pos, rot, scale);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::EditCamera(ECS::Entity entity, Camera::CameraMode mode, float fov, float nearClip, float farClip, int priority, bool collision) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry && registry->HasComponent<Camera::CameraComponent>(entity)) {
        auto oldCam = registry->GetComponent<Camera::CameraComponent>(entity);
        auto newCam = oldCam;
        newCam.mode = mode;
        newCam.fov = fov;
        newCam.nearClip = nearClip;
        newCam.farClip = farClip;
        newCam.priority = priority;
        newCam.collisionEnabled = collision;

        auto cmd = std::make_shared<ModifyCameraCommand>(registry, entity, oldCam, newCam);
        UndoSystem::Get().Execute(cmd);
    }
}

void InspectorWindow::EditLight(ECS::Entity entity, Lighting::LightType type, const glm::vec3& color, float intensity, float radius, float innerCutoff, float outerCutoff) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry && registry->HasComponent<Lighting::LightComponent>(entity)) {
        auto oldLight = registry->GetComponent<Lighting::LightComponent>(entity);
        auto newLight = oldLight;
        newLight.type = type;
        newLight.color = color;
        newLight.intensity = intensity;
        newLight.radius = radius;
        newLight.innerCutoff = innerCutoff;
        newLight.outerCutoff = outerCutoff;

        auto cmd = std::make_shared<ModifyLightCommand>(registry, entity, oldLight, newLight);
        UndoSystem::Get().Execute(cmd);
    }
}

void InspectorWindow::EditMeshRenderer(ECS::Entity entity, const std::string& meshPath, const std::string& materialPath, bool visible, bool castShadows) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry && registry->HasComponent<Renderer::MeshRendererComponent>(entity)) {
        auto oldRenderer = registry->GetComponent<Renderer::MeshRendererComponent>(entity);
        auto newRenderer = oldRenderer;
        newRenderer.meshPath = meshPath;
        newRenderer.materialPath = materialPath;
        newRenderer.visible = visible;
        newRenderer.castShadows = castShadows;

        auto cmd = std::make_shared<ModifyMeshRendererCommand>(registry, entity, oldRenderer, newRenderer);
        UndoSystem::Get().Execute(cmd);
    }
}

void InspectorWindow::EditPhysics(ECS::Entity entity, Physics::BodyType bodyType, float mass, float restitution, Physics::ColliderType colliderType) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry && registry->HasComponent<Physics::PhysicsComponent>(entity)) {
        auto oldPhysics = registry->GetComponent<Physics::PhysicsComponent>(entity);
        auto newPhysics = oldPhysics;
        newPhysics.bodyType = bodyType;
        newPhysics.SetMass(mass);
        newPhysics.restitution = restitution;
        newPhysics.collider.type = colliderType;

        auto cmd = std::make_shared<ModifyPhysicsCommand>(registry, entity, oldPhysics, newPhysics);
        UndoSystem::Get().Execute(cmd);
    }
}

void InspectorWindow::EditScript(ECS::Entity entity, const std::string& scriptPath) {
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry && registry->HasComponent<ECS::ScriptComponent>(entity)) {
        auto& sc = registry->GetComponent<ECS::ScriptComponent>(entity);
        auto cmd = std::make_shared<ModifyScriptCommand>(registry, entity, sc.scriptPath, scriptPath);
        UndoSystem::Get().Execute(cmd);
    }
}

void InspectorWindow::AddTransformComponent(ECS::Entity entity) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::Transform);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::AddCameraComponent(ECS::Entity entity) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::Camera);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::AddLightComponent(ECS::Entity entity) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::Light);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::AddMeshRendererComponent(ECS::Entity entity, const std::string& meshPath) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::MeshRenderer, meshPath);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::AddPhysicsComponent(ECS::Entity entity) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::Physics);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::AddScriptComponent(ECS::Entity entity, const std::string& scriptPath) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<AddComponentCommand>(registry, entity, ComponentType::Script, scriptPath);
    UndoSystem::Get().Execute(cmd);
}

void InspectorWindow::RemoveComponent(ECS::Entity entity, ComponentType type) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<RemoveComponentCommand>(registry, entity, type);
    UndoSystem::Get().Execute(cmd);
}

} // namespace KumariEngine::Editor
