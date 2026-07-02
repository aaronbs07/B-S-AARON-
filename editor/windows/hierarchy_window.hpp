#pragma once
#include "editor/window_system.hpp"
#include "scene/scene_node.hpp"

namespace KumariEngine::Editor {

class SceneHierarchyWindow : public EditorWindow {
public:
    SceneHierarchyWindow() : EditorWindow("Scene Hierarchy") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    void SelectEntity(ECS::Entity entity);
    void RenameNode(Scene::SceneNode* node, const std::string& newName);
    void DeleteNode(Scene::SceneNode* node);
    Scene::SceneNode* DuplicateNode(Scene::SceneNode* node, Scene::SceneNode* customParent = nullptr);

    // Entity Creation presets
    void CreateEmptyEntity(const std::string& name = "Empty Entity");
    void CreateCameraEntity(const std::string& name = "Camera");
    void CreateDirLightEntity(const std::string& name = "Directional Light");
    void CreatePointLightEntity(const std::string& name = "Point Light");
    void CreateSpotLightEntity(const std::string& name = "Spot Light");
    void CreateMeshEntity(const std::string& name = "Mesh Entity", const std::string& meshPath = "");
    void CreatePhysicsEntity(const std::string& name = "Physics Entity");

    // Parenting / Reordering operations
    void ParentNode(Scene::SceneNode* node, Scene::SceneNode* newParent);
    void UnparentNode(Scene::SceneNode* node);
    void ReorderChild(Scene::SceneNode* parent, int oldIndex, int newIndex);

    // Shift selection
    void SelectRange(Scene::SceneNode* startNode, Scene::SceneNode* endNode);

private:
    void PrintNodeHierarchy(Scene::SceneNode* node, int depth);
    void GetFlattenedNodes(Scene::SceneNode* node, std::vector<Scene::SceneNode*>& outNodes);
};

} // namespace KumariEngine::Editor
