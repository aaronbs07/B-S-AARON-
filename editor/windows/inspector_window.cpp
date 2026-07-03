#include "inspector_window.hpp"
#include "inspector_window_reflection.hpp"
#include "editor/window_system.hpp"
#include "editor/selection_system.hpp"
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

    ReflectionPropertyEditor::RenderEntityComponents(registry, entity);
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
