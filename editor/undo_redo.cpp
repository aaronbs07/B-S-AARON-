#include "undo_redo.hpp"
#include "editor/selection_system.hpp"
#include "scene/scene_manager.hpp"
#include "save/BinaryWriter.hpp"
#include "save/BinaryReader.hpp"
#include "scripting/script_engine.hpp"
#include <sstream>
#include <algorithm>

namespace KumariEngine::Editor {

// Entity Snapshot Capture and Restore helpers

EntitySnapshot CaptureEntity(ECS::Registry* registry, ECS::Entity entity) {
    EntitySnapshot snapshot;
    if (!registry || !registry->IsAlive(entity)) return snapshot;

    snapshot.entity = entity;
    snapshot.guid = registry->GetGUID(entity);
    
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        snapshot.name = node->GetName();
        snapshot.localPosition = node->GetLocalPosition();
        snapshot.localRotation = node->GetLocalRotation();
        snapshot.localScale = node->GetLocalScale();

        auto* parent = node->GetParent();
        if (parent && parent != Scene::SceneManager::Get().GetRootNode()) {
            ECS::Entity parentEnt = parent->GetEntity();
            if (parentEnt != ECS::NULL_ENTITY) {
                snapshot.parentGuid = registry->GetGUID(parentEnt);
            }
        }
    } else {
        snapshot.name = "Entity_" + std::to_string(entity);
    }

    if (registry->HasComponent<Scene::TransformComponent>(entity)) {
        snapshot.hasTransform = true;
        snapshot.transform = registry->GetComponent<Scene::TransformComponent>(entity);
    }
    if (registry->HasComponent<Camera::CameraComponent>(entity)) {
        snapshot.hasCamera = true;
        snapshot.camera = registry->GetComponent<Camera::CameraComponent>(entity);
    }
    if (registry->HasComponent<Lighting::LightComponent>(entity)) {
        snapshot.hasLight = true;
        snapshot.light = registry->GetComponent<Lighting::LightComponent>(entity);
    }
    if (registry->HasComponent<Renderer::MeshRendererComponent>(entity)) {
        snapshot.hasMeshRenderer = true;
        snapshot.meshRenderer = registry->GetComponent<Renderer::MeshRendererComponent>(entity);
    }
    if (registry->HasComponent<Physics::PhysicsComponent>(entity)) {
        snapshot.hasPhysics = true;
        snapshot.physics = registry->GetComponent<Physics::PhysicsComponent>(entity);
    }
    if (registry->HasComponent<ECS::ScriptComponent>(entity)) {
        snapshot.hasScript = true;
        auto& sc = registry->GetComponent<ECS::ScriptComponent>(entity);
        snapshot.scriptPath = sc.scriptPath;

        std::stringstream tempStream;
        Save::BinaryWriter tempWriter(tempStream);
        Scripting::ScriptEngine::Get().SerializeScriptState(entity, tempWriter);
        snapshot.scriptStateData = tempStream.str();
    }

    return snapshot;
}

void RestoreEntity(ECS::Registry* registry, const EntitySnapshot& snapshot) {
    if (!registry || snapshot.entity == ECS::NULL_ENTITY) return;

    // Create the entity with manual ID allocation
    registry->CreateEntityWithID(snapshot.entity);
    if (!snapshot.guid.IsNull()) {
        registry->AssignGUID(snapshot.entity, snapshot.guid);
    }

    // Reconstruct SceneNode
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(snapshot.entity);
    if (!node) {
        node = Scene::SceneManager::Get().CreateNode(snapshot.name);
        node->SetEntity(snapshot.entity);
        Scene::SceneManager::Get().RegisterEntityNode(snapshot.entity, node);
    }
    node->SetName(snapshot.name);
    node->SetLocalPosition(snapshot.localPosition);
    node->SetLocalRotation(snapshot.localRotation);
    node->SetLocalScale(snapshot.localScale);

    // Parent setup
    if (!snapshot.parentGuid.IsNull()) {
        ECS::Entity parentEnt = registry->GetEntityByGUID(snapshot.parentGuid);
        if (parentEnt != ECS::NULL_ENTITY) {
            auto* parentNode = Scene::SceneManager::Get().GetNodeByEntity(parentEnt);
            if (parentNode && node->GetParent() != parentNode) {
                auto* root = Scene::SceneManager::Get().GetRootNode();
                if (root) {
                    auto nodePtr = root->RemoveChild(node);
                    if (nodePtr) {
                        parentNode->AddChild(std::move(nodePtr));
                    } else if (node->GetParent()) {
                        nodePtr = node->GetParent()->RemoveChild(node);
                        if (nodePtr) {
                            parentNode->AddChild(std::move(nodePtr));
                        }
                    }
                }
            }
        }
    } else {
        auto* root = Scene::SceneManager::Get().GetRootNode();
        if (root && node->GetParent() != root) {
            if (node->GetParent()) {
                auto nodePtr = node->GetParent()->RemoveChild(node);
                if (nodePtr) {
                    root->AddChild(std::move(nodePtr));
                }
            }
        }
    }

    // Restore components
    if (snapshot.hasTransform) {
        registry->AddComponent<Scene::TransformComponent>(snapshot.entity, snapshot.transform);
    }
    if (snapshot.hasCamera) {
        registry->AddComponent<Camera::CameraComponent>(snapshot.entity, snapshot.camera);
    }
    if (snapshot.hasLight) {
        registry->AddComponent<Lighting::LightComponent>(snapshot.entity, snapshot.light);
    }
    if (snapshot.hasMeshRenderer) {
        registry->AddComponent<Renderer::MeshRendererComponent>(snapshot.entity, snapshot.meshRenderer);
    }
    if (snapshot.hasPhysics) {
        registry->AddComponent<Physics::PhysicsComponent>(snapshot.entity, snapshot.physics);
    }
    if (snapshot.hasScript) {
        registry->AddComponent<ECS::ScriptComponent>(snapshot.entity, snapshot.scriptPath);
        Scripting::ScriptEngine::Get().OnCreateEntity(snapshot.entity);
        if (!snapshot.scriptStateData.empty()) {
            std::stringstream tempStream(snapshot.scriptStateData);
            Save::BinaryReader tempReader(tempStream);
            Scripting::ScriptEngine::Get().DeserializeScriptState(snapshot.entity, tempReader);
        }
    }
}

// Undo System History Stack

void UndoSystem::Execute(std::shared_ptr<Command> command) {
    if (!command) return;
    command->Execute();
    m_undoStack.push_back(command);
    m_redoStack.clear(); // Clear redo on new action
    m_isDirty = true;
}

void UndoSystem::Undo() {
    if (m_undoStack.empty()) return;
    auto command = m_undoStack.back();
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(command);
    m_isDirty = true;
}

void UndoSystem::Redo() {
    if (m_redoStack.empty()) return;
    auto command = m_redoStack.back();
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(command);
    m_isDirty = true;
}

void UndoSystem::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_isDirty = false;
}

// CreateEntityCommand

CreateEntityCommand::CreateEntityCommand(ECS::Registry* registry, Preset preset, const std::string& name, Save::EntityGUID parentGuid)
    : m_registry(registry), m_preset(preset), m_name(name), m_parentGuid(parentGuid) {}

void CreateEntityCommand::Execute() {
    if (m_firstRun) {
        m_entity = m_registry->CreateEntity();
        m_guid = m_registry->CreateGUID(m_entity);
        m_firstRun = false;
    } else {
        m_registry->CreateEntityWithID(m_entity);
        m_registry->AssignGUID(m_entity, m_guid);
    }

    auto* parentNode = Scene::SceneManager::Get().GetRootNode();
    if (!m_parentGuid.IsNull()) {
        ECS::Entity parentEnt = m_registry->GetEntityByGUID(m_parentGuid);
        if (parentEnt != ECS::NULL_ENTITY) {
            parentNode = Scene::SceneManager::Get().GetNodeByEntity(parentEnt);
        }
    }

    auto* node = Scene::SceneManager::Get().CreateNode(m_name, parentNode);
    node->SetEntity(m_entity);
    Scene::SceneManager::Get().RegisterEntityNode(m_entity, node);

    // Add Preset components
    m_registry->AddComponent<Scene::TransformComponent>(m_entity);

    if (m_preset == Preset::Camera) {
        m_registry->AddComponent<Camera::CameraComponent>(m_entity);
    } else if (m_preset == Preset::DirLight) {
        auto& lc = m_registry->AddComponent<Lighting::LightComponent>(m_entity);
        lc.type = Lighting::LightType::Directional;
    } else if (m_preset == Preset::PointLight) {
        auto& lc = m_registry->AddComponent<Lighting::LightComponent>(m_entity);
        lc.type = Lighting::LightType::Point;
    } else if (m_preset == Preset::SpotLight) {
        auto& lc = m_registry->AddComponent<Lighting::LightComponent>(m_entity);
        lc.type = Lighting::LightType::Spot;
    } else if (m_preset == Preset::Mesh) {
        m_registry->AddComponent<Renderer::MeshRendererComponent>(m_entity);
    } else if (m_preset == Preset::Physics) {
        m_registry->AddComponent<Physics::PhysicsComponent>(m_entity);
    }
}

void CreateEntityCommand::Undo() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (node) {
        if (SelectionSystem::Get().GetSelected() == m_entity) {
            SelectionSystem::Get().ClearSelection();
        }
        Scene::SceneManager::Get().DestroyNode(node);
    }
}

// DeleteEntityCommand

DeleteEntityCommand::DeleteEntityCommand(ECS::Registry* registry, ECS::Entity entity)
    : m_registry(registry), m_rootEntity(entity) {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        m_rootName = node->GetName();
    }
}

void DeleteEntityCommand::CaptureSubtree(ECS::Entity entity) {
    if (entity == ECS::NULL_ENTITY) return;

    m_snapshots.push_back(CaptureEntity(m_registry, entity));

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        std::vector<ECS::Entity> childEntities;
        for (const auto& child : node->GetChildren()) {
            if (child->GetEntity() != ECS::NULL_ENTITY) {
                childEntities.push_back(child->GetEntity());
            }
        }
        for (auto childEnt : childEntities) {
            CaptureSubtree(childEnt);
        }
    }
}

void DeleteEntityCommand::Execute() {
    if (m_snapshots.empty()) {
        CaptureSubtree(m_rootEntity);
    }

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_rootEntity);
    if (node) {
        if (SelectionSystem::Get().GetSelected() == m_rootEntity) {
            SelectionSystem::Get().ClearSelection();
        }
        Scene::SceneManager::Get().DestroyNode(node);
    }
}

void DeleteEntityCommand::Undo() {
    // Restore snapshots in order (parent will be restored before children)
    for (const auto& snap : m_snapshots) {
        RestoreEntity(m_registry, snap);
    }
}

// DuplicateEntityCommand

DuplicateEntityCommand::DuplicateEntityCommand(ECS::Registry* registry, ECS::Entity sourceEntity)
    : m_registry(registry), m_sourceEntity(sourceEntity) {}

void DuplicateEntityCommand::DuplicateSubtree(ECS::Entity source, Save::EntityGUID parentGuid) {
    if (source == ECS::NULL_ENTITY) return;

    EntitySnapshot snap = CaptureEntity(m_registry, source);

    ECS::Entity newEnt = m_registry->CreateEntity();
    Save::EntityGUID newGuid = m_registry->CreateGUID(newEnt);
    m_registry->DestroyEntity(newEnt); // release temporary ID/GUID mappings

    snap.entity = newEnt;
    snap.guid = newGuid;
    if (source == m_sourceEntity) {
        snap.name = snap.name + "_copy";
    }
    snap.parentGuid = parentGuid;

    m_snapshots.push_back(snap);

    if (source == m_sourceEntity) {
        m_duplicatedEntity = newEnt;
    }

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(source);
    if (node) {
        for (const auto& child : node->GetChildren()) {
            if (child->GetEntity() != ECS::NULL_ENTITY) {
                DuplicateSubtree(child->GetEntity(), newGuid);
            }
        }
    }
}

void DuplicateEntityCommand::Execute() {
    if (m_firstRun) {
        // Find parent GUID of source entity if it has one
        Save::EntityGUID parentGuid = Save::NULL_GUID;
        auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_sourceEntity);
        if (node && node->GetParent() && node->GetParent() != Scene::SceneManager::Get().GetRootNode()) {
            ECS::Entity parentEnt = node->GetParent()->GetEntity();
            if (parentEnt != ECS::NULL_ENTITY) {
                parentGuid = m_registry->GetGUID(parentEnt);
            }
        }
        
        DuplicateSubtree(m_sourceEntity, parentGuid);
        m_firstRun = false;
    }

    for (const auto& snap : m_snapshots) {
        RestoreEntity(m_registry, snap);
    }
}

void DuplicateEntityCommand::Undo() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_duplicatedEntity);
    if (node) {
        if (SelectionSystem::Get().GetSelected() == m_duplicatedEntity) {
            SelectionSystem::Get().ClearSelection();
        }
        Scene::SceneManager::Get().DestroyNode(node);
    }
}

// RenameEntityCommand

RenameEntityCommand::RenameEntityCommand(ECS::Registry* registry, ECS::Entity entity, const std::string& newName)
    : m_registry(registry), m_entity(entity), m_newName(newName) {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        m_oldName = node->GetName();
    }
}

void RenameEntityCommand::Execute() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (node) {
        node->SetName(m_newName);
    }
}

void RenameEntityCommand::Undo() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (node) {
        node->SetName(m_oldName);
    }
}

// ParentEntityCommand

ParentEntityCommand::ParentEntityCommand(ECS::Registry* registry, ECS::Entity entity, ECS::Entity newParentEntity)
    : m_registry(registry), m_entity(entity), m_newParentEntity(newParentEntity) {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (node) {
        auto* parent = node->GetParent();
        if (parent && parent != Scene::SceneManager::Get().GetRootNode()) {
            m_oldParentEntity = parent->GetEntity();
        } else {
            m_oldParentEntity = ECS::NULL_ENTITY;
        }

        m_oldLocalPos = node->GetLocalPosition();
        m_oldLocalRot = node->GetLocalRotation();
        m_oldLocalScale = node->GetLocalScale();
    }
}

void ParentEntityCommand::Execute() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (!node) return;

    auto* parent = node->GetParent();
    auto* root = Scene::SceneManager::Get().GetRootNode();
    auto* newParentNode = root;

    if (m_newParentEntity != ECS::NULL_ENTITY) {
        newParentNode = Scene::SceneManager::Get().GetNodeByEntity(m_newParentEntity);
    }

    if (!newParentNode) return;

    // Calculate new local transform to preserve world transform
    glm::mat4 worldMat = node->GetWorldMatrix();
    glm::mat4 newParentWorldMat = newParentNode->GetWorldMatrix();
    glm::mat4 localMat = glm::inverse(newParentWorldMat) * worldMat;

    // Extract position
    m_newLocalPos = glm::vec3(localMat[3]);

    // Extract scale
    m_newLocalScale.x = glm::length(glm::vec3(localMat[0]));
    m_newLocalScale.y = glm::length(glm::vec3(localMat[1]));
    m_newLocalScale.z = glm::length(glm::vec3(localMat[2]));

    // Extract rotation
    glm::mat3 rotMat;
    rotMat[0] = glm::vec3(localMat[0]) / m_newLocalScale.x;
    rotMat[1] = glm::vec3(localMat[1]) / m_newLocalScale.y;
    rotMat[2] = glm::vec3(localMat[2]) / m_newLocalScale.z;
    m_newLocalRot = glm::quat_cast(rotMat);

    // Remove child from parent unique_ptr list
    std::unique_ptr<Scene::SceneNode> nodePtr;
    if (parent) {
        nodePtr = parent->RemoveChild(node);
    }

    if (nodePtr) {
        newParentNode->AddChild(std::move(nodePtr));
        node->SetLocalPosition(m_newLocalPos);
        node->SetLocalRotation(m_newLocalRot);
        node->SetLocalScale(m_newLocalScale);

        // Also update transform component in registry
        if (m_registry->HasComponent<Scene::TransformComponent>(m_entity)) {
            auto& tc = m_registry->GetComponent<Scene::TransformComponent>(m_entity);
            tc.position = m_newLocalPos;
            tc.rotation = m_newLocalRot;
            tc.scale = m_newLocalScale;
        }
    }
}

void ParentEntityCommand::Undo() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (!node) return;

    auto* parent = node->GetParent();
    auto* root = Scene::SceneManager::Get().GetRootNode();
    auto* oldParentNode = root;

    if (m_oldParentEntity != ECS::NULL_ENTITY) {
        oldParentNode = Scene::SceneManager::Get().GetNodeByEntity(m_oldParentEntity);
    }

    if (!oldParentNode) return;

    std::unique_ptr<Scene::SceneNode> nodePtr;
    if (parent) {
        nodePtr = parent->RemoveChild(node);
    }

    if (nodePtr) {
        oldParentNode->AddChild(std::move(nodePtr));
        node->SetLocalPosition(m_oldLocalPos);
        node->SetLocalRotation(m_oldLocalRot);
        node->SetLocalScale(m_oldLocalScale);

        if (m_registry->HasComponent<Scene::TransformComponent>(m_entity)) {
            auto& tc = m_registry->GetComponent<Scene::TransformComponent>(m_entity);
            tc.position = m_oldLocalPos;
            tc.rotation = m_oldLocalRot;
            tc.scale = m_oldLocalScale;
        }
    }
}

// ReorderChildrenCommand

ReorderChildrenCommand::ReorderChildrenCommand(ECS::Registry* registry, ECS::Entity parentEntity, int oldIndex, int newIndex)
    : m_registry(registry), m_parentEntity(parentEntity), m_oldIndex(oldIndex), m_newIndex(newIndex) {}

void ReorderChildrenCommand::Execute() {
    auto* parentNode = Scene::SceneManager::Get().GetRootNode();
    if (m_parentEntity != ECS::NULL_ENTITY) {
        parentNode = Scene::SceneManager::Get().GetNodeByEntity(m_parentEntity);
    }

    if (parentNode) {
        // Expose or dynamically manipulate m_children order
        // Because SceneNode returns a const reference to unique_ptr list,
        // we can cast away constness to do the reordering.
        auto& children = const_cast<std::vector<std::unique_ptr<Scene::SceneNode>>&>(parentNode->GetChildren());
        if (m_oldIndex >= 0 && m_oldIndex < static_cast<int>(children.size()) &&
            m_newIndex >= 0 && m_newIndex < static_cast<int>(children.size())) {
            
            auto temp = std::move(children[m_oldIndex]);
            children.erase(children.begin() + m_oldIndex);
            children.insert(children.begin() + m_newIndex, std::move(temp));
        }
    }
}

void ReorderChildrenCommand::Undo() {
    auto* parentNode = Scene::SceneManager::Get().GetRootNode();
    if (m_parentEntity != ECS::NULL_ENTITY) {
        parentNode = Scene::SceneManager::Get().GetNodeByEntity(m_parentEntity);
    }

    if (parentNode) {
        auto& children = const_cast<std::vector<std::unique_ptr<Scene::SceneNode>>&>(parentNode->GetChildren());
        if (m_newIndex >= 0 && m_newIndex < static_cast<int>(children.size()) &&
            m_oldIndex >= 0 && m_oldIndex < static_cast<int>(children.size())) {
            
            auto temp = std::move(children[m_newIndex]);
            children.erase(children.begin() + m_newIndex);
            children.insert(children.begin() + m_oldIndex, std::move(temp));
        }
    }
}

// AddComponentCommand

AddComponentCommand::AddComponentCommand(ECS::Registry* registry, ECS::Entity entity, ComponentType type, const std::string& extraPath)
    : m_registry(registry), m_entity(entity), m_type(type), m_extraPath(extraPath) {}

void AddComponentCommand::Execute() {
    if (m_type == ComponentType::Transform) {
        if (!m_registry->HasComponent<Scene::TransformComponent>(m_entity)) {
            m_registry->AddComponent<Scene::TransformComponent>(m_entity);
        }
    } else if (m_type == ComponentType::Camera) {
        if (!m_registry->HasComponent<Camera::CameraComponent>(m_entity)) {
            m_registry->AddComponent<Camera::CameraComponent>(m_entity);
        }
    } else if (m_type == ComponentType::Light) {
        if (!m_registry->HasComponent<Lighting::LightComponent>(m_entity)) {
            m_registry->AddComponent<Lighting::LightComponent>(m_entity);
        }
    } else if (m_type == ComponentType::MeshRenderer) {
        if (!m_registry->HasComponent<Renderer::MeshRendererComponent>(m_entity)) {
            m_registry->AddComponent<Renderer::MeshRendererComponent>(m_entity, m_extraPath);
        }
    } else if (m_type == ComponentType::Physics) {
        if (!m_registry->HasComponent<Physics::PhysicsComponent>(m_entity)) {
            m_registry->AddComponent<Physics::PhysicsComponent>(m_entity);
        }
    } else if (m_type == ComponentType::Script) {
        if (!m_registry->HasComponent<ECS::ScriptComponent>(m_entity)) {
            m_registry->AddComponent<ECS::ScriptComponent>(m_entity, m_extraPath);
            Scripting::ScriptEngine::Get().OnCreateEntity(m_entity);
        }
    }
}

void AddComponentCommand::Undo() {
    if (m_type == ComponentType::Transform) {
        m_registry->RemoveComponent<Scene::TransformComponent>(m_entity);
    } else if (m_type == ComponentType::Camera) {
        m_registry->RemoveComponent<Camera::CameraComponent>(m_entity);
    } else if (m_type == ComponentType::Light) {
        m_registry->RemoveComponent<Lighting::LightComponent>(m_entity);
    } else if (m_type == ComponentType::MeshRenderer) {
        m_registry->RemoveComponent<Renderer::MeshRendererComponent>(m_entity);
    } else if (m_type == ComponentType::Physics) {
        m_registry->RemoveComponent<Physics::PhysicsComponent>(m_entity);
    } else if (m_type == ComponentType::Script) {
        m_registry->RemoveComponent<ECS::ScriptComponent>(m_entity);
    }
}

// RemoveComponentCommand

RemoveComponentCommand::RemoveComponentCommand(ECS::Registry* registry, ECS::Entity entity, ComponentType type)
    : m_registry(registry), m_entity(entity), m_type(type) {}

void RemoveComponentCommand::Execute() {
    m_snapshot = CaptureEntity(m_registry, m_entity);

    if (m_type == ComponentType::Transform) {
        m_registry->RemoveComponent<Scene::TransformComponent>(m_entity);
    } else if (m_type == ComponentType::Camera) {
        m_registry->RemoveComponent<Camera::CameraComponent>(m_entity);
    } else if (m_type == ComponentType::Light) {
        m_registry->RemoveComponent<Lighting::LightComponent>(m_entity);
    } else if (m_type == ComponentType::MeshRenderer) {
        m_registry->RemoveComponent<Renderer::MeshRendererComponent>(m_entity);
    } else if (m_type == ComponentType::Physics) {
        m_registry->RemoveComponent<Physics::PhysicsComponent>(m_entity);
    } else if (m_type == ComponentType::Script) {
        m_registry->RemoveComponent<ECS::ScriptComponent>(m_entity);
    }
}

void RemoveComponentCommand::Undo() {
    if (m_type == ComponentType::Transform && m_snapshot.hasTransform) {
        m_registry->AddComponent<Scene::TransformComponent>(m_entity, m_snapshot.transform);
    } else if (m_type == ComponentType::Camera && m_snapshot.hasCamera) {
        m_registry->AddComponent<Camera::CameraComponent>(m_entity, m_snapshot.camera);
    } else if (m_type == ComponentType::Light && m_snapshot.hasLight) {
        m_registry->AddComponent<Lighting::LightComponent>(m_entity, m_snapshot.light);
    } else if (m_type == ComponentType::MeshRenderer && m_snapshot.hasMeshRenderer) {
        m_registry->AddComponent<Renderer::MeshRendererComponent>(m_entity, m_snapshot.meshRenderer);
    } else if (m_type == ComponentType::Physics && m_snapshot.hasPhysics) {
        m_registry->AddComponent<Physics::PhysicsComponent>(m_entity, m_snapshot.physics);
    } else if (m_type == ComponentType::Script && m_snapshot.hasScript) {
        m_registry->AddComponent<ECS::ScriptComponent>(m_entity, m_snapshot.scriptPath);
        Scripting::ScriptEngine::Get().OnCreateEntity(m_entity);
        if (!m_snapshot.scriptStateData.empty()) {
            std::stringstream tempStream(m_snapshot.scriptStateData);
            Save::BinaryReader tempReader(tempStream);
            Scripting::ScriptEngine::Get().DeserializeScriptState(m_entity, tempReader);
        }
    }
}

// ModifyTransformCommand

ModifyTransformCommand::ModifyTransformCommand(ECS::Registry* registry, ECS::Entity entity,
                                               const glm::vec3& oldPos, const glm::quat& oldRot, const glm::vec3& oldScale,
                                               const glm::vec3& newPos, const glm::quat& newRot, const glm::vec3& newScale)
    : m_registry(registry), m_entity(entity),
      m_oldPos(oldPos), m_newPos(newPos),
      m_oldRot(oldRot), m_newRot(newRot),
      m_oldScale(oldScale), m_newScale(newScale) {}

void ModifyTransformCommand::Execute() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (node) {
        node->SetLocalPosition(m_newPos);
        node->SetLocalRotation(m_newRot);
        node->SetLocalScale(m_newScale);
    }
    if (m_registry->HasComponent<Scene::TransformComponent>(m_entity)) {
        auto& tc = m_registry->GetComponent<Scene::TransformComponent>(m_entity);
        tc.position = m_newPos;
        tc.rotation = m_newRot;
        tc.scale = m_newScale;
    }
}

void ModifyTransformCommand::Undo() {
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(m_entity);
    if (node) {
        node->SetLocalPosition(m_oldPos);
        node->SetLocalRotation(m_oldRot);
        node->SetLocalScale(m_oldScale);
    }
    if (m_registry->HasComponent<Scene::TransformComponent>(m_entity)) {
        auto& tc = m_registry->GetComponent<Scene::TransformComponent>(m_entity);
        tc.position = m_oldPos;
        tc.rotation = m_oldRot;
        tc.scale = m_oldScale;
    }
}

// ModifyCameraCommand

ModifyCameraCommand::ModifyCameraCommand(ECS::Registry* registry, ECS::Entity entity, const Camera::CameraComponent& oldVal, const Camera::CameraComponent& newVal)
    : m_registry(registry), m_entity(entity), m_oldVal(oldVal), m_newVal(newVal) {}

void ModifyCameraCommand::Execute() {
    if (m_registry->HasComponent<Camera::CameraComponent>(m_entity)) {
        m_registry->GetComponent<Camera::CameraComponent>(m_entity) = m_newVal;
    }
}

void ModifyCameraCommand::Undo() {
    if (m_registry->HasComponent<Camera::CameraComponent>(m_entity)) {
        m_registry->GetComponent<Camera::CameraComponent>(m_entity) = m_oldVal;
    }
}

// ModifyLightCommand

ModifyLightCommand::ModifyLightCommand(ECS::Registry* registry, ECS::Entity entity, const Lighting::LightComponent& oldVal, const Lighting::LightComponent& newVal)
    : m_registry(registry), m_entity(entity), m_oldVal(oldVal), m_newVal(newVal) {}

void ModifyLightCommand::Execute() {
    if (m_registry->HasComponent<Lighting::LightComponent>(m_entity)) {
        m_registry->GetComponent<Lighting::LightComponent>(m_entity) = m_newVal;
    }
}

void ModifyLightCommand::Undo() {
    if (m_registry->HasComponent<Lighting::LightComponent>(m_entity)) {
        m_registry->GetComponent<Lighting::LightComponent>(m_entity) = m_oldVal;
    }
}

// ModifyMeshRendererCommand

ModifyMeshRendererCommand::ModifyMeshRendererCommand(ECS::Registry* registry, ECS::Entity entity, const Renderer::MeshRendererComponent& oldVal, const Renderer::MeshRendererComponent& newVal)
    : m_registry(registry), m_entity(entity), m_oldVal(oldVal), m_newVal(newVal) {}

void ModifyMeshRendererCommand::Execute() {
    if (m_registry->HasComponent<Renderer::MeshRendererComponent>(m_entity)) {
        m_registry->GetComponent<Renderer::MeshRendererComponent>(m_entity) = m_newVal;
    }
}

void ModifyMeshRendererCommand::Undo() {
    if (m_registry->HasComponent<Renderer::MeshRendererComponent>(m_entity)) {
        m_registry->GetComponent<Renderer::MeshRendererComponent>(m_entity) = m_oldVal;
    }
}

// ModifyPhysicsCommand

ModifyPhysicsCommand::ModifyPhysicsCommand(ECS::Registry* registry, ECS::Entity entity, const Physics::PhysicsComponent& oldVal, const Physics::PhysicsComponent& newVal)
    : m_registry(registry), m_entity(entity), m_oldVal(oldVal), m_newVal(newVal) {}

void ModifyPhysicsCommand::Execute() {
    if (m_registry->HasComponent<Physics::PhysicsComponent>(m_entity)) {
        m_registry->GetComponent<Physics::PhysicsComponent>(m_entity) = m_newVal;
    }
}

void ModifyPhysicsCommand::Undo() {
    if (m_registry->HasComponent<Physics::PhysicsComponent>(m_entity)) {
        m_registry->GetComponent<Physics::PhysicsComponent>(m_entity) = m_oldVal;
    }
}

// ModifyScriptCommand

ModifyScriptCommand::ModifyScriptCommand(ECS::Registry* registry, ECS::Entity entity, const std::string& oldPath, const std::string& newPath)
    : m_registry(registry), m_entity(entity), m_oldPath(oldPath), m_newPath(newPath) {}

void ModifyScriptCommand::Execute() {
    if (m_firstRun) {
        if (m_registry->HasComponent<ECS::ScriptComponent>(m_entity)) {
            std::stringstream tempStream;
            Save::BinaryWriter tempWriter(tempStream);
            Scripting::ScriptEngine::Get().SerializeScriptState(m_entity, tempWriter);
            m_oldStateData = tempStream.str();
        }
        m_firstRun = false;
    }

    if (m_registry->HasComponent<ECS::ScriptComponent>(m_entity)) {
        m_registry->RemoveComponent<ECS::ScriptComponent>(m_entity);
    }
    
    m_registry->AddComponent<ECS::ScriptComponent>(m_entity, m_newPath);
    Scripting::ScriptEngine::Get().OnCreateEntity(m_entity);
    if (!m_newStateData.empty()) {
        std::stringstream tempStream(m_newStateData);
        Save::BinaryReader tempReader(tempStream);
        Scripting::ScriptEngine::Get().DeserializeScriptState(m_entity, tempReader);
    }
}

void ModifyScriptCommand::Undo() {
    if (m_registry->HasComponent<ECS::ScriptComponent>(m_entity)) {
        std::stringstream tempStream;
        Save::BinaryWriter tempWriter(tempStream);
        Scripting::ScriptEngine::Get().SerializeScriptState(m_entity, tempWriter);
        m_newStateData = tempStream.str();
        m_registry->RemoveComponent<ECS::ScriptComponent>(m_entity);
    }

    m_registry->AddComponent<ECS::ScriptComponent>(m_entity, m_oldPath);
    Scripting::ScriptEngine::Get().OnCreateEntity(m_entity);
    if (!m_oldStateData.empty()) {
        std::stringstream tempStream(m_oldStateData);
        Save::BinaryReader tempReader(tempStream);
        Scripting::ScriptEngine::Get().DeserializeScriptState(m_entity, tempReader);
    }
}

// ModifyTerrainHeightCommand

void ModifyTerrainHeightCommand::Execute() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetHeightEdits(m_newHeights);
    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

void ModifyTerrainHeightCommand::Undo() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetHeightEdits(m_oldHeights);
    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

// ModifyTerrainLayersCommand

void ModifyTerrainLayersCommand::Execute() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetLayerEdits(m_newWeights);
    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

void ModifyTerrainLayersCommand::Undo() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetLayerEdits(m_oldWeights);
    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

// ModifyTerrainVegetationCommand

void ModifyTerrainVegetationCommand::Execute() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetEditedVegetationChunks(m_newChunks);
    tm.SetPaintedVegetation(m_newVeg);

    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        if (m_newChunks.find(coord) != m_newChunks.end()) {
            auto it = m_newVeg.find(coord);
            if (it != m_newVeg.end()) {
                chunk->GetVegetation() = it->second;
            } else {
                chunk->GetVegetation().clear();
            }
        } else {
            tm.PopulateChunkVegetation(chunk.get());
        }
    }
}

void ModifyTerrainVegetationCommand::Undo() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetEditedVegetationChunks(m_oldChunks);
    tm.SetPaintedVegetation(m_oldVeg);

    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        if (m_oldChunks.find(coord) != m_oldChunks.end()) {
            auto it = m_oldVeg.find(coord);
            if (it != m_oldVeg.end()) {
                chunk->GetVegetation() = it->second;
            } else {
                chunk->GetVegetation().clear();
            }
        } else {
            tm.PopulateChunkVegetation(chunk.get());
        }
    }
}

// ModifyTerrainSplineCommand

void ModifyTerrainSplineCommand::Execute() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetRoads(m_newRoads);
    tm.SetRivers(m_newRivers);
    tm.SetHeightEdits(m_newHeights);
    tm.SetLayerEdits(m_newWeights);

    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

void ModifyTerrainSplineCommand::Undo() {
    auto& tm = Terrain::TerrainManager::Get();
    tm.SetRoads(m_oldRoads);
    tm.SetRivers(m_oldRivers);
    tm.SetHeightEdits(m_oldHeights);
    tm.SetLayerEdits(m_oldWeights);

    for (auto& [coord, chunk] : tm.GetActiveChunks()) {
        chunk->Regenerate(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

} // namespace KumariEngine::Editor
