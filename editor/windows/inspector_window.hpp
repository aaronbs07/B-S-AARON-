#pragma once
#include "editor/window_system.hpp"
#include "editor/undo_redo.hpp"

namespace KumariEngine::Editor {

class InspectorWindow : public EditorWindow {
public:
    InspectorWindow() : EditorWindow("Inspector") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    void RenderComponentReadout(ECS::Entity entity);

    // Component Edit Operations (Undoable)
    void EditTransform(ECS::Entity entity, const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);
    void EditCamera(ECS::Entity entity, Camera::CameraMode mode, float fov, float nearClip, float farClip, int priority, bool collision);
    void EditLight(ECS::Entity entity, Lighting::LightType type, const glm::vec3& color, float intensity, float radius, float innerCutoff, float outerCutoff);
    void EditMeshRenderer(ECS::Entity entity, const std::string& meshPath, const std::string& materialPath, bool visible, bool castShadows);
    void EditPhysics(ECS::Entity entity, Physics::BodyType bodyType, float mass, float restitution, Physics::ColliderType colliderType);
    void EditScript(ECS::Entity entity, const std::string& scriptPath);

    // Component Addition and Removal (Undoable)
    void AddTransformComponent(ECS::Entity entity);
    void AddCameraComponent(ECS::Entity entity);
    void AddLightComponent(ECS::Entity entity);
    void AddMeshRendererComponent(ECS::Entity entity, const std::string& meshPath = "");
    void AddPhysicsComponent(ECS::Entity entity);
    void AddScriptComponent(ECS::Entity entity, const std::string& scriptPath);
    void RemoveComponent(ECS::Entity entity, ComponentType type);
};

} // namespace KumariEngine::Editor
