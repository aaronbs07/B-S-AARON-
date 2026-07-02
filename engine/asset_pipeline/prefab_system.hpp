#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "ecs/ecs.hpp"
#include "scene/scene_node.hpp"
#include "save/EntityGUID.hpp"

namespace KumariEngine::Prefab {

struct PrefabOverride {
    uint16_t componentTypeId;
    std::string fieldName; // e.g. "color", "visible"
    std::string value;     // serialized binary value or string representation
};

// Component attached to instantiated entities to track prefab linkage
struct PrefabInstanceComponent {
    std::string prefabAssetGuid; // GUID of the prefab asset
    Save::EntityGUID prefabEntityId; // Original GUID in the prefab definition
    bool isRoot = false;
    std::vector<PrefabOverride> overrides;
};

class PrefabSystem {
public:
    static PrefabSystem& Get() {
        static PrefabSystem instance;
        return instance;
    }

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Prefab Operations
    bool CreatePrefab(const std::string& filepath, ECS::Entity rootEntity);
    ECS::Entity InstantiatePrefab(const std::string& prefabAssetGuid, Scene::SceneNode* parent = nullptr);
    
    // Overrides Management
    void TrackOverride(ECS::Entity entity, uint16_t componentTypeId, const std::string& fieldName, const std::string& value);
    void ApplyOverrides(ECS::Entity instanceRoot);
    void RevertOverrides(ECS::Entity instanceRoot);

    // Prefab Variants
    bool CreatePrefabVariant(const std::string& filepath, const std::string& basePrefabAssetGuid, const std::vector<PrefabOverride>& variantOverrides);

private:
    PrefabSystem() = default;
    ~PrefabSystem() = default;

    ECS::Registry* m_registry = nullptr;
};

} // namespace KumariEngine::Prefab
