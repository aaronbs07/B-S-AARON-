#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"
#include "save/EntityGUID.hpp"
#include "scene/transform_component.hpp"
#include "camera/camera_component.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "terrain/terrain_manager.hpp"
#include <unordered_set>

namespace KumariEngine::Editor {

enum class ComponentType {
    Script = 1,
    Transform = 2,
    Camera = 3,
    Light = 4,
    MeshRenderer = 5,
    Physics = 6
};

struct EntitySnapshot {
    ECS::Entity entity = ECS::NULL_ENTITY;
    Save::EntityGUID guid;
    std::string name;
    Save::EntityGUID parentGuid;
    glm::vec3 localPosition{0.0f};
    glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 localScale{1.0f};

    bool hasTransform = false;
    Scene::TransformComponent transform;

    bool hasCamera = false;
    Camera::CameraComponent camera;

    bool hasLight = false;
    Lighting::LightComponent light;

    bool hasMeshRenderer = false;
    Renderer::MeshRendererComponent meshRenderer;

    bool hasPhysics = false;
    Physics::PhysicsComponent physics;

    bool hasScript = false;
    std::string scriptPath;
    std::string scriptStateData;
};

class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string GetDescription() const = 0;
};

class UndoSystem {
public:
    static UndoSystem& Get() {
        static UndoSystem instance;
        return instance;
    }

    UndoSystem(const UndoSystem&) = delete;
    UndoSystem& operator=(const UndoSystem&) = delete;

    void Execute(std::shared_ptr<Command> command);
    void Undo();
    void Redo();
    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }
    void Clear();

    bool IsDirty() const { return m_isDirty; }
    void SetDirty(bool dirty) { m_isDirty = dirty; }

private:
    UndoSystem() = default;
    ~UndoSystem() = default;

    std::vector<std::shared_ptr<Command>> m_undoStack;
    std::vector<std::shared_ptr<Command>> m_redoStack;
    bool m_isDirty = false;
};

// Snapshot capture helpers
EntitySnapshot CaptureEntity(ECS::Registry* registry, ECS::Entity entity);
void RestoreEntity(ECS::Registry* registry, const EntitySnapshot& snapshot);

// Concrete Commands

class CreateEntityCommand : public Command {
public:
    enum class Preset { Empty, Camera, DirLight, PointLight, SpotLight, Mesh, Physics };
    CreateEntityCommand(ECS::Registry* registry, Preset preset, const std::string& name, Save::EntityGUID parentGuid = Save::NULL_GUID);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Create Entity: " + m_name; }
    ECS::Entity GetCreatedEntity() const { return m_entity; }

private:
    ECS::Registry* m_registry;
    Preset m_preset;
    std::string m_name;
    Save::EntityGUID m_parentGuid;
    ECS::Entity m_entity = ECS::NULL_ENTITY;
    Save::EntityGUID m_guid;
    bool m_firstRun = true;
};

class DeleteEntityCommand : public Command {
public:
    DeleteEntityCommand(ECS::Registry* registry, ECS::Entity entity);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Delete Entity: " + m_rootName; }

private:
    void CaptureSubtree(ECS::Entity entity);

    ECS::Registry* m_registry;
    ECS::Entity m_rootEntity;
    std::string m_rootName;
    std::vector<EntitySnapshot> m_snapshots;
};

class DuplicateEntityCommand : public Command {
public:
    DuplicateEntityCommand(ECS::Registry* registry, ECS::Entity sourceEntity);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Duplicate Entity"; }
    ECS::Entity GetDuplicatedEntity() const { return m_duplicatedEntity; }

private:
    void DuplicateSubtree(ECS::Entity source, Save::EntityGUID parentGuid);

    ECS::Registry* m_registry;
    ECS::Entity m_sourceEntity;
    ECS::Entity m_duplicatedEntity = ECS::NULL_ENTITY;
    std::vector<EntitySnapshot> m_snapshots;
    bool m_firstRun = true;
};

class RenameEntityCommand : public Command {
public:
    RenameEntityCommand(ECS::Registry* registry, ECS::Entity entity, const std::string& newName);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Rename Entity"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    std::string m_oldName;
    std::string m_newName;
};

class ParentEntityCommand : public Command {
public:
    ParentEntityCommand(ECS::Registry* registry, ECS::Entity entity, ECS::Entity newParentEntity);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Reparent Entity"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    ECS::Entity m_oldParentEntity;
    ECS::Entity m_newParentEntity;

    glm::vec3 m_oldLocalPos, m_newLocalPos;
    glm::quat m_oldLocalRot, m_newLocalRot;
    glm::vec3 m_oldLocalScale, m_newLocalScale;
};

class ReorderChildrenCommand : public Command {
public:
    ReorderChildrenCommand(ECS::Registry* registry, ECS::Entity parentEntity, int oldIndex, int newIndex);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Reorder Children"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_parentEntity;
    int m_oldIndex;
    int m_newIndex;
};

class AddComponentCommand : public Command {
public:
    AddComponentCommand(ECS::Registry* registry, ECS::Entity entity, ComponentType type, const std::string& extraPath = "");
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Add Component"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    ComponentType m_type;
    std::string m_extraPath;
};

class RemoveComponentCommand : public Command {
public:
    RemoveComponentCommand(ECS::Registry* registry, ECS::Entity entity, ComponentType type);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Remove Component"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    ComponentType m_type;
    EntitySnapshot m_snapshot;
};

class ModifyTransformCommand : public Command {
public:
    ModifyTransformCommand(ECS::Registry* registry, ECS::Entity entity,
                           const glm::vec3& oldPos, const glm::quat& oldRot, const glm::vec3& oldScale,
                           const glm::vec3& newPos, const glm::quat& newRot, const glm::vec3& newScale);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Transform"; }

private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    glm::vec3 m_oldPos, m_newPos;
    glm::quat m_oldRot, m_newRot;
    glm::vec3 m_oldScale, m_newScale;
};

class ModifyCameraCommand : public Command {
public:
    ModifyCameraCommand(ECS::Registry* registry, ECS::Entity entity, const Camera::CameraComponent& oldVal, const Camera::CameraComponent& newVal);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Camera"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Camera::CameraComponent m_oldVal, m_newVal;
};

class ModifyLightCommand : public Command {
public:
    ModifyLightCommand(ECS::Registry* registry, ECS::Entity entity, const Lighting::LightComponent& oldVal, const Lighting::LightComponent& newVal);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Light"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Lighting::LightComponent m_oldVal, m_newVal;
};

class ModifyMeshRendererCommand : public Command {
public:
    ModifyMeshRendererCommand(ECS::Registry* registry, ECS::Entity entity, const Renderer::MeshRendererComponent& oldVal, const Renderer::MeshRendererComponent& newVal);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Mesh Renderer"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Renderer::MeshRendererComponent m_oldVal, m_newVal;
};

class ModifyPhysicsCommand : public Command {
public:
    ModifyPhysicsCommand(ECS::Registry* registry, ECS::Entity entity, const Physics::PhysicsComponent& oldVal, const Physics::PhysicsComponent& newVal);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Physics"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Physics::PhysicsComponent m_oldVal, m_newVal;
};

class ModifyScriptCommand : public Command {
public:
    ModifyScriptCommand(ECS::Registry* registry, ECS::Entity entity, const std::string& oldPath, const std::string& newPath);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Script Path"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    std::string m_oldPath, m_newPath;
    std::string m_oldStateData, m_newStateData; // captures serialized state
    bool m_firstRun = true;
};

class ModifyTerrainHeightCommand : public Command {
public:
    ModifyTerrainHeightCommand(const std::unordered_map<uint64_t, float>& oldHeights, const std::unordered_map<uint64_t, float>& newHeights)
        : m_oldHeights(oldHeights), m_newHeights(newHeights) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Terrain Height"; }
private:
    std::unordered_map<uint64_t, float> m_oldHeights;
    std::unordered_map<uint64_t, float> m_newHeights;
};

class ModifyTerrainLayersCommand : public Command {
public:
    ModifyTerrainLayersCommand(const std::unordered_map<uint64_t, glm::vec4>& oldWeights, const std::unordered_map<uint64_t, glm::vec4>& newWeights)
        : m_oldWeights(oldWeights), m_newWeights(newWeights) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Terrain Layers"; }
private:
    std::unordered_map<uint64_t, glm::vec4> m_oldWeights;
    std::unordered_map<uint64_t, glm::vec4> m_newWeights;
};

class ModifyTerrainVegetationCommand : public Command {
public:
    ModifyTerrainVegetationCommand(
        const std::unordered_set<Terrain::ChunkCoord, Terrain::ChunkCoordHash>& oldChunks,
        const std::unordered_set<Terrain::ChunkCoord, Terrain::ChunkCoordHash>& newChunks,
        const std::unordered_map<Terrain::ChunkCoord, std::vector<Terrain::VegetationInstance>, Terrain::ChunkCoordHash>& oldVeg,
        const std::unordered_map<Terrain::ChunkCoord, std::vector<Terrain::VegetationInstance>, Terrain::ChunkCoordHash>& newVeg)
        : m_oldChunks(oldChunks), m_newChunks(newChunks), m_oldVeg(oldVeg), m_newVeg(newVeg) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Terrain Vegetation"; }
private:
    std::unordered_set<Terrain::ChunkCoord, Terrain::ChunkCoordHash> m_oldChunks;
    std::unordered_set<Terrain::ChunkCoord, Terrain::ChunkCoordHash> m_newChunks;
    std::unordered_map<Terrain::ChunkCoord, std::vector<Terrain::VegetationInstance>, Terrain::ChunkCoordHash> m_oldVeg;
    std::unordered_map<Terrain::ChunkCoord, std::vector<Terrain::VegetationInstance>, Terrain::ChunkCoordHash> m_newVeg;
};

class ModifyTerrainSplineCommand : public Command {
public:
    ModifyTerrainSplineCommand(
        const std::vector<Terrain::TerrainManager::RoadData>& oldRoads, const std::vector<Terrain::TerrainManager::RoadData>& newRoads,
        const std::vector<Terrain::TerrainManager::RiverData>& oldRivers, const std::vector<Terrain::TerrainManager::RiverData>& newRivers,
        const std::unordered_map<uint64_t, float>& oldHeights, const std::unordered_map<uint64_t, float>& newHeights,
        const std::unordered_map<uint64_t, glm::vec4>& oldWeights, const std::unordered_map<uint64_t, glm::vec4>& newWeights)
        : m_oldRoads(oldRoads), m_newRoads(newRoads), m_oldRivers(oldRivers), m_newRivers(newRivers),
          m_oldHeights(oldHeights), m_newHeights(newHeights), m_oldWeights(oldWeights), m_newWeights(newWeights) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Terrain Splines"; }
private:
    std::vector<Terrain::TerrainManager::RoadData> m_oldRoads, m_newRoads;
    std::vector<Terrain::TerrainManager::RiverData> m_oldRivers, m_newRivers;
    std::unordered_map<uint64_t, float> m_oldHeights, m_newHeights;
    std::unordered_map<uint64_t, glm::vec4> m_oldWeights, m_newWeights;
};

} // namespace KumariEngine::Editor
