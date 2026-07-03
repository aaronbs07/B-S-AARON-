#include "hierarchy_window.hpp"
#include "editor/selection_system.hpp"
#include "editor/undo_redo.hpp"
#include "scene/scene_manager.hpp"
#include "scene/transform_component.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "camera/camera_component.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void SceneHierarchyWindow::Initialize() {
    Core::Logger::Info("Editor", "Scene Hierarchy Window Initialized");
}

void SceneHierarchyWindow::Update(float deltaTime) {
    (void)deltaTime;
}
void SceneHierarchyWindow::RenderUI() {
    auto* root = Scene::SceneManager::Get().GetRootNode();
    Core::Logger::Info("EditorUI", "=== [Scene Hierarchy] ===");
    if (root) {
        PrintNodeHierarchy(root, 0);
    }
}

void SceneHierarchyWindow::PrintNodeHierarchy(Scene::SceneNode* node, int depth) {
    if (!node) return;
    std::string indent(depth * 2, ' ');
    std::string details = "";
    if (node->GetEntity() != ECS::NULL_ENTITY) {
        details = " [Entity: " + std::to_string(node->GetEntity()) + "]";
    }
    Core::Logger::Info("EditorUI", "%s- %s%s", indent.c_str(), node->GetName().c_str(), details.c_str());
    for (const auto& child : node->GetChildren()) {
        PrintNodeHierarchy(child.get(), depth + 1);
    }
}
void SceneHierarchyWindow::SelectEntity(ECS::Entity entity) {
    SelectionSystem::Get().Select(entity);
}

void SceneHierarchyWindow::RenameNode(Scene::SceneNode* node, const std::string& newName) {
    if (node && node->GetEntity() != ECS::NULL_ENTITY) {
        auto* registry = WindowSystem::Get().GetRegistry();
        auto cmd = std::make_shared<RenameEntityCommand>(registry, node->GetEntity(), newName);
        UndoSystem::Get().Execute(cmd);
    } else if (node) {
        node->SetName(newName);
    }
}

void SceneHierarchyWindow::DeleteNode(Scene::SceneNode* node) {
    if (!node) return;
    ECS::Entity ent = node->GetEntity();
    if (ent != ECS::NULL_ENTITY) {
        auto* registry = WindowSystem::Get().GetRegistry();
        auto cmd = std::make_shared<DeleteEntityCommand>(registry, ent);
        UndoSystem::Get().Execute(cmd);
    } else {
        auto* parent = node->GetParent();
        if (parent) {
            parent->RemoveChild(node);
        }
    }
}

Scene::SceneNode* SceneHierarchyWindow::DuplicateNode(Scene::SceneNode* node, Scene::SceneNode* customParent) {
    (void)customParent;
    if (!node) return nullptr;
    ECS::Entity ent = node->GetEntity();
    if (ent != ECS::NULL_ENTITY) {
        auto* registry = WindowSystem::Get().GetRegistry();
        auto cmd = std::make_shared<DuplicateEntityCommand>(registry, ent);
        UndoSystem::Get().Execute(cmd);
        ECS::Entity dupEnt = cmd->GetDuplicatedEntity();
        if (dupEnt != ECS::NULL_ENTITY) {
            return Scene::SceneManager::Get().GetNodeByEntity(dupEnt);
        }
    }
    return nullptr;
}

void SceneHierarchyWindow::CreateEmptyEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::Empty, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::CreateCameraEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::Camera, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::CreateDirLightEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::DirLight, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::CreatePointLightEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::PointLight, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::CreateSpotLightEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::SpotLight, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::CreateMeshEntity(const std::string& name, const std::string& meshPath) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::Mesh, name);
    UndoSystem::Get().Execute(cmd);
    if (!meshPath.empty()) {
        ECS::Entity created = cmd->GetCreatedEntity();
        if (created != ECS::NULL_ENTITY && registry->HasComponent<Renderer::MeshRendererComponent>(created)) {
            registry->GetComponent<Renderer::MeshRendererComponent>(created).meshPath = meshPath;
        }
    }
}

void SceneHierarchyWindow::CreatePhysicsEntity(const std::string& name) {
    auto* registry = WindowSystem::Get().GetRegistry();
    auto cmd = std::make_shared<CreateEntityCommand>(registry, CreateEntityCommand::Preset::Physics, name);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::ParentNode(Scene::SceneNode* node, Scene::SceneNode* newParent) {
    if (!node) return;
    auto* registry = WindowSystem::Get().GetRegistry();
    ECS::Entity entity = node->GetEntity();
    ECS::Entity newParentEnt = newParent ? newParent->GetEntity() : ECS::NULL_ENTITY;
    if (entity != ECS::NULL_ENTITY) {
        auto cmd = std::make_shared<ParentEntityCommand>(registry, entity, newParentEnt);
        UndoSystem::Get().Execute(cmd);
    }
}

void SceneHierarchyWindow::UnparentNode(Scene::SceneNode* node) {
    ParentNode(node, nullptr);
}

void SceneHierarchyWindow::ReorderChild(Scene::SceneNode* parent, int oldIndex, int newIndex) {
    auto* registry = WindowSystem::Get().GetRegistry();
    ECS::Entity parentEnt = parent ? parent->GetEntity() : ECS::NULL_ENTITY;
    auto cmd = std::make_shared<ReorderChildrenCommand>(registry, parentEnt, oldIndex, newIndex);
    UndoSystem::Get().Execute(cmd);
}

void SceneHierarchyWindow::SelectRange(Scene::SceneNode* startNode, Scene::SceneNode* endNode) {
    if (!startNode || !endNode) return;
    std::vector<Scene::SceneNode*> flat;
    GetFlattenedNodes(Scene::SceneManager::Get().GetRootNode(), flat);

    auto startIt = std::find(flat.begin(), flat.end(), startNode);
    auto endIt = std::find(flat.begin(), flat.end(), endNode);

    if (startIt != flat.end() && endIt != flat.end()) {
        int startIdx = static_cast<int>(std::distance(flat.begin(), startIt));
        int endIdx = static_cast<int>(std::distance(flat.begin(), endIt));

        int minIdx = std::min(startIdx, endIdx);
        int maxIdx = std::max(startIdx, endIdx);

        auto& ss = SelectionSystem::Get();
        ss.ClearSelection();
        for (int i = minIdx; i <= maxIdx; ++i) {
            if (flat[i]->GetEntity() != ECS::NULL_ENTITY) {
                ss.AddToSelection(flat[i]->GetEntity());
            }
        }
    }
}

void SceneHierarchyWindow::GetFlattenedNodes(Scene::SceneNode* node, std::vector<Scene::SceneNode*>& outNodes) {
    if (!node) return;
    if (node != Scene::SceneManager::Get().GetRootNode()) {
        outNodes.push_back(node);
    }
    for (const auto& child : node->GetChildren()) {
        GetFlattenedNodes(child.get(), outNodes);
    }
}

} // namespace KumariEngine::Editor
