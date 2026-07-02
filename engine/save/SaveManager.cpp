#include "SaveManager.hpp"
#include "core/logger.hpp"
#include "save/SaveSystem.hpp"
#include "scripting/script_engine.hpp"
#include "scripting/script_component.hpp"
#include "scene/transform_component.hpp"
#include "camera/camera_component.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "physics/physics_components.hpp"
#include "scene/scene_manager.hpp"
#include "asset_pipeline/asset_database.hpp"
#include "asset_pipeline/prefab_system.hpp"
#include "terrain/terrain_manager.hpp"
#include "weather/environment_manager.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "gameplay/InventorySystem.hpp"
#include "gameplay/EquipmentSystem.hpp"
#include "gameplay/CraftingSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/DialogueSystem.hpp"
#include "gameplay/InteractionSystem.hpp"
#include <sstream>
#include <vector>

namespace KumariEngine::Save {

bool SaveManager::Initialize() {
    Core::Logger::Info("SaveManager", "Initializing SaveManager.");
    return true;
}

void SaveManager::Shutdown() {
    Core::Logger::Info("SaveManager", "Shutting down SaveManager.");
}

bool SaveManager::SaveGame(const std::string& filepath, ECS::Registry* registry) {
    if (!registry) {
        Core::Logger::Error("SaveManager", "Registry is null in SaveGame.");
        return false;
    }

    return SaveSystem::WriteSave(filepath, [&](BinaryWriter& writer) {
        auto aliveEntities = registry->GetAliveEntities();
        writer.WriteUint32(static_cast<uint32_t>(aliveEntities.size()));

        for (auto entity : aliveEntities) {
            EntityGUID guid = registry->GetGUID(entity);
            if (guid.IsNull()) {
                guid = registry->CreateGUID(entity);
            }

            writer.WriteUint64(guid.high);
            writer.WriteUint64(guid.low);

            // Version 3 addition: Name and Parent GUID
            std::string name = "Entity_" + std::to_string(entity);
            EntityGUID parentGuid = NULL_GUID;
            auto* node = (Scene::SceneManager::Get().GetRegistry() == registry) ?
                         Scene::SceneManager::Get().GetNodeByEntity(entity) : nullptr;
            if (node) {
                name = node->GetName();
                auto* parent = node->GetParent();
                if (parent && parent != Scene::SceneManager::Get().GetRootNode()) {
                    ECS::Entity parentEnt = parent->GetEntity();
                    if (parentEnt != ECS::NULL_ENTITY) {
                        parentGuid = registry->GetGUID(parentEnt);
                        if (parentGuid.IsNull()) {
                            parentGuid = registry->CreateGUID(parentEnt);
                        }
                    }
                }
            }

            writer.WriteString(name);
            writer.WriteUint64(parentGuid.high);
            writer.WriteUint64(parentGuid.low);

            // Compute Component Count
            uint32_t numComponents = 0;
            if (registry->HasComponent<ECS::ScriptComponent>(entity)) numComponents++;
            if (registry->HasComponent<Scene::TransformComponent>(entity)) numComponents++;
            if (registry->HasComponent<Camera::CameraComponent>(entity)) numComponents++;
            if (registry->HasComponent<Lighting::LightComponent>(entity)) numComponents++;
            if (registry->HasComponent<Renderer::MeshRendererComponent>(entity)) numComponents++;
            if (registry->HasComponent<Physics::PhysicsComponent>(entity)) numComponents++;
            if (registry->HasComponent<Prefab::PrefabInstanceComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::HealthComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::DamageComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::TeamComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::InteractionComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::SpawnPointComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::PlayerControllerComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::PlayerStateComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::GameStateComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::InventoryComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::ItemComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::EquipmentComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::QuestComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::DialogueComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::InteractableComponent>(entity)) numComponents++;
            if (registry->HasComponent<Gameplay::TriggerVolumeComponent>(entity)) numComponents++;
            if (registry->HasComponent<Audio::AudioSourceComponent>(entity)) numComponents++;
            if (registry->HasComponent<Audio::AudioListenerComponent>(entity)) numComponents++;
            if (registry->HasComponent<Timeline::CinematicPlayerComponent>(entity)) numComponents++;

            writer.WriteUint32(numComponents);

            auto serializeComponentPayload = [&](uint16_t typeId, std::string& outPayload) -> bool {
                std::stringstream tempStream;
                BinaryWriter tempWriter(tempStream);

                if (typeId == 1) { // ScriptComponent
                    auto& sc = registry->GetComponent<ECS::ScriptComponent>(entity);
                    std::string scriptPathOrGuid = sc.scriptPath;
                    std::string g = Asset::AssetDatabase::Get().GetAssetGuid(sc.scriptPath);
                    if (!g.empty()) scriptPathOrGuid = g;

                    tempWriter.WriteString(scriptPathOrGuid);
                    tempWriter.WriteUint64(guid.high);
                    tempWriter.WriteUint64(guid.low);
                    tempWriter.WriteBool(sc.initialized);
                    Scripting::ScriptEngine::Get().SerializeScriptState(entity, tempWriter);
                } else if (typeId == 2) { // TransformComponent
                    auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
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
                } else if (typeId == 3) { // CameraComponent
                    auto& cc = registry->GetComponent<Camera::CameraComponent>(entity);
                    tempWriter.WriteInt32(static_cast<int32_t>(cc.mode));
                    tempWriter.WriteFloat(cc.fov);
                    tempWriter.WriteFloat(cc.aspect);
                    tempWriter.WriteFloat(cc.nearClip);
                    tempWriter.WriteFloat(cc.farClip);
                    tempWriter.WriteInt32(cc.priority);
                    tempWriter.WriteBool(cc.collisionEnabled);
                } else if (typeId == 4) { // LightComponent
                    auto& lc = registry->GetComponent<Lighting::LightComponent>(entity);
                    tempWriter.WriteInt32(static_cast<int32_t>(lc.type));
                    tempWriter.WriteFloat(lc.color.x);
                    tempWriter.WriteFloat(lc.color.y);
                    tempWriter.WriteFloat(lc.color.z);
                    tempWriter.WriteFloat(lc.intensity);
                    tempWriter.WriteFloat(lc.radius);
                    tempWriter.WriteFloat(lc.innerCutoff);
                    tempWriter.WriteFloat(lc.outerCutoff);
                } else if (typeId == 5) { // MeshRendererComponent
                    auto& mrc = registry->GetComponent<Renderer::MeshRendererComponent>(entity);
                    std::string meshPathOrGuid = mrc.meshPath;
                    std::string mg = Asset::AssetDatabase::Get().GetAssetGuid(mrc.meshPath);
                    if (!mg.empty()) meshPathOrGuid = mg;

                    std::string matPathOrGuid = mrc.materialPath;
                    std::string matg = Asset::AssetDatabase::Get().GetAssetGuid(mrc.materialPath);
                    if (!matg.empty()) matPathOrGuid = matg;

                    tempWriter.WriteString(meshPathOrGuid);
                    tempWriter.WriteString(matPathOrGuid);
                    tempWriter.WriteBool(mrc.visible);
                    tempWriter.WriteBool(mrc.castShadows);
                } else if (typeId == 6) { // PhysicsComponent
                    auto& pc = registry->GetComponent<Physics::PhysicsComponent>(entity);
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
                } else if (typeId == 7) { // PrefabInstanceComponent
                    auto& pic = registry->GetComponent<Prefab::PrefabInstanceComponent>(entity);
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
                } else if (typeId == 8) { // HealthComponent
                    auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
                    tempWriter.WriteFloat(hc.currentHealth);
                    tempWriter.WriteFloat(hc.maxHealth);
                    tempWriter.WriteFloat(hc.shield);
                    tempWriter.WriteBool(hc.invulnerable);
                } else if (typeId == 9) { // DamageComponent
                    auto& dc = registry->GetComponent<Gameplay::DamageComponent>(entity);
                    tempWriter.WriteFloat(dc.damageAmount);
                    tempWriter.WriteString(dc.damageType);
                    tempWriter.WriteFloat(dc.multiplier);
                    tempWriter.WriteFloat(dc.knockbackForce);
                } else if (typeId == 10) { // TeamComponent
                    auto& tc = registry->GetComponent<Gameplay::TeamComponent>(entity);
                    tempWriter.WriteInt32(tc.teamId);
                    tempWriter.WriteBool(tc.friendlyFire);
                } else if (typeId == 11) { // InteractionComponent
                    auto& ic = registry->GetComponent<Gameplay::InteractionComponent>(entity);
                    tempWriter.WriteString(ic.prompt);
                    tempWriter.WriteFloat(ic.distance);
                    tempWriter.WriteBool(ic.isInteractable);
                } else if (typeId == 12) { // GameplayTagsComponent
                    auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
                    auto rawTags = gtc.GetRawTags();
                    tempWriter.WriteUint32(static_cast<uint32_t>(rawTags.size()));
                    for (const auto& tag : rawTags) {
                        tempWriter.WriteString(tag);
                    }
                } else if (typeId == 13) { // SpawnPointComponent
                    auto& sp = registry->GetComponent<Gameplay::SpawnPointComponent>(entity);
                    tempWriter.WriteString(sp.spawnGroup);
                    tempWriter.WriteBool(sp.isEnabled);
                } else if (typeId == 14) { // PlayerControllerComponent
                    auto& pcc = registry->GetComponent<Gameplay::PlayerControllerComponent>(entity);
                    tempWriter.WriteUint32(pcc.peerId);
                    tempWriter.WriteBool(pcc.isLocal);
                    
                    EntityGUID pawnGuid = NULL_GUID;
                    if (pcc.possessedPawn != ECS::NULL_ENTITY && registry->IsAlive(pcc.possessedPawn)) {
                        pawnGuid = registry->GetGUID(pcc.possessedPawn);
                        if (pawnGuid.IsNull()) pawnGuid = registry->CreateGUID(pcc.possessedPawn);
                    }
                    tempWriter.WriteUint64(pawnGuid.high);
                    tempWriter.WriteUint64(pawnGuid.low);

                    EntityGUID stateGuid = NULL_GUID;
                    if (pcc.playerStateEntity != ECS::NULL_ENTITY && registry->IsAlive(pcc.playerStateEntity)) {
                        stateGuid = registry->GetGUID(pcc.playerStateEntity);
                        if (stateGuid.IsNull()) stateGuid = registry->CreateGUID(pcc.playerStateEntity);
                    }
                    tempWriter.WriteUint64(stateGuid.high);
                    tempWriter.WriteUint64(stateGuid.low);
                } else if (typeId == 15) { // PlayerStateComponent
                    auto& psc = registry->GetComponent<Gameplay::PlayerStateComponent>(entity);
                    tempWriter.WriteString(psc.playerName);
                    tempWriter.WriteUint32(psc.peerId);
                    tempWriter.WriteFloat(psc.score);
                    tempWriter.WriteInt32(psc.teamId);
                    tempWriter.WriteFloat(psc.ping);
                } else if (typeId == 16) { // GameStateComponent
                    auto& gsc = registry->GetComponent<Gameplay::GameStateComponent>(entity);
                    tempWriter.WriteFloat(gsc.elapsedTime);
                    tempWriter.WriteBool(gsc.isMatchRunning);
                    tempWriter.WriteBool(gsc.isMatchOver);
                    tempWriter.WriteInt32(gsc.winnerTeamId);
                } else if (typeId == 17) { // InventoryComponent
                    auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
                    tempWriter.WriteUint32(ic.maxSlots);
                    tempWriter.WriteUint32(static_cast<uint32_t>(ic.slots.size()));
                    for (const auto& slot : ic.slots) {
                        tempWriter.WriteString(slot.itemId);
                        tempWriter.WriteUint32(slot.quantity);
                    }
                } else if (typeId == 18) { // ItemComponent
                    auto& ic = registry->GetComponent<Gameplay::ItemComponent>(entity);
                    tempWriter.WriteString(ic.itemId);
                    tempWriter.WriteUint32(ic.quantity);
                } else if (typeId == 19) { // EquipmentComponent
                    auto& ec = registry->GetComponent<Gameplay::EquipmentComponent>(entity);
                    tempWriter.WriteUint32(static_cast<uint32_t>(ec.slots.size()));
                    for (const auto& item : ec.slots) {
                        tempWriter.WriteString(item);
                    }
                } else if (typeId == 20) { // QuestComponent
                    auto& qc = registry->GetComponent<Gameplay::QuestComponent>(entity);
                    tempWriter.WriteUint32(static_cast<uint32_t>(qc.completedQuests.size()));
                    for (const auto& questId : qc.completedQuests) {
                        tempWriter.WriteString(questId);
                    }
                    tempWriter.WriteUint32(static_cast<uint32_t>(qc.activeQuests.size()));
                    for (const auto& [questId, state] : qc.activeQuests) {
                        tempWriter.WriteString(questId);
                        tempWriter.WriteInt32(state.currentStageIndex);
                        tempWriter.WriteBool(state.isCompleted);
                        tempWriter.WriteUint32(static_cast<uint32_t>(state.objectiveProgress.size()));
                        for (const auto& [objId, count] : state.objectiveProgress) {
                            tempWriter.WriteString(objId);
                            tempWriter.WriteInt32(count);
                        }
                    }
                } else if (typeId == 21) { // DialogueComponent
                    auto& dc = registry->GetComponent<Gameplay::DialogueComponent>(entity);
                    tempWriter.WriteString(dc.currentDialogueId);
                    tempWriter.WriteString(dc.currentNodeId);
                    tempWriter.WriteBool(dc.isInDialogue);
                } else if (typeId == 22) { // InteractableComponent
                    auto& ic = registry->GetComponent<Gameplay::InteractableComponent>(entity);
                    tempWriter.WriteString(ic.prompt);
                    tempWriter.WriteFloat(ic.distance);
                    tempWriter.WriteBool(ic.isInteractable);
                    tempWriter.WriteString(ic.interactionType);
                    tempWriter.WriteString(ic.targetData);
                    tempWriter.WriteString(ic.onInteractLua);
                } else if (typeId == 23) { // TriggerVolumeComponent
                    auto& tvc = registry->GetComponent<Gameplay::TriggerVolumeComponent>(entity);
                    tempWriter.WriteUint8(static_cast<uint8_t>(tvc.type));
                    tempWriter.WriteString(tvc.onEnterLua);
                    tempWriter.WriteString(tvc.onExitLua);
                } else if (typeId == 24) { // AudioSourceComponent
                    auto& asc = registry->GetComponent<Audio::AudioSourceComponent>(entity);
                    tempWriter.WriteString(asc.eventOrPath);
                    tempWriter.WriteBool(asc.isPlaying);
                    tempWriter.WriteBool(asc.loop);
                    tempWriter.WriteBool(asc.spatial3D);
                    tempWriter.WriteFloat(asc.pitch);
                    tempWriter.WriteFloat(asc.volume);
                    tempWriter.WriteFloat(asc.pan);
                    tempWriter.WriteFloat(asc.minDistance);
                    tempWriter.WriteFloat(asc.maxDistance);
                } else if (typeId == 25) { // AudioListenerComponent
                    auto& alc = registry->GetComponent<Audio::AudioListenerComponent>(entity);
                    tempWriter.WriteFloat(alc.position.x);
                    tempWriter.WriteFloat(alc.position.y);
                    tempWriter.WriteFloat(alc.position.z);
                    tempWriter.WriteFloat(alc.forward.x);
                    tempWriter.WriteFloat(alc.forward.y);
                    tempWriter.WriteFloat(alc.forward.z);
                    tempWriter.WriteFloat(alc.up.x);
                    tempWriter.WriteFloat(alc.up.y);
                    tempWriter.WriteFloat(alc.up.z);
                } else if (typeId == 26) { // CinematicPlayerComponent
                    auto& cpc = registry->GetComponent<Timeline::CinematicPlayerComponent>(entity);
                    tempWriter.WriteString(cpc.timelineName);
                    tempWriter.WriteBool(cpc.isPlaying);
                    tempWriter.WriteBool(cpc.isPaused);
                    tempWriter.WriteFloat(cpc.currentTime);
                    tempWriter.WriteBool(cpc.loop);
                    tempWriter.WriteFloat(cpc.playbackSpeed);
                }
 
                outPayload = tempStream.str();
                return true;
            };
 
            std::vector<uint16_t> activeTypes;
            if (registry->HasComponent<ECS::ScriptComponent>(entity)) activeTypes.push_back(1);
            if (registry->HasComponent<Scene::TransformComponent>(entity)) activeTypes.push_back(2);
            if (registry->HasComponent<Camera::CameraComponent>(entity)) activeTypes.push_back(3);
            if (registry->HasComponent<Lighting::LightComponent>(entity)) activeTypes.push_back(4);
            if (registry->HasComponent<Renderer::MeshRendererComponent>(entity)) activeTypes.push_back(5);
            if (registry->HasComponent<Physics::PhysicsComponent>(entity)) activeTypes.push_back(6);
            if (registry->HasComponent<Prefab::PrefabInstanceComponent>(entity)) activeTypes.push_back(7);
            if (registry->HasComponent<Gameplay::HealthComponent>(entity)) activeTypes.push_back(8);
            if (registry->HasComponent<Gameplay::DamageComponent>(entity)) activeTypes.push_back(9);
            if (registry->HasComponent<Gameplay::TeamComponent>(entity)) activeTypes.push_back(10);
            if (registry->HasComponent<Gameplay::InteractionComponent>(entity)) activeTypes.push_back(11);
            if (registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) activeTypes.push_back(12);
            if (registry->HasComponent<Gameplay::SpawnPointComponent>(entity)) activeTypes.push_back(13);
            if (registry->HasComponent<Gameplay::PlayerControllerComponent>(entity)) activeTypes.push_back(14);
            if (registry->HasComponent<Gameplay::PlayerStateComponent>(entity)) activeTypes.push_back(15);
            if (registry->HasComponent<Gameplay::GameStateComponent>(entity)) activeTypes.push_back(16);
            if (registry->HasComponent<Gameplay::InventoryComponent>(entity)) activeTypes.push_back(17);
            if (registry->HasComponent<Gameplay::ItemComponent>(entity)) activeTypes.push_back(18);
            if (registry->HasComponent<Gameplay::EquipmentComponent>(entity)) activeTypes.push_back(19);
            if (registry->HasComponent<Gameplay::QuestComponent>(entity)) activeTypes.push_back(20);
            if (registry->HasComponent<Gameplay::DialogueComponent>(entity)) activeTypes.push_back(21);
            if (registry->HasComponent<Gameplay::InteractableComponent>(entity)) activeTypes.push_back(22);
            if (registry->HasComponent<Gameplay::TriggerVolumeComponent>(entity)) activeTypes.push_back(23);
            if (registry->HasComponent<Audio::AudioSourceComponent>(entity)) activeTypes.push_back(24);
            if (registry->HasComponent<Audio::AudioListenerComponent>(entity)) activeTypes.push_back(25);
            if (registry->HasComponent<Timeline::CinematicPlayerComponent>(entity)) activeTypes.push_back(26);

            for (uint16_t typeId : activeTypes) {
                std::string payload;
                try {
                    if (serializeComponentPayload(typeId, payload)) {
                        writer.WriteUint16(typeId);
                        writer.WriteUint32(static_cast<uint32_t>(payload.size()));
                        writer.WriteBytes(payload.data(), payload.size());
                    }
                } catch (const std::exception& e) {
                    Core::Logger::Error("SaveManager", "Exception during payload serialization of component %d on entity %d: %s", typeId, entity, e.what());
                    throw;
                }
            }
        }

        // Save Terrain and Environment settings (unified save version 5 additions)
        auto& tm = Terrain::TerrainManager::Get();
        writer.WriteUint32(tm.GetSeed());
        writer.WriteFloat(tm.GetChunkSize());

        // Height edits
        const auto& heightEdits = tm.GetHeightEdits();
        writer.WriteUint32(static_cast<uint32_t>(heightEdits.size()));
        for (const auto& [key, height] : heightEdits) {
            writer.WriteUint64(key);
            writer.WriteFloat(height);
        }

        // Layer edits
        const auto& layerEdits = tm.GetLayerEdits();
        writer.WriteUint32(static_cast<uint32_t>(layerEdits.size()));
        for (const auto& [key, weights] : layerEdits) {
            writer.WriteUint64(key);
            writer.WriteFloat(weights.x);
            writer.WriteFloat(weights.y);
            writer.WriteFloat(weights.z);
            writer.WriteFloat(weights.w);
        }

        // Vegetation edits
        const auto& editedVegChunks = tm.GetEditedVegetationChunks();
        const auto& paintedVeg = tm.GetPaintedVegetation();
        writer.WriteUint32(static_cast<uint32_t>(editedVegChunks.size()));
        for (const auto& coord : editedVegChunks) {
            writer.WriteInt32(coord.x);
            writer.WriteInt32(coord.z);
            auto it = paintedVeg.find(coord);
            if (it != paintedVeg.end()) {
                writer.WriteUint32(static_cast<uint32_t>(it->second.size()));
                for (const auto& inst : it->second) {
                    writer.WriteInt32(inst.type);
                    writer.WriteFloat(inst.position.x);
                    writer.WriteFloat(inst.position.y);
                    writer.WriteFloat(inst.position.z);
                    writer.WriteFloat(inst.scale);
                    writer.WriteFloat(inst.rotation);
                }
            } else {
                writer.WriteUint32(0);
            }
        }

        // Roads
        const auto& roads = tm.GetRoads();
        writer.WriteUint32(static_cast<uint32_t>(roads.size()));
        for (const auto& road : roads) {
            writer.WriteFloat(road.width);
            writer.WriteInt32(road.roadType);
            writer.WriteUint32(static_cast<uint32_t>(road.splinePoints.size()));
            for (const auto& pt : road.splinePoints) {
                writer.WriteFloat(pt.x);
                writer.WriteFloat(pt.y);
                writer.WriteFloat(pt.z);
            }
        }

        // Rivers
        const auto& rivers = tm.GetRivers();
        writer.WriteUint32(static_cast<uint32_t>(rivers.size()));
        for (const auto& river : rivers) {
            writer.WriteFloat(river.width);
            writer.WriteFloat(river.depth);
            writer.WriteUint32(static_cast<uint32_t>(river.splinePoints.size()));
            for (const auto& pt : river.splinePoints) {
                writer.WriteFloat(pt.x);
                writer.WriteFloat(pt.y);
                writer.WriteFloat(pt.z);
            }
        }

        // Environment data
        auto& em = Environment::EnvironmentManager::Get();
        const auto& sky = em.GetSkySettings();
        writer.WriteFloat(sky.skyColor.x);
        writer.WriteFloat(sky.skyColor.y);
        writer.WriteFloat(sky.skyColor.z);
        writer.WriteFloat(sky.turbidity);
        writer.WriteFloat(sky.exposure);

        const auto& sunDir = em.GetSunDirection();
        writer.WriteFloat(sunDir.x);
        writer.WriteFloat(sunDir.y);
        writer.WriteFloat(sunDir.z);

        writer.WriteFloat(em.GetAmbientColor().x);
        writer.WriteFloat(em.GetAmbientColor().y);
        writer.WriteFloat(em.GetAmbientColor().z);
        writer.WriteFloat(em.GetAmbientIntensity());

        const auto& fog = em.GetFogSettings();
        writer.WriteBool(fog.enabled);
        writer.WriteFloat(fog.color.x);
        writer.WriteFloat(fog.color.y);
        writer.WriteFloat(fog.color.z);
        writer.WriteFloat(fog.density);
        writer.WriteFloat(fog.start);
        writer.WriteFloat(fog.end);

        const auto& weather = em.GetWeatherSettings();
        writer.WriteFloat(weather.rainIntensity);
        writer.WriteFloat(weather.windSpeed);
        writer.WriteFloat(weather.windDirection.x);
        writer.WriteFloat(weather.windDirection.y);
        writer.WriteFloat(weather.windDirection.z);

        return true;
    });
}

bool SaveManager::LoadGame(const std::string& filepath, ECS::Registry* registry) {
    if (!registry) {
        Core::Logger::Error("SaveManager", "Registry is null in LoadGame.");
        return false;
    }

    // Clean existing SceneNodes and ECS entities
    if (Scene::SceneManager::Get().GetRegistry() == registry) {
        Scene::SceneManager::Get().Reset();
    }
    auto aliveEntities = registry->GetAliveEntities();
    for (auto entity : aliveEntities) {
        registry->DestroyEntity(entity);
    }

    return SaveSystem::ReadSave(filepath, [&](BinaryReader& reader, uint32_t version) {
        if (version < 2) {
            Core::Logger::Info("SaveManager", "Loaded Version 1 save file (skeleton/no-op).");
            return true;
        }

        uint32_t numEntities = 0;
        if (!reader.ReadUint32(numEntities)) {
            Core::Logger::Error("SaveManager", "Failed to read entity count.");
            return false;
        }

        size_t startOffset = reader.GetOffset();

        // Pass 1: Create entities and assign GUIDs
        struct EntityMeta {
            ECS::Entity entity;
            EntityGUID guid;
            uint32_t numComponents;
            std::string name;
            EntityGUID parentGuid;
        };
        std::vector<EntityMeta> entities;
        entities.reserve(numEntities);

        for (uint32_t i = 0; i < numEntities; ++i) {
            EntityGUID guid;
            if (!reader.ReadUint64(guid.high) || !reader.ReadUint64(guid.low)) {
                Core::Logger::Error("SaveManager", "Failed to read entity GUID during Pass 1.");
                return false;
            }

            std::string name = "Entity";
            EntityGUID parentGuid = NULL_GUID;
            if (version >= 3) {
                if (!reader.ReadString(name)) {
                    Core::Logger::Error("SaveManager", "Failed to read entity name.");
                    return false;
                }
                if (!reader.ReadUint64(parentGuid.high) || !reader.ReadUint64(parentGuid.low)) {
                    Core::Logger::Error("SaveManager", "Failed to read parent GUID.");
                    return false;
                }
            }

            ECS::Entity entity = registry->CreateEntity();
            registry->AssignGUID(entity, guid);

            uint32_t numComponents = 0;
            if (!reader.ReadUint32(numComponents)) {
                Core::Logger::Error("SaveManager", "Failed to read component count during Pass 1.");
                return false;
            }

            entities.push_back({entity, guid, numComponents, name, parentGuid});

            for (uint32_t j = 0; j < numComponents; ++j) {
                uint16_t typeId = 0;
                uint32_t payloadSize = 0;
                if (!reader.ReadUint16(typeId) || !reader.ReadUint32(payloadSize)) {
                    Core::Logger::Error("SaveManager", "Failed to read component metadata during Pass 1.");
                    return false;
                }
                if (!reader.Skip(payloadSize)) {
                    Core::Logger::Error("SaveManager", "Failed to skip component payload during Pass 1.");
                    return false;
                }
            }
        }

        // Reconstruct Hierarchy Nodes (so child parenting references resolve in Pass 2)
        if (version >= 3 && Scene::SceneManager::Get().GetRegistry() == registry) {
            for (const auto& meta : entities) {
                auto* node = Scene::SceneManager::Get().CreateNode(meta.name);
                node->SetEntity(meta.entity);
                Scene::SceneManager::Get().RegisterEntityNode(meta.entity, node);
            }

            for (const auto& meta : entities) {
                auto* node = Scene::SceneManager::Get().GetNodeByEntity(meta.entity);
                if (node && !meta.parentGuid.IsNull()) {
                    ECS::Entity parentEnt = registry->GetEntityByGUID(meta.parentGuid);
                    if (parentEnt != ECS::NULL_ENTITY) {
                        auto* parentNode = Scene::SceneManager::Get().GetNodeByEntity(parentEnt);
                        if (parentNode) {
                            auto* root = Scene::SceneManager::Get().GetRootNode();
                            if (root) {
                                auto nodePtr = root->RemoveChild(node);
                                if (nodePtr) {
                                    parentNode->AddChild(std::move(nodePtr));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Seek back to start of entities for Pass 2
        if (!reader.Seek(startOffset)) {
            Core::Logger::Error("SaveManager", "Failed to seek back for Pass 2.");
            return false;
        }

        // Pass 2: Restore components and state
        for (uint32_t i = 0; i < numEntities; ++i) {
            EntityGUID guid;
            reader.ReadUint64(guid.high);
            reader.ReadUint64(guid.low);

            if (version >= 3) {
                std::string dummyName;
                EntityGUID dummyParent;
                reader.ReadString(dummyName);
                reader.ReadUint64(dummyParent.high);
                reader.ReadUint64(dummyParent.low);
            }

            uint32_t numComponents = 0;
            reader.ReadUint32(numComponents);

            ECS::Entity entity = entities[i].entity;

            for (uint32_t j = 0; j < numComponents; ++j) {
                uint16_t typeId = 0;
                uint32_t payloadSize = 0;
                reader.ReadUint16(typeId);
                reader.ReadUint32(payloadSize);

                size_t nextCompOffset = reader.GetOffset() + payloadSize;

                if (typeId == 1) { // ScriptComponent
                    std::string scriptPath;
                    EntityGUID scriptGuid;
                    bool initialized = false;

                    bool success = true;
                    success &= reader.ReadString(scriptPath);
                    success &= reader.ReadUint64(scriptGuid.high);
                    success &= reader.ReadUint64(scriptGuid.low);
                    success &= reader.ReadBool(initialized);

                    if (!success || reader.HasError()) {
                        Core::Logger::Warning("SaveManager", "Corrupted ScriptComponent metadata on entity %d. Skipping component.", entity);
                        reader.ClearError();
                        reader.Seek(nextCompOffset);
                        continue;
                    }

                    if (version >= 4) {
                        std::string resPath = Asset::AssetDatabase::Get().GetAssetPath(scriptPath);
                        if (!resPath.empty()) {
                            scriptPath = resPath;
                        }
                    }

                    try {
                        registry->AddComponent<ECS::ScriptComponent>(entity, scriptPath);
                        if (initialized) {
                            Scripting::ScriptEngine::Get().OnCreateEntity(entity);
                        }

                        if (!Scripting::ScriptEngine::Get().DeserializeScriptState(entity, reader) || reader.HasError()) {
                            Core::Logger::Warning("SaveManager", "Failed to restore script state for entity %d. Skipping component.", entity);
                            if (registry->HasComponent<ECS::ScriptComponent>(entity)) {
                                registry->RemoveComponent<ECS::ScriptComponent>(entity);
                            }
                            reader.ClearError();
                            reader.Seek(nextCompOffset);
                            continue;
                        }
                    } catch (const std::exception& e) {
                        Core::Logger::Warning("SaveManager", "Exception during script restore on entity %d: %s. Skipping component.", entity, e.what());
                        if (registry->HasComponent<ECS::ScriptComponent>(entity)) {
                            registry->RemoveComponent<ECS::ScriptComponent>(entity);
                        }
                        reader.ClearError();
                        reader.Seek(nextCompOffset);
                        continue;
                    }
                } else if (typeId == 2) { // TransformComponent
                    glm::vec3 pos;
                    glm::quat rot;
                    glm::vec3 scale;
                    bool ok = true;
                    ok &= reader.ReadFloat(pos.x);
                    ok &= reader.ReadFloat(pos.y);
                    ok &= reader.ReadFloat(pos.z);
                    ok &= reader.ReadFloat(rot.w);
                    ok &= reader.ReadFloat(rot.x);
                    ok &= reader.ReadFloat(rot.y);
                    ok &= reader.ReadFloat(rot.z);
                    ok &= reader.ReadFloat(scale.x);
                    ok &= reader.ReadFloat(scale.y);
                    ok &= reader.ReadFloat(scale.z);
                    if (ok) {
                        registry->AddComponent<Scene::TransformComponent>(entity, pos, rot, scale);
                        auto* node = (Scene::SceneManager::Get().GetRegistry() == registry) ?
                                     Scene::SceneManager::Get().GetNodeByEntity(entity) : nullptr;
                        if (node) {
                            node->SetLocalPosition(pos);
                            node->SetLocalRotation(rot);
                            node->SetLocalScale(scale);
                        }
                    }
                } else if (typeId == 3) { // CameraComponent
                    int32_t modeVal;
                    float fov, aspect, nearClip, farClip;
                    int32_t priority;
                    bool collision;
                    bool ok = true;
                    ok &= reader.ReadInt32(modeVal);
                    ok &= reader.ReadFloat(fov);
                    ok &= reader.ReadFloat(aspect);
                    ok &= reader.ReadFloat(nearClip);
                    ok &= reader.ReadFloat(farClip);
                    ok &= reader.ReadInt32(priority);
                    ok &= reader.ReadBool(collision);
                    if (ok) {
                        auto& cc = registry->AddComponent<Camera::CameraComponent>(entity);
                        cc.mode = static_cast<Camera::CameraMode>(modeVal);
                        cc.fov = fov;
                        cc.aspect = aspect;
                        cc.nearClip = nearClip;
                        cc.farClip = farClip;
                        cc.priority = priority;
                        cc.collisionEnabled = collision;
                    }
                } else if (typeId == 4) { // LightComponent
                    int32_t typeVal;
                    glm::vec3 color;
                    float intensity, radius, inner, outer;
                    bool ok = true;
                    ok &= reader.ReadInt32(typeVal);
                    ok &= reader.ReadFloat(color.x);
                    ok &= reader.ReadFloat(color.y);
                    ok &= reader.ReadFloat(color.z);
                    ok &= reader.ReadFloat(intensity);
                    ok &= reader.ReadFloat(radius);
                    ok &= reader.ReadFloat(inner);
                    ok &= reader.ReadFloat(outer);
                    if (ok) {
                        auto& lc = registry->AddComponent<Lighting::LightComponent>(entity);
                        lc.type = static_cast<Lighting::LightType>(typeVal);
                        lc.color = color;
                        lc.intensity = intensity;
                        lc.radius = radius;
                        lc.innerCutoff = inner;
                        lc.outerCutoff = outer;
                    }
                } else if (typeId == 5) { // MeshRendererComponent
                    std::string mesh, mat;
                    bool visible, shadow;
                    bool ok = true;
                    ok &= reader.ReadString(mesh);
                    ok &= reader.ReadString(mat);
                    ok &= reader.ReadBool(visible);
                    ok &= reader.ReadBool(shadow);
                    if (ok) {
                        if (version >= 4) {
                            std::string resMesh = Asset::AssetDatabase::Get().GetAssetPath(mesh);
                            if (!resMesh.empty()) mesh = resMesh;
                            std::string resMat = Asset::AssetDatabase::Get().GetAssetPath(mat);
                            if (!resMat.empty()) mat = resMat;
                        }
                        auto& mrc = registry->AddComponent<Renderer::MeshRendererComponent>(entity);
                        mrc.meshPath = mesh;
                        mrc.materialPath = mat;
                        mrc.visible = visible;
                        mrc.castShadows = shadow;
                    }
                } else if (typeId == 6) { // PhysicsComponent
                    int32_t bodyVal, colVal;
                    bool ok = true;
                    ok &= reader.ReadInt32(bodyVal);
                    ok &= reader.ReadInt32(colVal);
                    Physics::Collider collider;
                    collider.type = static_cast<Physics::ColliderType>(colVal);
                    if (collider.type == Physics::ColliderType::AABB) {
                        Physics::AABB aabb;
                        ok &= reader.ReadFloat(aabb.min.x);
                        ok &= reader.ReadFloat(aabb.min.y);
                        ok &= reader.ReadFloat(aabb.min.z);
                        ok &= reader.ReadFloat(aabb.max.x);
                        ok &= reader.ReadFloat(aabb.max.y);
                        ok &= reader.ReadFloat(aabb.max.z);
                        collider.shape = aabb;
                    } else if (collider.type == Physics::ColliderType::Sphere) {
                        Physics::Sphere sphere;
                        ok &= reader.ReadFloat(sphere.center.x);
                        ok &= reader.ReadFloat(sphere.center.y);
                        ok &= reader.ReadFloat(sphere.center.z);
                        ok &= reader.ReadFloat(sphere.radius);
                        collider.shape = sphere;
                    } else if (collider.type == Physics::ColliderType::Capsule) {
                        Physics::Capsule capsule;
                        ok &= reader.ReadFloat(capsule.center.x);
                        ok &= reader.ReadFloat(capsule.halfHeight);
                        ok &= reader.ReadFloat(capsule.radius);
                        collider.shape = capsule;
                    }
                    float mass, restitution, friction;
                    uint32_t layer, mask;
                    ok &= reader.ReadFloat(mass);
                    ok &= reader.ReadFloat(restitution);
                    ok &= reader.ReadFloat(friction);
                    ok &= reader.ReadUint32(layer);
                    ok &= reader.ReadUint32(mask);
                    if (ok) {
                        auto& pc = registry->AddComponent<Physics::PhysicsComponent>(entity);
                        pc.bodyType = static_cast<Physics::BodyType>(bodyVal);
                        pc.collider = collider;
                        pc.SetMass(mass);
                        pc.restitution = restitution;
                        pc.friction = friction;
                        pc.collisionLayer = layer;
                        pc.collisionMask = mask;
                    }
                } else if (typeId == 7) { // PrefabInstanceComponent
                    std::string pGuid;
                    EntityGUID pEntityId;
                    bool isRoot;
                    uint32_t overridesCount;
                    bool ok = true;
                    ok &= reader.ReadString(pGuid);
                    ok &= reader.ReadUint64(pEntityId.high);
                    ok &= reader.ReadUint64(pEntityId.low);
                    ok &= reader.ReadBool(isRoot);
                    ok &= reader.ReadUint32(overridesCount);

                    std::vector<Prefab::PrefabOverride> ovs;
                    for (uint32_t k = 0; k < overridesCount; ++k) {
                        uint16_t cId;
                        std::string fName;
                        std::string fVal;
                        ok &= reader.ReadUint16(cId);
                        ok &= reader.ReadString(fName);
                        ok &= reader.ReadString(fVal);
                        ovs.push_back({cId, fName, fVal});
                    }

                    if (ok) {
                        registry->AddComponent<Prefab::PrefabInstanceComponent>(entity, pGuid, pEntityId, isRoot, ovs);
                    }
                } else if (typeId == 8) { // HealthComponent
                    float currentHealth, maxHealth, shield;
                    bool invulnerable;
                    bool ok = true;
                    ok &= reader.ReadFloat(currentHealth);
                    ok &= reader.ReadFloat(maxHealth);
                    ok &= reader.ReadFloat(shield);
                    ok &= reader.ReadBool(invulnerable);
                    if (ok) {
                        auto& hc = registry->AddComponent<Gameplay::HealthComponent>(entity);
                        hc.currentHealth = currentHealth;
                        hc.maxHealth = maxHealth;
                        hc.shield = shield;
                        hc.invulnerable = invulnerable;
                    }
                } else if (typeId == 9) { // DamageComponent
                    float damageAmount, multiplier, knockbackForce;
                    std::string damageType;
                    bool ok = true;
                    ok &= reader.ReadFloat(damageAmount);
                    ok &= reader.ReadString(damageType);
                    ok &= reader.ReadFloat(multiplier);
                    ok &= reader.ReadFloat(knockbackForce);
                    if (ok) {
                        auto& dc = registry->AddComponent<Gameplay::DamageComponent>(entity);
                        dc.damageAmount = damageAmount;
                        dc.damageType = damageType;
                        dc.multiplier = multiplier;
                        dc.knockbackForce = knockbackForce;
                    }
                } else if (typeId == 10) { // TeamComponent
                    int32_t teamId;
                    bool friendlyFire;
                    bool ok = true;
                    ok &= reader.ReadInt32(teamId);
                    ok &= reader.ReadBool(friendlyFire);
                    if (ok) {
                        auto& tc = registry->AddComponent<Gameplay::TeamComponent>(entity);
                        tc.teamId = teamId;
                        tc.friendlyFire = friendlyFire;
                    }
                } else if (typeId == 11) { // InteractionComponent
                    std::string prompt;
                    float distance;
                    bool isInteractable;
                    bool ok = true;
                    ok &= reader.ReadString(prompt);
                    ok &= reader.ReadFloat(distance);
                    ok &= reader.ReadBool(isInteractable);
                    if (ok) {
                        auto& ic = registry->AddComponent<Gameplay::InteractionComponent>(entity);
                        ic.prompt = prompt;
                        ic.distance = distance;
                        ic.isInteractable = isInteractable;
                    }
                } else if (typeId == 12) { // GameplayTagsComponent
                    uint32_t tagCount;
                    bool ok = true;
                    ok &= reader.ReadUint32(tagCount);
                    std::vector<std::string> rawTags;
                    for (uint32_t k = 0; k < tagCount; ++k) {
                        std::string tagStr;
                        ok &= reader.ReadString(tagStr);
                        rawTags.push_back(tagStr);
                    }
                    if (ok) {
                        auto& gtc = registry->AddComponent<Gameplay::GameplayTagsComponent>(entity);
                        for (const auto& tagStr : rawTags) {
                            gtc.AddTag(tagStr);
                        }
                    }
                } else if (typeId == 13) { // SpawnPointComponent
                    std::string spawnGroup;
                    bool isEnabled;
                    bool ok = true;
                    ok &= reader.ReadString(spawnGroup);
                    ok &= reader.ReadBool(isEnabled);
                    if (ok) {
                        auto& sp = registry->AddComponent<Gameplay::SpawnPointComponent>(entity);
                        sp.spawnGroup = spawnGroup;
                        sp.isEnabled = isEnabled;
                    }
                } else if (typeId == 14) { // PlayerControllerComponent
                    uint32_t peerId;
                    bool isLocal;
                    EntityGUID pawnGuid;
                    EntityGUID stateGuid;
                    bool ok = true;
                    ok &= reader.ReadUint32(peerId);
                    ok &= reader.ReadBool(isLocal);
                    ok &= reader.ReadUint64(pawnGuid.high);
                    ok &= reader.ReadUint64(pawnGuid.low);
                    ok &= reader.ReadUint64(stateGuid.high);
                    ok &= reader.ReadUint64(stateGuid.low);
                    if (ok) {
                        auto& pcc = registry->AddComponent<Gameplay::PlayerControllerComponent>(entity);
                        pcc.peerId = peerId;
                        pcc.isLocal = isLocal;
                        pcc.possessedPawn = registry->GetEntityByGUID(pawnGuid);
                        pcc.playerStateEntity = registry->GetEntityByGUID(stateGuid);
                    }
                } else if (typeId == 15) { // PlayerStateComponent
                    std::string playerName;
                    uint32_t peerId;
                    float score;
                    int32_t teamId;
                    float ping;
                    bool ok = true;
                    ok &= reader.ReadString(playerName);
                    ok &= reader.ReadUint32(peerId);
                    ok &= reader.ReadFloat(score);
                    ok &= reader.ReadInt32(teamId);
                    ok &= reader.ReadFloat(ping);
                    if (ok) {
                        auto& psc = registry->AddComponent<Gameplay::PlayerStateComponent>(entity);
                        psc.playerName = playerName;
                        psc.peerId = peerId;
                        psc.score = score;
                        psc.teamId = teamId;
                        psc.ping = ping;
                    }
                } else if (typeId == 16) { // GameStateComponent
                    float elapsedTime;
                    bool isMatchRunning;
                    bool isMatchOver;
                    int32_t winnerTeamId;
                    bool ok = true;
                    ok &= reader.ReadFloat(elapsedTime);
                    ok &= reader.ReadBool(isMatchRunning);
                    ok &= reader.ReadBool(isMatchOver);
                    ok &= reader.ReadInt32(winnerTeamId);
                    if (ok) {
                        auto& gsc = registry->AddComponent<Gameplay::GameStateComponent>(entity);
                        gsc.elapsedTime = elapsedTime;
                        gsc.isMatchRunning = isMatchRunning;
                        gsc.isMatchOver = isMatchOver;
                        gsc.winnerTeamId = winnerTeamId;
                    }
                } else if (typeId == 17) { // InventoryComponent
                    uint32_t maxSlots;
                    uint32_t numSlots;
                    bool ok = true;
                    ok &= reader.ReadUint32(maxSlots);
                    ok &= reader.ReadUint32(numSlots);
                    std::vector<Gameplay::InventorySlot> tempSlots(numSlots);
                    for (uint32_t k = 0; k < numSlots; ++k) {
                        ok &= reader.ReadString(tempSlots[k].itemId);
                        ok &= reader.ReadUint32(tempSlots[k].quantity);
                    }
                    if (ok) {
                        auto& ic = registry->AddComponent<Gameplay::InventoryComponent>(entity, maxSlots);
                        ic.slots = tempSlots;
                    }
                } else if (typeId == 18) { // ItemComponent
                    std::string itemId;
                    uint32_t quantity;
                    bool ok = true;
                    ok &= reader.ReadString(itemId);
                    ok &= reader.ReadUint32(quantity);
                    if (ok) {
                        auto& ic = registry->AddComponent<Gameplay::ItemComponent>(entity);
                        ic.itemId = itemId;
                        ic.quantity = quantity;
                    }
                } else if (typeId == 19) { // EquipmentComponent
                    uint32_t numSlots;
                    bool ok = true;
                    ok &= reader.ReadUint32(numSlots);
                    std::vector<std::string> tempSlots(numSlots);
                    for (uint32_t k = 0; k < numSlots; ++k) {
                        ok &= reader.ReadString(tempSlots[k]);
                    }
                    if (ok) {
                        auto& ec = registry->AddComponent<Gameplay::EquipmentComponent>(entity);
                        for (size_t k = 0; k < (std::min)(static_cast<size_t>(numSlots), ec.slots.size()); ++k) {
                            ec.slots[k] = tempSlots[k];
                        }
                    }
                } else if (typeId == 20) { // QuestComponent
                    uint32_t completedCount;
                    bool ok = true;
                    ok &= reader.ReadUint32(completedCount);
                    std::vector<std::string> compQuests(completedCount);
                    for (uint32_t k = 0; k < completedCount; ++k) {
                        ok &= reader.ReadString(compQuests[k]);
                    }
                    uint32_t activeCount;
                    ok &= reader.ReadUint32(activeCount);
                    std::unordered_map<std::string, Gameplay::QuestState> actQuests;
                    for (uint32_t k = 0; k < activeCount; ++k) {
                        Gameplay::QuestState state;
                        ok &= reader.ReadString(state.questId);
                        ok &= reader.ReadInt32(state.currentStageIndex);
                        ok &= reader.ReadBool(state.isCompleted);
                        uint32_t progressCount;
                        ok &= reader.ReadUint32(progressCount);
                        for (uint32_t p = 0; p < progressCount; ++p) {
                            std::string objId;
                            int32_t count;
                            ok &= reader.ReadString(objId);
                            ok &= reader.ReadInt32(count);
                            state.objectiveProgress[objId] = count;
                        }
                        actQuests[state.questId] = state;
                    }
                    if (ok) {
                        auto& qc = registry->AddComponent<Gameplay::QuestComponent>(entity);
                        qc.completedQuests = compQuests;
                        qc.activeQuests = actQuests;
                    }
                } else if (typeId == 21) { // DialogueComponent
                    std::string currentDialogueId;
                    std::string currentNodeId;
                    bool isInDialogue;
                    bool ok = true;
                    ok &= reader.ReadString(currentDialogueId);
                    ok &= reader.ReadString(currentNodeId);
                    ok &= reader.ReadBool(isInDialogue);
                    if (ok) {
                        auto& dc = registry->AddComponent<Gameplay::DialogueComponent>(entity);
                        dc.currentDialogueId = currentDialogueId;
                        dc.currentNodeId = currentNodeId;
                        dc.isInDialogue = isInDialogue;
                    }
                } else if (typeId == 22) { // InteractableComponent
                    std::string prompt;
                    float distance;
                    bool isInteractable;
                    std::string interactionType;
                    std::string targetData;
                    std::string onInteractLua;
                    bool ok = true;
                    ok &= reader.ReadString(prompt);
                    ok &= reader.ReadFloat(distance);
                    ok &= reader.ReadBool(isInteractable);
                    ok &= reader.ReadString(interactionType);
                    ok &= reader.ReadString(targetData);
                    ok &= reader.ReadString(onInteractLua);
                    if (ok) {
                        auto& ic = registry->AddComponent<Gameplay::InteractableComponent>(entity);
                        ic.prompt = prompt;
                        ic.distance = distance;
                        ic.isInteractable = isInteractable;
                        ic.interactionType = interactionType;
                        ic.targetData = targetData;
                        ic.onInteractLua = onInteractLua;
                    }
                } else if (typeId == 23) { // TriggerVolumeComponent
                    uint8_t typeVal;
                    std::string onEnterLua;
                    std::string onExitLua;
                    bool ok = true;
                    ok &= reader.ReadUint8(typeVal);
                    ok &= reader.ReadString(onEnterLua);
                    ok &= reader.ReadString(onExitLua);
                    if (ok) {
                        auto& tvc = registry->AddComponent<Gameplay::TriggerVolumeComponent>(entity);
                        tvc.type = static_cast<Physics::ColliderType>(typeVal);
                        tvc.onEnterLua = onEnterLua;
                        tvc.onExitLua = onExitLua;
                    }
                } else if (typeId == 24) { // AudioSourceComponent
                    std::string eventOrPath;
                    bool isPlaying, loop, spatial3D;
                    float pitch, volume, pan, minDistance, maxDistance;
                    bool ok = true;
                    ok &= reader.ReadString(eventOrPath);
                    ok &= reader.ReadBool(isPlaying);
                    ok &= reader.ReadBool(loop);
                    ok &= reader.ReadBool(spatial3D);
                    ok &= reader.ReadFloat(pitch);
                    ok &= reader.ReadFloat(volume);
                    ok &= reader.ReadFloat(pan);
                    ok &= reader.ReadFloat(minDistance);
                    ok &= reader.ReadFloat(maxDistance);
                    if (ok) {
                        auto& asc = registry->AddComponent<Audio::AudioSourceComponent>(entity);
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
                } else if (typeId == 25) { // AudioListenerComponent
                    glm::vec3 position, forward, up;
                    bool ok = true;
                    ok &= reader.ReadFloat(position.x);
                    ok &= reader.ReadFloat(position.y);
                    ok &= reader.ReadFloat(position.z);
                    ok &= reader.ReadFloat(forward.x);
                    ok &= reader.ReadFloat(forward.y);
                    ok &= reader.ReadFloat(forward.z);
                    ok &= reader.ReadFloat(up.x);
                    ok &= reader.ReadFloat(up.y);
                    ok &= reader.ReadFloat(up.z);
                    if (ok) {
                        auto& alc = registry->AddComponent<Audio::AudioListenerComponent>(entity);
                        alc.position = position;
                        alc.forward = forward;
                        alc.up = up;
                    }
                } else if (typeId == 26) { // CinematicPlayerComponent
                    std::string timelineName;
                    bool isPlaying, isPaused, loop;
                    float currentTime, playbackSpeed;
                    bool ok = true;
                    ok &= reader.ReadString(timelineName);
                    ok &= reader.ReadBool(isPlaying);
                    ok &= reader.ReadBool(isPaused);
                    ok &= reader.ReadFloat(currentTime);
                    ok &= reader.ReadBool(loop);
                    ok &= reader.ReadFloat(playbackSpeed);
                    if (ok) {
                        auto& cpc = registry->AddComponent<Timeline::CinematicPlayerComponent>(entity);
                        cpc.timelineName = timelineName;
                        cpc.isPlaying = isPlaying;
                        cpc.isPaused = isPaused;
                        cpc.currentTime = currentTime;
                        cpc.loop = loop;
                        cpc.playbackSpeed = playbackSpeed;
                    }
                } else {
                    reader.Skip(payloadSize);
                }

                if (reader.GetOffset() != nextCompOffset) {
                    reader.Seek(nextCompOffset);
                }
            }
        }

        if (version >= 5) {
            auto& tm = Terrain::TerrainManager::Get();
            uint32_t seed;
            float chunkSize;
            if (!reader.ReadUint32(seed) || !reader.ReadFloat(chunkSize)) return false;
            tm.Initialize(seed, chunkSize);

            // Height edits
            uint32_t hCount;
            if (!reader.ReadUint32(hCount)) return false;
            std::unordered_map<uint64_t, float> heightEdits;
            for (uint32_t i = 0; i < hCount; ++i) {
                uint64_t key;
                float h;
                if (!reader.ReadUint64(key) || !reader.ReadFloat(h)) return false;
                heightEdits[key] = h;
            }
            tm.SetHeightEdits(heightEdits);

            // Layer edits
            uint32_t lCount;
            if (!reader.ReadUint32(lCount)) return false;
            std::unordered_map<uint64_t, glm::vec4> layerEdits;
            for (uint32_t i = 0; i < lCount; ++i) {
                uint64_t key;
                float rx, ry, rz, rw;
                if (!reader.ReadUint64(key) || !reader.ReadFloat(rx) || !reader.ReadFloat(ry) || !reader.ReadFloat(rz) || !reader.ReadFloat(rw)) return false;
                layerEdits[key] = glm::vec4(rx, ry, rz, rw);
            }
            tm.SetLayerEdits(layerEdits);

            // Vegetation edits
            uint32_t vCount;
            if (!reader.ReadUint32(vCount)) return false;
            std::unordered_set<Terrain::ChunkCoord, Terrain::ChunkCoordHash> editedVegChunks;
            std::unordered_map<Terrain::ChunkCoord, std::vector<Terrain::VegetationInstance>, Terrain::ChunkCoordHash> paintedVeg;
            for (uint32_t i = 0; i < vCount; ++i) {
                Terrain::ChunkCoord coord;
                if (!reader.ReadInt32(coord.x) || !reader.ReadInt32(coord.z)) return false;
                editedVegChunks.insert(coord);
                uint32_t instCount;
                if (!reader.ReadUint32(instCount)) return false;
                std::vector<Terrain::VegetationInstance> insts;
                insts.reserve(instCount);
                for (uint32_t j = 0; j < instCount; ++j) {
                    int type;
                    float px, py, pz, scale, rot;
                    if (!reader.ReadInt32(type) || !reader.ReadFloat(px) || !reader.ReadFloat(py) || !reader.ReadFloat(pz) || !reader.ReadFloat(scale) || !reader.ReadFloat(rot)) return false;
                    Terrain::VegetationInstance inst;
                    inst.type = type;
                    inst.position = glm::vec3(px, py, pz);
                    inst.scale = scale;
                    inst.rotation = rot;
                    insts.push_back(inst);
                }
                paintedVeg[coord] = insts;
            }
            tm.SetEditedVegetationChunks(editedVegChunks);
            tm.SetPaintedVegetation(paintedVeg);

            // Roads
            uint32_t roadCount;
            if (!reader.ReadUint32(roadCount)) return false;
            std::vector<Terrain::TerrainManager::RoadData> roads;
            for (uint32_t i = 0; i < roadCount; ++i) {
                float width;
                int rType;
                uint32_t ptCount;
                if (!reader.ReadFloat(width) || !reader.ReadInt32(rType) || !reader.ReadUint32(ptCount)) return false;
                std::vector<glm::vec3> pts(ptCount);
                for (uint32_t j = 0; j < ptCount; ++j) {
                    if (!reader.ReadFloat(pts[j].x) || !reader.ReadFloat(pts[j].y) || !reader.ReadFloat(pts[j].z)) return false;
                }
                Terrain::TerrainManager::RoadData road;
                road.width = width;
                road.roadType = rType;
                road.splinePoints = pts;
                roads.push_back(road);
            }
            tm.SetRoads(roads);

            // Rivers
            uint32_t riverCount;
            if (!reader.ReadUint32(riverCount)) return false;
            std::vector<Terrain::TerrainManager::RiverData> rivers;
            for (uint32_t i = 0; i < riverCount; ++i) {
                float width, depth;
                uint32_t ptCount;
                if (!reader.ReadFloat(width) || !reader.ReadFloat(depth) || !reader.ReadUint32(ptCount)) return false;
                std::vector<glm::vec3> pts(ptCount);
                for (uint32_t j = 0; j < ptCount; ++j) {
                    if (!reader.ReadFloat(pts[j].x) || !reader.ReadFloat(pts[j].y) || !reader.ReadFloat(pts[j].z)) return false;
                }
                Terrain::TerrainManager::RiverData river;
                river.width = width;
                river.depth = depth;
                river.splinePoints = pts;
                rivers.push_back(river);
            }
            tm.SetRivers(rivers);

            // Environment data
            auto& em = Environment::EnvironmentManager::Get();
            float sx, sy, sz, turb, exp;
            if (!reader.ReadFloat(sx) || !reader.ReadFloat(sy) || !reader.ReadFloat(sz) ||
                !reader.ReadFloat(turb) || !reader.ReadFloat(exp)) return false;
            Environment::SkySettings sky;
            sky.skyColor = glm::vec3(sx, sy, sz);
            sky.turbidity = turb;
            sky.exposure = exp;
            em.SetSkySettings(sky);

            float dx, dy, dz;
            if (!reader.ReadFloat(dx) || !reader.ReadFloat(dy) || !reader.ReadFloat(dz)) return false;
            em.SetSunDirection(glm::vec3(dx, dy, dz));

            float ax, ay, az, aIntensity;
            if (!reader.ReadFloat(ax) || !reader.ReadFloat(ay) || !reader.ReadFloat(az) || !reader.ReadFloat(aIntensity)) return false;
            em.SetAmbientLighting(glm::vec3(ax, ay, az), aIntensity);

            bool fogEnabled;
            float fx, fy, fz, fDensity, fStart, fEnd;
            if (!reader.ReadBool(fogEnabled) || !reader.ReadFloat(fx) || !reader.ReadFloat(fy) || !reader.ReadFloat(fz) ||
                !reader.ReadFloat(fDensity) || !reader.ReadFloat(fStart) || !reader.ReadFloat(fEnd)) return false;
            Environment::FogSettings fog;
            fog.enabled = fogEnabled;
            fog.color = glm::vec3(fx, fy, fz);
            fog.density = fDensity;
            fog.start = fStart;
            fog.end = fEnd;
            em.SetFogSettings(fog);

            float rain, wSpeed, wdx, wdy, wdz;
            if (!reader.ReadFloat(rain) || !reader.ReadFloat(wSpeed) ||
                !reader.ReadFloat(wdx) || !reader.ReadFloat(wdy) || !reader.ReadFloat(wdz)) return false;
            Environment::WeatherSettings weather;
            weather.rainIntensity = rain;
            weather.windSpeed = wSpeed;
            weather.windDirection = glm::vec3(wdx, wdy, wdz);
            em.SetWeatherSettings(weather);

            // Rebuild active chunks
            tm.RegenerateActiveChunks();
        }

        return true;
    });
}

} // namespace KumariEngine::Save

