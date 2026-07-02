#include "prefab_system.hpp"
#include "asset_database.hpp"
#include "save/BinaryWriter.hpp"
#include "save/BinaryReader.hpp"
#include "save/SaveSystem.hpp"
#include "save/EntityGUID.hpp"
#include "scene/scene_manager.hpp"
#include "scene/transform_component.hpp"
#include "camera/camera_component.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "scripting/script_engine.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <sstream>

namespace KumariEngine::Prefab {

void PrefabSystem::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    if (m_registry) {
        m_registry->RegisterComponent<PrefabInstanceComponent>();
    }
    Core::Logger::Info("PrefabSystem", "Prefab System initialized.");
}

void PrefabSystem::Shutdown() {
    m_registry = nullptr;
    Core::Logger::Info("PrefabSystem", "Prefab System shut down.");
}

static void GetHierarchyEntities(Scene::SceneNode* node, std::vector<Scene::SceneNode*>& outNodes) {
    if (!node) return;
    outNodes.push_back(node);
    for (const auto& child : node->GetChildren()) {
        GetHierarchyEntities(child.get(), outNodes);
    }
}

bool PrefabSystem::CreatePrefab(const std::string& filepath, ECS::Entity rootEntity) {
    if (!m_registry) return false;

    auto* rootNode = Scene::SceneManager::Get().GetNodeByEntity(rootEntity);
    if (!rootNode) {
        Core::Logger::Error("PrefabSystem", "Failed to find SceneNode for prefab root entity.");
        return false;
    }

    std::vector<Scene::SceneNode*> nodes;
    GetHierarchyEntities(rootNode, nodes);

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        Core::Logger::Error("PrefabSystem", "Failed to open prefab file for writing: %s", filepath.c_str());
        return false;
    }

    Save::BinaryWriter writer(file);
    // Write header
    writer.WriteBytes("KMPR", 4);
    writer.WriteUint32(1); // Version 1

    // Write entity count
    writer.WriteUint32(static_cast<uint32_t>(nodes.size()));

    for (auto* node : nodes) {
        ECS::Entity entity = node->GetEntity();
        
        // Retrieve or assign stable GUID for prefab context
        Save::EntityGUID entityGuid;
        if (m_registry->HasComponent<PrefabInstanceComponent>(entity)) {
            entityGuid = m_registry->GetComponent<PrefabInstanceComponent>(entity).prefabEntityId;
        } else {
            entityGuid = m_registry->GetGUID(entity);
            if (entityGuid.IsNull()) {
                entityGuid = m_registry->CreateGUID(entity);
            }
        }

        writer.WriteUint64(entityGuid.high);
        writer.WriteUint64(entityGuid.low);

        // Parent GUID
        Save::EntityGUID parentGuid = Save::NULL_GUID;
        auto* parentNode = node->GetParent();
        if (parentNode && parentNode->GetEntity() != ECS::NULL_ENTITY && node != rootNode) {
            ECS::Entity parentEnt = parentNode->GetEntity();
            if (m_registry->HasComponent<PrefabInstanceComponent>(parentEnt)) {
                parentGuid = m_registry->GetComponent<PrefabInstanceComponent>(parentEnt).prefabEntityId;
            } else {
                parentGuid = m_registry->GetGUID(parentEnt);
                if (parentGuid.IsNull()) {
                    parentGuid = m_registry->CreateGUID(parentEnt);
                }
            }
        }

        writer.WriteUint64(parentGuid.high);
        writer.WriteUint64(parentGuid.low);

        writer.WriteString(node->GetName());

        // Component Count (excluding PrefabInstanceComponent in the source file, or we can serialize it)
        uint32_t numComponents = 0;
        if (m_registry->HasComponent<ECS::ScriptComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Scene::TransformComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Camera::CameraComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Lighting::LightComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Renderer::MeshRendererComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<PrefabInstanceComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Audio::AudioSourceComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Audio::AudioListenerComponent>(entity)) numComponents++;
        if (m_registry->HasComponent<Timeline::CinematicPlayerComponent>(entity)) numComponents++;

        writer.WriteUint32(numComponents);

        auto serializePayload = [&](uint16_t typeId, std::string& outPayload) {
            std::stringstream tempStream;
            Save::BinaryWriter tempWriter(tempStream);
            if (typeId == 1) {
                auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
                tempWriter.WriteString(sc.scriptPath);
                tempWriter.WriteUint64(entityGuid.high);
                tempWriter.WriteUint64(entityGuid.low);
                tempWriter.WriteBool(sc.initialized);
                Scripting::ScriptEngine::Get().SerializeScriptState(entity, tempWriter);
            } else if (typeId == 2) {
                auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                tempWriter.WriteFloat(tc.position.x);
                tempWriter.WriteFloat(tc.position.y);
                tempWriter.WriteFloat(tc.position.z);
                tempWriter.WriteFloat(tc.rotation.w);
                tempWriter.WriteFloat(tc.rotation.x);
                tempWriter.WriteFloat(tc.rotation.y);
                tempWriter.WriteFloat(tc.rotation.z);
                tempWriter.WriteFloat(tc.scale.x);
                tempWriter.WriteFloat(tc.scale.y);
                tempWriter.WriteFloat(tc.scale.z);
            } else if (typeId == 3) {
                auto& cc = m_registry->GetComponent<Camera::CameraComponent>(entity);
                tempWriter.WriteInt32(static_cast<int32_t>(cc.mode));
                tempWriter.WriteFloat(cc.fov);
                tempWriter.WriteFloat(cc.aspect);
                tempWriter.WriteFloat(cc.nearClip);
                tempWriter.WriteFloat(cc.farClip);
                tempWriter.WriteInt32(cc.priority);
                tempWriter.WriteBool(cc.collisionEnabled);
            } else if (typeId == 4) {
                auto& lc = m_registry->GetComponent<Lighting::LightComponent>(entity);
                tempWriter.WriteInt32(static_cast<int32_t>(lc.type));
                tempWriter.WriteFloat(lc.color.x);
                tempWriter.WriteFloat(lc.color.y);
                tempWriter.WriteFloat(lc.color.z);
                tempWriter.WriteFloat(lc.intensity);
                tempWriter.WriteFloat(lc.radius);
                tempWriter.WriteFloat(lc.innerCutoff);
                tempWriter.WriteFloat(lc.outerCutoff);
            } else if (typeId == 5) {
                auto& mrc = m_registry->GetComponent<Renderer::MeshRendererComponent>(entity);
                tempWriter.WriteString(mrc.meshPath);
                tempWriter.WriteString(mrc.materialPath);
                tempWriter.WriteBool(mrc.visible);
                tempWriter.WriteBool(mrc.castShadows);
            } else if (typeId == 6) {
                auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(entity);
                tempWriter.WriteInt32(static_cast<int32_t>(pc.bodyType));
                tempWriter.WriteInt32(static_cast<int32_t>(pc.collider.type));
                if (pc.collider.type == Physics::ColliderType::AABB) {
                    auto aabb = std::get<Physics::AABB>(pc.collider.shape);
                    tempWriter.WriteFloat(aabb.min.x);
                    tempWriter.WriteFloat(aabb.min.y);
                    tempWriter.WriteFloat(aabb.min.z);
                    tempWriter.WriteFloat(aabb.max.x);
                    tempWriter.WriteFloat(aabb.max.y);
                    tempWriter.WriteFloat(aabb.max.z);
                } else if (pc.collider.type == Physics::ColliderType::Sphere) {
                    auto sphere = std::get<Physics::Sphere>(pc.collider.shape);
                    tempWriter.WriteFloat(sphere.center.x);
                    tempWriter.WriteFloat(sphere.center.y);
                    tempWriter.WriteFloat(sphere.center.z);
                    tempWriter.WriteFloat(sphere.radius);
                } else if (pc.collider.type == Physics::ColliderType::Capsule) {
                    auto capsule = std::get<Physics::Capsule>(pc.collider.shape);
                    tempWriter.WriteFloat(capsule.center.x);
                    tempWriter.WriteFloat(capsule.halfHeight);
                    tempWriter.WriteFloat(capsule.radius);
                }
                tempWriter.WriteFloat(pc.mass);
                tempWriter.WriteFloat(pc.restitution);
                tempWriter.WriteFloat(pc.friction);
                tempWriter.WriteUint32(pc.collisionLayer);
                tempWriter.WriteUint32(pc.collisionMask);
            } else if (typeId == 7) {
                auto& pic = m_registry->GetComponent<PrefabInstanceComponent>(entity);
                tempWriter.WriteString(pic.prefabAssetGuid);
                tempWriter.WriteUint64(pic.prefabEntityId.high);
                tempWriter.WriteUint64(pic.prefabEntityId.low);
                tempWriter.WriteBool(pic.isRoot);
                tempWriter.WriteUint32(static_cast<uint32_t>(pic.overrides.size()));
                for (const auto& ov : pic.overrides) {
                    tempWriter.WriteUint16(ov.componentTypeId);
                    tempWriter.WriteString(ov.fieldName);
                    tempWriter.WriteString(ov.value);
                }
            } else if (typeId == 24) {
                auto& asc = m_registry->GetComponent<Audio::AudioSourceComponent>(entity);
                tempWriter.WriteString(asc.eventOrPath);
                tempWriter.WriteBool(asc.isPlaying);
                tempWriter.WriteBool(asc.loop);
                tempWriter.WriteBool(asc.spatial3D);
                tempWriter.WriteFloat(asc.pitch);
                tempWriter.WriteFloat(asc.volume);
                tempWriter.WriteFloat(asc.pan);
                tempWriter.WriteFloat(asc.minDistance);
                tempWriter.WriteFloat(asc.maxDistance);
            } else if (typeId == 25) {
                auto& alc = m_registry->GetComponent<Audio::AudioListenerComponent>(entity);
                tempWriter.WriteFloat(alc.position.x);
                tempWriter.WriteFloat(alc.position.y);
                tempWriter.WriteFloat(alc.position.z);
                tempWriter.WriteFloat(alc.forward.x);
                tempWriter.WriteFloat(alc.forward.y);
                tempWriter.WriteFloat(alc.forward.z);
                tempWriter.WriteFloat(alc.up.x);
                tempWriter.WriteFloat(alc.up.y);
                tempWriter.WriteFloat(alc.up.z);
            } else if (typeId == 26) {
                auto& cpc = m_registry->GetComponent<Timeline::CinematicPlayerComponent>(entity);
                tempWriter.WriteString(cpc.timelineName);
                tempWriter.WriteBool(cpc.isPlaying);
                tempWriter.WriteBool(cpc.isPaused);
                tempWriter.WriteFloat(cpc.currentTime);
                tempWriter.WriteBool(cpc.loop);
                tempWriter.WriteFloat(cpc.playbackSpeed);
            }
            outPayload = tempStream.str();
        };

        std::vector<uint16_t> activeTypes;
        if (m_registry->HasComponent<ECS::ScriptComponent>(entity)) activeTypes.push_back(1);
        if (m_registry->HasComponent<Scene::TransformComponent>(entity)) activeTypes.push_back(2);
        if (m_registry->HasComponent<Camera::CameraComponent>(entity)) activeTypes.push_back(3);
        if (m_registry->HasComponent<Lighting::LightComponent>(entity)) activeTypes.push_back(4);
        if (m_registry->HasComponent<Renderer::MeshRendererComponent>(entity)) activeTypes.push_back(5);
        if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) activeTypes.push_back(6);
        if (m_registry->HasComponent<PrefabInstanceComponent>(entity)) activeTypes.push_back(7);
        if (m_registry->HasComponent<Audio::AudioSourceComponent>(entity)) activeTypes.push_back(24);
        if (m_registry->HasComponent<Audio::AudioListenerComponent>(entity)) activeTypes.push_back(25);
        if (m_registry->HasComponent<Timeline::CinematicPlayerComponent>(entity)) activeTypes.push_back(26);

        for (uint16_t typeId : activeTypes) {
            std::string payload;
            serializePayload(typeId, payload);
            writer.WriteUint16(typeId);
            writer.WriteUint32(static_cast<uint32_t>(payload.size()));
            writer.WriteBytes(payload.data(), payload.size());
        }
    }

    file.close();
    Core::Logger::Info("PrefabSystem", "Prefab saved to: %s", filepath.c_str());
    
    // Scan asset database to register new prefab
    Asset::AssetDatabase::Get().Scan();
    return true;
}

ECS::Entity PrefabSystem::InstantiatePrefab(const std::string& prefabAssetGuid, Scene::SceneNode* parent) {
    if (!m_registry) return ECS::NULL_ENTITY;

    std::string path = Asset::AssetDatabase::Get().GetAssetPath(prefabAssetGuid);
    if (path.empty()) {
        Core::Logger::Error("PrefabSystem", "Failed to find asset path for GUID: %s", prefabAssetGuid.c_str());
        return ECS::NULL_ENTITY;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Core::Logger::Error("PrefabSystem", "Failed to open prefab file for reading: %s", path.c_str());
        return ECS::NULL_ENTITY;
    }

    Save::BinaryReader reader(file);
    char magic[4];
    if (!reader.ReadBytes(magic, 4) || std::string(magic, 4) != "KMPR") {
        Core::Logger::Error("PrefabSystem", "Invalid prefab magic in: %s", path.c_str());
        return ECS::NULL_ENTITY;
    }

    uint32_t version;
    reader.ReadUint32(version);

    uint32_t numEntities = 0;
    if (!reader.ReadUint32(numEntities)) {
        return ECS::NULL_ENTITY;
    }

    struct TempEntityInfo {
        ECS::Entity newEntity;
        Save::EntityGUID origGuid;
        Save::EntityGUID parentGuid;
        std::string name;
        uint32_t compCount;
        size_t componentsOffset;
    };

    std::vector<TempEntityInfo> tempEntities;
    tempEntities.reserve(numEntities);

    // Pass 1: Create ECS Entities and map GUIDs
    for (uint32_t i = 0; i < numEntities; ++i) {
        Save::EntityGUID origGuid;
        reader.ReadUint64(origGuid.high);
        reader.ReadUint64(origGuid.low);

        Save::EntityGUID parentGuid;
        reader.ReadUint64(parentGuid.high);
        reader.ReadUint64(parentGuid.low);

        std::string name;
        reader.ReadString(name);

        uint32_t compCount;
        reader.ReadUint32(compCount);

        ECS::Entity newEnt = m_registry->CreateEntity();
        
        // Generate new GUID for the scene instantiation to avoid collisions
        m_registry->CreateGUID(newEnt);

        // Assign hierarchy nodes
        auto* node = Scene::SceneManager::Get().CreateNode(name);
        node->SetEntity(newEnt);
        Scene::SceneManager::Get().RegisterEntityNode(newEnt, node);

        size_t currentOffset = reader.GetOffset();
        tempEntities.push_back({newEnt, origGuid, parentGuid, name, compCount, currentOffset});

        // Skip component payloads in stream
        for (uint32_t j = 0; j < compCount; ++j) {
            uint16_t typeId;
            uint32_t payloadSize;
            reader.ReadUint16(typeId);
            reader.ReadUint32(payloadSize);
            reader.Skip(payloadSize);
        }
    }

    // Pass 2: Establish parenting
    ECS::Entity rootInstanceEntity = ECS::NULL_ENTITY;
    std::unordered_map<Save::EntityGUID, ECS::Entity> origToNewMap;
    for (const auto& temp : tempEntities) {
        origToNewMap[temp.origGuid] = temp.newEntity;
    }

    for (const auto& temp : tempEntities) {
        auto* node = Scene::SceneManager::Get().GetNodeByEntity(temp.newEntity);
        if (!node) continue;

        bool hasParentInPrefab = false;
        if (!temp.parentGuid.IsNull()) {
            auto it = origToNewMap.find(temp.parentGuid);
            if (it != origToNewMap.end()) {
                auto* parentNode = Scene::SceneManager::Get().GetNodeByEntity(it->second);
                if (parentNode) {
                    // Re-parent node
                    auto* rootNode = Scene::SceneManager::Get().GetRootNode();
                    if (rootNode) {
                        auto nodePtr = rootNode->RemoveChild(node);
                        if (nodePtr) {
                            parentNode->AddChild(std::move(nodePtr));
                        }
                    }
                    hasParentInPrefab = true;
                }
            }
        }

        if (!hasParentInPrefab) {
            // This is the root entity of the prefab!
            rootInstanceEntity = temp.newEntity;
            if (parent) {
                auto* rootNode = Scene::SceneManager::Get().GetRootNode();
                if (rootNode) {
                    auto nodePtr = rootNode->RemoveChild(node);
                    if (nodePtr) {
                        parent->AddChild(std::move(nodePtr));
                    }
                }
            }
        }
    }

    // Pass 3: Deserialize components
    for (const auto& temp : tempEntities) {
        reader.Seek(temp.componentsOffset);
        
        for (uint32_t j = 0; j < temp.compCount; ++j) {
            uint16_t typeId;
            uint32_t payloadSize;
            reader.ReadUint16(typeId);
            reader.ReadUint32(payloadSize);

            size_t nextCompOffset = reader.GetOffset() + payloadSize;

            if (typeId == 1) { // ScriptComponent
                std::string scriptPath;
                Save::EntityGUID scriptGuid;
                bool initialized = false;
                reader.ReadString(scriptPath);
                reader.ReadUint64(scriptGuid.high);
                reader.ReadUint64(scriptGuid.low);
                reader.ReadBool(initialized);

                m_registry->AddComponent<ECS::ScriptComponent>(temp.newEntity, scriptPath);
                if (initialized) {
                    Scripting::ScriptEngine::Get().OnCreateEntity(temp.newEntity);
                }
                Scripting::ScriptEngine::Get().DeserializeScriptState(temp.newEntity, reader);
            } 
            else if (typeId == 2) { // TransformComponent
                glm::vec3 pos;
                glm::quat rot;
                glm::vec3 scale;
                reader.ReadFloat(pos.x); reader.ReadFloat(pos.y); reader.ReadFloat(pos.z);
                reader.ReadFloat(rot.w); reader.ReadFloat(rot.x); reader.ReadFloat(rot.y); reader.ReadFloat(rot.z);
                reader.ReadFloat(scale.x); reader.ReadFloat(scale.y); reader.ReadFloat(scale.z);
                
                m_registry->AddComponent<Scene::TransformComponent>(temp.newEntity, pos, rot, scale);
                auto* node = Scene::SceneManager::Get().GetNodeByEntity(temp.newEntity);
                if (node) {
                    node->SetLocalPosition(pos);
                    node->SetLocalRotation(rot);
                    node->SetLocalScale(scale);
                }
            } 
            else if (typeId == 3) { // CameraComponent
                int32_t modeVal;
                float fov, aspect, nearClip, farClip;
                int32_t priority;
                bool collision;
                reader.ReadInt32(modeVal);
                reader.ReadFloat(fov);
                reader.ReadFloat(aspect);
                reader.ReadFloat(nearClip);
                reader.ReadFloat(farClip);
                reader.ReadInt32(priority);
                reader.ReadBool(collision);

                auto& cc = m_registry->AddComponent<Camera::CameraComponent>(temp.newEntity);
                cc.mode = static_cast<Camera::CameraMode>(modeVal);
                cc.fov = fov;
                cc.aspect = aspect;
                cc.nearClip = nearClip;
                cc.farClip = farClip;
                cc.priority = priority;
                cc.collisionEnabled = collision;
            } 
            else if (typeId == 4) { // LightComponent
                int32_t typeVal;
                glm::vec3 color;
                float intensity, radius, inner, outer;
                reader.ReadInt32(typeVal);
                reader.ReadFloat(color.x); reader.ReadFloat(color.y); reader.ReadFloat(color.z);
                reader.ReadFloat(intensity);
                reader.ReadFloat(radius);
                reader.ReadFloat(inner);
                reader.ReadFloat(outer);

                auto& lc = m_registry->AddComponent<Lighting::LightComponent>(temp.newEntity);
                lc.type = static_cast<Lighting::LightType>(typeVal);
                lc.color = color;
                lc.intensity = intensity;
                lc.radius = radius;
                lc.innerCutoff = inner;
                lc.outerCutoff = outer;
            } 
            else if (typeId == 5) { // MeshRendererComponent
                std::string mesh, mat;
                bool visible, shadow;
                reader.ReadString(mesh);
                reader.ReadString(mat);
                reader.ReadBool(visible);
                reader.ReadBool(shadow);

                auto& mrc = m_registry->AddComponent<Renderer::MeshRendererComponent>(temp.newEntity);
                mrc.meshPath = mesh;
                mrc.materialPath = mat;
                mrc.visible = visible;
                mrc.castShadows = shadow;
            } 
            else if (typeId == 6) { // PhysicsComponent
                int32_t bodyVal, colVal;
                reader.ReadInt32(bodyVal);
                reader.ReadInt32(colVal);
                Physics::Collider collider;
                collider.type = static_cast<Physics::ColliderType>(colVal);
                if (collider.type == Physics::ColliderType::AABB) {
                    Physics::AABB aabb;
                    reader.ReadFloat(aabb.min.x); reader.ReadFloat(aabb.min.y); reader.ReadFloat(aabb.min.z);
                    reader.ReadFloat(aabb.max.x); reader.ReadFloat(aabb.max.y); reader.ReadFloat(aabb.max.z);
                    collider.shape = aabb;
                } else if (collider.type == Physics::ColliderType::Sphere) {
                    Physics::Sphere sphere;
                    reader.ReadFloat(sphere.center.x); reader.ReadFloat(sphere.center.y); reader.ReadFloat(sphere.center.z);
                    reader.ReadFloat(sphere.radius);
                    collider.shape = sphere;
                } else if (collider.type == Physics::ColliderType::Capsule) {
                    Physics::Capsule capsule;
                    reader.ReadFloat(capsule.center.x);
                    reader.ReadFloat(capsule.halfHeight);
                    reader.ReadFloat(capsule.radius);
                    collider.shape = capsule;
                }
                float mass, restitution, friction;
                uint32_t layer, mask;
                reader.ReadFloat(mass);
                reader.ReadFloat(restitution);
                reader.ReadFloat(friction);
                reader.ReadUint32(layer);
                reader.ReadUint32(mask);

                auto& pc = m_registry->AddComponent<Physics::PhysicsComponent>(temp.newEntity);
                pc.bodyType = static_cast<Physics::BodyType>(bodyVal);
                pc.collider = collider;
                pc.SetMass(mass);
                pc.restitution = restitution;
                pc.friction = friction;
                pc.collisionLayer = layer;
                pc.collisionMask = mask;
            }
            else if (typeId == 7) { // PrefabInstanceComponent
                std::string pGuid;
                Save::EntityGUID pEntityId;
                bool isRoot;
                uint32_t overridesCount;
                reader.ReadString(pGuid);
                reader.ReadUint64(pEntityId.high);
                reader.ReadUint64(pEntityId.low);
                reader.ReadBool(isRoot);
                reader.ReadUint32(overridesCount);

                std::vector<PrefabOverride> ovs;
                for (uint32_t k = 0; k < overridesCount; ++k) {
                    uint16_t cId;
                    std::string fName;
                    std::string fVal;
                    reader.ReadUint16(cId);
                    reader.ReadString(fName);
                    reader.ReadString(fVal);
                    ovs.push_back({cId, fName, fVal});
                }
                
                m_registry->AddComponent<PrefabInstanceComponent>(temp.newEntity, pGuid, pEntityId, isRoot, ovs);
            }
            else if (typeId == 24) {
                std::string eventOrPath;
                bool isPlaying, loop, spatial3D;
                float pitch, volume, pan, minDistance, maxDistance;
                reader.ReadString(eventOrPath);
                reader.ReadBool(isPlaying);
                reader.ReadBool(loop);
                reader.ReadBool(spatial3D);
                reader.ReadFloat(pitch);
                reader.ReadFloat(volume);
                reader.ReadFloat(pan);
                reader.ReadFloat(minDistance);
                reader.ReadFloat(maxDistance);

                auto& asc = m_registry->AddComponent<Audio::AudioSourceComponent>(temp.newEntity);
                asc.eventOrPath = eventOrPath;
                asc.isPlaying = isPlaying;
                asc.loop = loop;
                asc.spatial3D = spatial3D;
                asc.pitch = pitch;
                asc.volume = volume;
                asc.pan = pan;
                asc.minDistance = minDistance;
                asc.maxDistance = maxDistance;
            }
            else if (typeId == 25) {
                glm::vec3 position, forward, up;
                reader.ReadFloat(position.x); reader.ReadFloat(position.y); reader.ReadFloat(position.z);
                reader.ReadFloat(forward.x); reader.ReadFloat(forward.y); reader.ReadFloat(forward.z);
                reader.ReadFloat(up.x); reader.ReadFloat(up.y); reader.ReadFloat(up.z);

                auto& alc = m_registry->AddComponent<Audio::AudioListenerComponent>(temp.newEntity);
                alc.position = position;
                alc.forward = forward;
                alc.up = up;
            }
            else if (typeId == 26) {
                std::string timelineName;
                bool isPlaying, isPaused, loop;
                float currentTime, playbackSpeed;
                reader.ReadString(timelineName);
                reader.ReadBool(isPlaying);
                reader.ReadBool(isPaused);
                reader.ReadFloat(currentTime);
                reader.ReadBool(loop);
                reader.ReadFloat(playbackSpeed);

                auto& cpc = m_registry->AddComponent<Timeline::CinematicPlayerComponent>(temp.newEntity);
                cpc.timelineName = timelineName;
                cpc.isPlaying = isPlaying;
                cpc.isPaused = isPaused;
                cpc.currentTime = currentTime;
                cpc.loop = loop;
                cpc.playbackSpeed = playbackSpeed;
            }

            reader.Seek(nextCompOffset);
        }

        // Attach clean PrefabInstanceComponent tracking
        if (!m_registry->HasComponent<PrefabInstanceComponent>(temp.newEntity)) {
            bool isRoot = (temp.newEntity == rootInstanceEntity);
            m_registry->AddComponent<PrefabInstanceComponent>(temp.newEntity, prefabAssetGuid, temp.origGuid, isRoot, std::vector<PrefabOverride>{});
        }
    }

    file.close();
    
    // Register dependency from the active scene to this Prefab
    // Scene can register its dependencies dynamically
    
    Core::Logger::Info("PrefabSystem", "Instantiated prefab instance root entity %d from GUID: %s", rootInstanceEntity, prefabAssetGuid.c_str());
    return rootInstanceEntity;
}

void PrefabSystem::TrackOverride(ECS::Entity entity, uint16_t componentTypeId, const std::string& fieldName, const std::string& value) {
    if (!m_registry || !m_registry->HasComponent<PrefabInstanceComponent>(entity)) return;

    auto& pic = m_registry->GetComponent<PrefabInstanceComponent>(entity);
    
    // Check if duplicate override exists, overwrite if so
    auto it = std::find_if(pic.overrides.begin(), pic.overrides.end(), [&](const PrefabOverride& ov) {
        return ov.componentTypeId == componentTypeId && ov.fieldName == fieldName;
    });

    if (it != pic.overrides.end()) {
        it->value = value;
    } else {
        pic.overrides.push_back({componentTypeId, fieldName, value});
    }

    Core::Logger::Info("PrefabSystem", "Tracked override on entity %d: type %d, field %s", entity, componentTypeId, fieldName.c_str());
}

void PrefabSystem::ApplyOverrides(ECS::Entity instanceRoot) {
    if (!m_registry || !m_registry->HasComponent<PrefabInstanceComponent>(instanceRoot)) return;

    auto& pic = m_registry->GetComponent<PrefabInstanceComponent>(instanceRoot);
    std::string prefabGuid = pic.prefabAssetGuid;
    std::string filepath = Asset::AssetDatabase::Get().GetAssetPath(prefabGuid);

    if (filepath.empty()) {
        Core::Logger::Error("PrefabSystem", "Failed to apply overrides: Prefab path not found for GUID %s", prefabGuid.c_str());
        return;
    }

    // Save the entire current instance hierarchy to the prefab file, preserving the original entity GUIDs!
    CreatePrefab(filepath, instanceRoot);

    // Clear overrides on the applied hierarchy
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(instanceRoot);
    std::vector<Scene::SceneNode*> nodes;
    GetHierarchyEntities(node, nodes);

    for (auto* n : nodes) {
        ECS::Entity e = n->GetEntity();
        if (m_registry->HasComponent<PrefabInstanceComponent>(e)) {
            m_registry->GetComponent<PrefabInstanceComponent>(e).overrides.clear();
        }
    }

    Core::Logger::Info("PrefabSystem", "Applied overrides back to prefab: %s", filepath.c_str());

    // Automatically synchronize/re-instantiate other active instances of this prefab in the scene!
    auto allEntities = m_registry->GetAliveEntities();
    std::vector<ECS::Entity> instancesToRefresh;
    for (auto e : allEntities) {
        if (e != instanceRoot && m_registry->HasComponent<PrefabInstanceComponent>(e)) {
            auto& otherPic = m_registry->GetComponent<PrefabInstanceComponent>(e);
            if (otherPic.prefabAssetGuid == prefabGuid && otherPic.isRoot) {
                instancesToRefresh.push_back(e);
            }
        }
    }

    for (auto e : instancesToRefresh) {
        // Get parent and transform info before recreation
        auto* otherNode = Scene::SceneManager::Get().GetNodeByEntity(e);
        if (otherNode) {
            auto* parentNode = otherNode->GetParent();
            glm::vec3 pos = otherNode->GetLocalPosition();
            glm::quat rot = otherNode->GetLocalRotation();
            glm::vec3 scale = otherNode->GetLocalScale();

            // Destroy old instance
            RevertOverrides(e);

            // Re-instantiate
            ECS::Entity newRoot = InstantiatePrefab(prefabGuid, parentNode);
            if (newRoot != ECS::NULL_ENTITY) {
                auto* newNode = Scene::SceneManager::Get().GetNodeByEntity(newRoot);
                if (newNode) {
                    newNode->SetLocalPosition(pos);
                    newNode->SetLocalRotation(rot);
                    newNode->SetLocalScale(scale);
                }
            }
        }
    }
}

void PrefabSystem::RevertOverrides(ECS::Entity instanceRoot) {
    if (!m_registry || !m_registry->HasComponent<PrefabInstanceComponent>(instanceRoot)) return;

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(instanceRoot);
    if (!node) return;

    // Save placement details
    auto* parent = node->GetParent();
    
    // Collect all entities in the hierarchy to delete them
    std::vector<Scene::SceneNode*> nodes;
    GetHierarchyEntities(node, nodes);

    // Delete nodes and entities
    for (auto* n : nodes) {
        ECS::Entity e = n->GetEntity();
        if (e != ECS::NULL_ENTITY) {
            m_registry->DestroyEntity(e);
        }
    }

    // Destroy the scene node hierarchy from parent
    if (parent) {
        parent->RemoveChild(node);
    } else {
        auto* root = Scene::SceneManager::Get().GetRootNode();
        if (root) {
            root->RemoveChild(node);
        }
    }

    Core::Logger::Info("PrefabSystem", "Reverted overrides by destroying instance tree root entity %d", instanceRoot);
}

bool PrefabSystem::CreatePrefabVariant(const std::string& filepath, const std::string& basePrefabAssetGuid, const std::vector<PrefabOverride>& variantOverrides) {
    if (!m_registry) return false;

    // Instantiate base prefab temporarily
    ECS::Entity tempRoot = InstantiatePrefab(basePrefabAssetGuid, nullptr);
    if (tempRoot == ECS::NULL_ENTITY) {
        return false;
    }

    // Apply variant overrides to the temporary instance
    for (const auto& ov : variantOverrides) {
        TrackOverride(tempRoot, ov.componentTypeId, ov.fieldName, ov.value);
        
        // Directly apply override to temporary entity components for serialization
        if (ov.componentTypeId == 2 && ov.fieldName == "position") {
            auto& tc = m_registry->GetComponent<Scene::TransformComponent>(tempRoot);
            glm::vec3 pos;
            if (sscanf_s(ov.value.c_str(), "%f %f %f", &pos.x, &pos.y, &pos.z) == 3) {
                tc.position = pos;
            }
        }
        else if (ov.componentTypeId == 4 && ov.fieldName == "color") {
            auto& lc = m_registry->GetComponent<Lighting::LightComponent>(tempRoot);
            glm::vec3 color;
            if (sscanf_s(ov.value.c_str(), "%f %f %f", &color.x, &color.y, &color.z) == 3) {
                lc.color = color;
            }
        }
        else if (ov.componentTypeId == 5 && ov.fieldName == "visible") {
            auto& mrc = m_registry->GetComponent<Renderer::MeshRendererComponent>(tempRoot);
            mrc.visible = (ov.value == "true" || ov.value == "1");
        }
    }

    // Save as a new prefab file
    bool ok = CreatePrefab(filepath, tempRoot);

    // Clean up temporary instance
    RevertOverrides(tempRoot);

    return ok;
}

} // namespace KumariEngine::Prefab
