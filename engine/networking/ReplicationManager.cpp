#include "networking/ReplicationManager.hpp"
#include "networking/NetworkManager.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include "networking/ClientPrediction.hpp"
#include "networking/TimeSync.hpp"
#include "physics/character_controller.hpp"
#include "physics/physics_world.hpp"
#include "networking/NetworkOwnershipComponent.hpp"
#include <chrono>
#include <algorithm>

namespace KumariEngine::Networking {

struct ScopedTimer {
    std::chrono::high_resolution_clock::time_point start;
    std::atomic<double>& target;
    ScopedTimer(std::atomic<double>& targetMs) : start(std::chrono::high_resolution_clock::now()), target(targetMs) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        target.store(std::chrono::duration<double, std::milli>(end - start).count());
    }
};

static uint32_t CompressQuat(const glm::quat& q) {
    glm::quat nq = glm::normalize(q);
    int maxIndex = 0;
    float maxValue = std::abs(nq.x);
    if (std::abs(nq.y) > maxValue) { maxIndex = 1; maxValue = std::abs(nq.y); }
    if (std::abs(nq.z) > maxValue) { maxIndex = 2; maxValue = std::abs(nq.z); }
    if (std::abs(nq.w) > maxValue) { maxIndex = 3; maxValue = std::abs(nq.w); }

    float sign = (nq[maxIndex] < 0.0f) ? -1.0f : 1.0f;
    glm::quat sq = nq * sign;

    float a = 0.0f, b = 0.0f, c = 0.0f;
    if (maxIndex == 0) { a = sq.y; b = sq.z; c = sq.w; }
    else if (maxIndex == 1) { a = sq.x; b = sq.z; c = sq.w; }
    else if (maxIndex == 2) { a = sq.x; b = sq.y; c = sq.w; }
    else if (maxIndex == 3) { a = sq.x; b = sq.y; c = sq.z; }

    auto quantize = [](float val) -> uint32_t {
        constexpr float RANGE = 0.70710678f;
        float clamped = std::clamp(val, -RANGE, RANGE);
        float normalized = (clamped + RANGE) / (2.0f * RANGE);
        return std::clamp(static_cast<uint32_t>(normalized * 1022.0f + 0.5f), 0u, 1022u);
    };

    uint32_t qa = quantize(a);
    uint32_t qb = quantize(b);
    uint32_t qc = quantize(c);

    return (static_cast<uint32_t>(maxIndex) << 30) | (qa << 20) | (qb << 10) | qc;
}

static glm::quat DecompressQuat(uint32_t compressed) {
    int maxIndex = (compressed >> 30) & 3;
    uint32_t qa = (compressed >> 20) & 1023;
    uint32_t qb = (compressed >> 10) & 1023;
    uint32_t qc = compressed & 1023;

    auto dequantize = [](uint32_t val) -> float {
        constexpr float RANGE = 0.70710678f;
        float normalized = static_cast<float>(val) / 1022.0f;
        return normalized * (2.0f * RANGE) - RANGE;
    };

    float a = dequantize(qa);
    float b = dequantize(qb);
    float c = dequantize(qc);

    float sumSq = a*a + b*b + c*c;
    float largest = std::sqrt(std::max(0.0f, 1.0f - sumSq));

    glm::quat q;
    if (maxIndex == 0) {
        q.x = largest; q.y = a; q.z = b; q.w = c;
    } else if (maxIndex == 1) {
        q.x = a; q.y = largest; q.z = b; q.w = c;
    } else if (maxIndex == 2) {
        q.x = a; q.y = b; q.z = largest; q.w = c;
    } else if (maxIndex == 3) {
        q.x = a; q.y = b; q.z = c; q.w = largest;
    }
    return q;
}

static bool AreTransformsEqual(const Scene::TransformComponent& a, const Scene::TransformComponent& b) {
    return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
}

static bool ArePhysicsEqual(const Physics::PhysicsComponent& a, const Physics::PhysicsComponent& b) {
    if (a.bodyType != b.bodyType) return false;
    if (a.mass != b.mass) return false;
    if (a.inverseMass != b.inverseMass) return false;
    if (a.velocity != b.velocity) return false;
    if (a.angularVelocity != b.angularVelocity) return false;
    if (a.forceAccum != b.forceAccum) return false;
    if (a.torqueAccum != b.torqueAccum) return false;
    if (a.friction != b.friction) return false;
    if (a.restitution != b.restitution) return false;
    if (a.collisionLayer != b.collisionLayer) return false;
    if (a.collisionMask != b.collisionMask) return false;
    if (a.collider.type != b.collider.type) return false;
    if (a.collider.isTrigger != b.collider.isTrigger) return false;

    if (a.collider.type == Physics::ColliderType::AABB) {
        auto sA = std::get<Physics::AABB>(a.collider.shape);
        auto sB = std::get<Physics::AABB>(b.collider.shape);
        if (sA.min != sB.min || sA.max != sB.max) return false;
    } else if (a.collider.type == Physics::ColliderType::Sphere) {
        auto sA = std::get<Physics::Sphere>(a.collider.shape);
        auto sB = std::get<Physics::Sphere>(b.collider.shape);
        if (sA.center != sB.center || sA.radius != sB.radius) return false;
    } else if (a.collider.type == Physics::ColliderType::Capsule) {
        auto sA = std::get<Physics::Capsule>(a.collider.shape);
        auto sB = std::get<Physics::Capsule>(b.collider.shape);
        if (sA.center != sB.center || sA.halfHeight != sB.halfHeight || sA.radius != sB.radius) return false;
    }
    return true;
}

static bool AreCCsEqual(const Physics::CharacterControllerComponent& a, const Physics::CharacterControllerComponent& b) {
    return a.state == b.state &&
           a.walkSpeed == b.walkSpeed &&
           a.runSpeed == b.runSpeed &&
           a.sprintSpeed == b.sprintSpeed &&
           a.jumpForce == b.jumpForce &&
           a.gravityMultiplier == b.gravityMultiplier &&
           a.moveDirection == b.moveDirection &&
           a.requestJump == b.requestJump &&
           a.isGrounded == b.isGrounded &&
           a.verticalVelocity == b.verticalVelocity &&
           a.slopeLimit == b.slopeLimit &&
           a.stepHeight == b.stepHeight;
}

static bool AreHealthsEqual(const Gameplay::HealthComponent& a, const Gameplay::HealthComponent& b) {
    return a.currentHealth == b.currentHealth && a.maxHealth == b.maxHealth && a.shield == b.shield && a.invulnerable == b.invulnerable;
}

static bool AreTeamsEqual(const Gameplay::TeamComponent& a, const Gameplay::TeamComponent& b) {
    return a.teamId == b.teamId && a.friendlyFire == b.friendlyFire;
}

static bool ArePlayerStatesEqual(const Gameplay::PlayerStateComponent& a, const Gameplay::PlayerStateComponent& b) {
    return a.playerName == b.playerName && a.peerId == b.peerId && a.score == b.score && a.teamId == b.teamId && a.ping == b.ping;
}

static bool AreGameplayTagsEqual(const Gameplay::GameplayTagsComponent& a, const Gameplay::GameplayTagsComponent& b) {
    return a.GetRawTags() == b.GetRawTags();
}

static bool AreInventoriesEqual(const Gameplay::InventoryComponent& a, const Gameplay::InventoryComponent& b) {
    if (a.maxSlots != b.maxSlots || a.slots.size() != b.slots.size()) return false;
    for (size_t i = 0; i < a.slots.size(); ++i) {
        if (a.slots[i].itemId != b.slots[i].itemId || a.slots[i].quantity != b.slots[i].quantity) return false;
    }
    return true;
}

static bool AreItemsEqual(const Gameplay::ItemComponent& a, const Gameplay::ItemComponent& b) {
    return a.itemId == b.itemId && a.quantity == b.quantity;
}

static bool AreEquipmentsEqual(const Gameplay::EquipmentComponent& a, const Gameplay::EquipmentComponent& b) {
    return a.slots == b.slots;
}

static bool AreQuestsEqual(const Gameplay::QuestComponent& a, const Gameplay::QuestComponent& b) {
    if (a.completedQuests != b.completedQuests) return false;
    if (a.activeQuests.size() != b.activeQuests.size()) return false;
    for (const auto& [questId, stateA] : a.activeQuests) {
        auto it = b.activeQuests.find(questId);
        if (it == b.activeQuests.end()) return false;
        const auto& stateB = it->second;
        if (stateA.currentStageIndex != stateB.currentStageIndex || stateA.isCompleted != stateB.isCompleted) return false;
        if (stateA.objectiveProgress != stateB.objectiveProgress) return false;
    }
    return true;
}

static bool AreDialoguesEqual(const Gameplay::DialogueComponent& a, const Gameplay::DialogueComponent& b) {
    return a.currentDialogueId == b.currentDialogueId && a.currentNodeId == b.currentNodeId && a.isInDialogue == b.isInDialogue;
}

static bool AreInteractablesEqual(const Gameplay::InteractableComponent& a, const Gameplay::InteractableComponent& b) {
    return a.prompt == b.prompt && a.distance == b.distance && a.isInteractable == b.isInteractable && 
           a.interactionType == b.interactionType && a.targetData == b.targetData && a.onInteractLua == b.onInteractLua;
}

static bool AreTriggerVolumesEqual(const Gameplay::TriggerVolumeComponent& a, const Gameplay::TriggerVolumeComponent& b) {
    return a.type == b.type && a.onEnterLua == b.onEnterLua && a.onExitLua == b.onExitLua;
}

void ReplicationManager::Initialize(ECS::Registry* registry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = registry;
    
    if (m_registry) {
        m_registry->RegisterComponent<Scene::TransformComponent>();
        m_registry->RegisterComponent<Physics::PhysicsComponent>();
        m_registry->RegisterComponent<Physics::CharacterControllerComponent>();
        m_registry->RegisterComponent<ECS::ScriptComponent>();
        m_registry->RegisterComponent<Gameplay::HealthComponent>();
        m_registry->RegisterComponent<Gameplay::TeamComponent>();
        m_registry->RegisterComponent<Gameplay::PlayerStateComponent>();
        m_registry->RegisterComponent<Gameplay::GameplayTagsComponent>();
        m_registry->RegisterComponent<Gameplay::InventoryComponent>();
        m_registry->RegisterComponent<Gameplay::ItemComponent>();
        m_registry->RegisterComponent<Gameplay::EquipmentComponent>();
        m_registry->RegisterComponent<Gameplay::QuestComponent>();
        m_registry->RegisterComponent<Gameplay::DialogueComponent>();
        m_registry->RegisterComponent<Gameplay::InteractableComponent>();
        m_registry->RegisterComponent<Gameplay::TriggerVolumeComponent>();
    }

    m_snapshotHistory.clear();
    m_clientStates.clear();
    m_clientReplicatedGUIDs.clear();
    m_pendingParents.clear();
    m_nextSnapshotId = 1;

    Core::Logger::Info("Replication", "ReplicationManager initialized successfully.");
}

void ReplicationManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotHistory.clear();
    m_clientStates.clear();
    m_clientReplicatedGUIDs.clear();
    m_pendingParents.clear();
    m_registry = nullptr;
    Core::Logger::Info("Replication", "ReplicationManager shut down.");
}

Snapshot ReplicationManager::CaptureCurrentSnapshot() {
    Snapshot snapshot;
    snapshot.id = m_nextSnapshotId;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    snapshot.timestamp = std::chrono::duration<double, std::milli>(now).count();

    if (!m_registry) return snapshot;

    auto aliveEntities = m_registry->GetAliveEntities();
    for (auto entity : aliveEntities) {
        Save::EntityGUID guid = m_registry->GetGUID(entity);
        if (guid.IsNull()) {
            guid = m_registry->CreateGUID(entity);
        }

        EntityState entityState;
        entityState.guid = guid;
        entityState.isAlive = true;

        // Get parent GUID from scene node hierarchy
        entityState.parentGuid = Save::NULL_GUID;
        if (Scene::SceneManager::Get().GetRegistry() == m_registry) {
            auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
            if (node && node->GetParent()) {
                auto parentEnt = node->GetParent()->GetEntity();
                if (parentEnt != ECS::NULL_ENTITY) {
                    entityState.parentGuid = m_registry->GetGUID(parentEnt);
                }
            }
        }

        // Copy component states if they exist
        if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
            entityState.components.hasTransform = true;
            entityState.components.transform = m_registry->GetComponent<Scene::TransformComponent>(entity);
        }
        if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) {
            entityState.components.hasPhysics = true;
            entityState.components.physics = m_registry->GetComponent<Physics::PhysicsComponent>(entity);
        }
        if (m_registry->HasComponent<Physics::CharacterControllerComponent>(entity)) {
            entityState.components.hasCharacterController = true;
            entityState.components.characterController = m_registry->GetComponent<Physics::CharacterControllerComponent>(entity);
        }
        if (m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
            entityState.components.hasScript = true;
            entityState.components.scriptPath = m_registry->GetComponent<ECS::ScriptComponent>(entity).scriptPath;
        }
        if (m_registry->HasComponent<Gameplay::HealthComponent>(entity)) {
            entityState.components.hasHealth = true;
            entityState.components.health = m_registry->GetComponent<Gameplay::HealthComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::TeamComponent>(entity)) {
            entityState.components.hasTeam = true;
            entityState.components.team = m_registry->GetComponent<Gameplay::TeamComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::PlayerStateComponent>(entity)) {
            entityState.components.hasPlayerState = true;
            entityState.components.playerState = m_registry->GetComponent<Gameplay::PlayerStateComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
            entityState.components.hasGameplayTags = true;
            entityState.components.gameplayTags = m_registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
            entityState.components.hasInventory = true;
            entityState.components.inventory = m_registry->GetComponent<Gameplay::InventoryComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::ItemComponent>(entity)) {
            entityState.components.hasItem = true;
            entityState.components.item = m_registry->GetComponent<Gameplay::ItemComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::EquipmentComponent>(entity)) {
            entityState.components.hasEquipment = true;
            entityState.components.equipment = m_registry->GetComponent<Gameplay::EquipmentComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::QuestComponent>(entity)) {
            entityState.components.hasQuest = true;
            entityState.components.quest = m_registry->GetComponent<Gameplay::QuestComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::DialogueComponent>(entity)) {
            entityState.components.hasDialogue = true;
            entityState.components.dialogue = m_registry->GetComponent<Gameplay::DialogueComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::InteractableComponent>(entity)) {
            entityState.components.hasInteractable = true;
            entityState.components.interactable = m_registry->GetComponent<Gameplay::InteractableComponent>(entity);
        }
        if (m_registry->HasComponent<Gameplay::TriggerVolumeComponent>(entity)) {
            entityState.components.hasTriggerVolume = true;
            entityState.components.triggerVolume = m_registry->GetComponent<Gameplay::TriggerVolumeComponent>(entity);
        }

        // Only replicate if it has at least one of the replicated components
        if (entityState.components.hasTransform || entityState.components.hasPhysics || 
            entityState.components.hasCharacterController || entityState.components.hasScript ||
            entityState.components.hasHealth || entityState.components.hasTeam ||
            entityState.components.hasPlayerState || entityState.components.hasGameplayTags ||
            entityState.components.hasInventory || entityState.components.hasItem ||
            entityState.components.hasEquipment || entityState.components.hasQuest ||
            entityState.components.hasDialogue || entityState.components.hasInteractable ||
            entityState.components.hasTriggerVolume) {
            snapshot.entities[guid] = entityState;
        }
    }

    return snapshot;
}

void ReplicationManager::ServerUpdate() {
    ScopedTimer timer(m_lastReplicationTimeMs);
    m_lastSerializationTimeMs.store(0.0);

    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_registry) return;

    // 1. Capture current frame state
    Snapshot current = CaptureCurrentSnapshot();
    m_snapshotHistory[m_nextSnapshotId] = current;

    // Prune history to limit memory
    while (m_snapshotHistory.size() > m_maxHistorySize) {
        m_snapshotHistory.erase(m_snapshotHistory.begin());
    }

    // 2. Dispatch snapshot to each connected client
    for (auto& [peerId, clientState] : m_clientStates) {
        PacketWriter writer(PacketId::ReplicationSnapshot);
        
        // Write header: snapshot ID
        writer.WriteUint32(m_nextSnapshotId);

        // Determine if delta replication is possible
        bool isDelta = clientState.hasReceivedInitialSnapshot && 
                      (m_snapshotHistory.find(clientState.lastAckedSnapshotId) != m_snapshotHistory.end());

        writer.WriteBool(isDelta);
        writer.WriteUint32(isDelta ? clientState.lastAckedSnapshotId : 0);

        // Calculate Interest Set for this peer
        std::unordered_set<Save::EntityGUID> currentInterestSet;
        
        ECS::Entity playerEnt = ECS::NULL_ENTITY;
        m_registry->Each<NetworkOwnershipComponent>([&](auto pEntity, const NetworkOwnershipComponent& own) {
            if (own.ownerPeerId == peerId) {
                playerEnt = pEntity;
            }
        });

        for (const auto& [guid, entState] : current.entities) {
            ECS::Entity ent = m_registry->GetEntityByGUID(guid);
            if (ent != ECS::NULL_ENTITY) {
                bool isVisible = true;
                if (playerEnt == ent) {
                    isVisible = true; // Always visible to self
                } else if (m_visibilityCallback) {
                    isVisible = m_visibilityCallback(peerId, ent);
                } else {
                    // Default range check
                    if (playerEnt != ECS::NULL_ENTITY && m_registry->HasComponent<Scene::TransformComponent>(playerEnt) && m_registry->HasComponent<Scene::TransformComponent>(ent)) {
                        const auto& pTrans = m_registry->GetComponent<Scene::TransformComponent>(playerEnt);
                        const auto& tTrans = m_registry->GetComponent<Scene::TransformComponent>(ent);
                        if (glm::distance(pTrans.position, tTrans.position) > m_interestRange) {
                            isVisible = false;
                        }
                    }
                }
                if (isVisible) {
                    currentInterestSet.insert(guid);
                }
            }
        }

        if (isDelta) {
            const Snapshot& base = m_snapshotHistory[clientState.lastAckedSnapshotId];
            
            // Build delta set based on interest set
            std::vector<EntityState> changedEntities;
            std::vector<Save::EntityGUID> destroyedEntities;

            // 1. Entities that were active/known to the client but are now out of interest or deleted
            for (const auto& guid : clientState.activeEntities) {
                if (currentInterestSet.find(guid) == currentInterestSet.end()) {
                    destroyedEntities.push_back(guid);
                }
            }

            // 2. Entities in the interest set
            for (const auto& guid : currentInterestSet) {
                auto currIt = current.entities.find(guid);
                if (currIt != current.entities.end()) {
                    const auto& currEnt = currIt->second;
                    // If not currently known, it entered interest -> creation
                    if (clientState.activeEntities.find(guid) == clientState.activeEntities.end()) {
                        changedEntities.push_back(currEnt);
                    } else {
                        // Already active/known. Check if it changed since base snapshot
                        auto baseIt = base.entities.find(guid);
                        if (baseIt == base.entities.end()) {
                            // Shouldn't happen if in activeEntities, but send just in case
                            changedEntities.push_back(currEnt);
                        } else {
                            const auto& baseEnt = baseIt->second;
                            bool parentChanged = currEnt.parentGuid != baseEnt.parentGuid;
                            
                            bool transformChanged = currEnt.components.hasTransform != baseEnt.components.hasTransform ||
                                (currEnt.components.hasTransform && !AreTransformsEqual(currEnt.components.transform, baseEnt.components.transform));

                            bool physicsChanged = currEnt.components.hasPhysics != baseEnt.components.hasPhysics ||
                                (currEnt.components.hasPhysics && !ArePhysicsEqual(currEnt.components.physics, baseEnt.components.physics));

                            bool ccChanged = currEnt.components.hasCharacterController != baseEnt.components.hasCharacterController ||
                                (currEnt.components.hasCharacterController && !AreCCsEqual(currEnt.components.characterController, baseEnt.components.characterController));

                            bool scriptChanged = currEnt.components.hasScript != baseEnt.components.hasScript ||
                                (currEnt.components.hasScript && currEnt.components.scriptPath != baseEnt.components.scriptPath);

                            bool healthChanged = currEnt.components.hasHealth != baseEnt.components.hasHealth ||
                                (currEnt.components.hasHealth && !AreHealthsEqual(currEnt.components.health, baseEnt.components.health));

                            bool teamChanged = currEnt.components.hasTeam != baseEnt.components.hasTeam ||
                                (currEnt.components.hasTeam && !AreTeamsEqual(currEnt.components.team, baseEnt.components.team));

                            bool playerStateChanged = currEnt.components.hasPlayerState != baseEnt.components.hasPlayerState ||
                                (currEnt.components.hasPlayerState && !ArePlayerStatesEqual(currEnt.components.playerState, baseEnt.components.playerState));

                            bool tagsChanged = currEnt.components.hasGameplayTags != baseEnt.components.hasGameplayTags ||
                                (currEnt.components.hasGameplayTags && !AreGameplayTagsEqual(currEnt.components.gameplayTags, baseEnt.components.gameplayTags));

                            bool inventoryChanged = currEnt.components.hasInventory != baseEnt.components.hasInventory ||
                                (currEnt.components.hasInventory && !AreInventoriesEqual(currEnt.components.inventory, baseEnt.components.inventory));

                            bool itemChanged = currEnt.components.hasItem != baseEnt.components.hasItem ||
                                (currEnt.components.hasItem && !AreItemsEqual(currEnt.components.item, baseEnt.components.item));

                            bool equipmentChanged = currEnt.components.hasEquipment != baseEnt.components.hasEquipment ||
                                (currEnt.components.hasEquipment && !AreEquipmentsEqual(currEnt.components.equipment, baseEnt.components.equipment));

                            bool questChanged = currEnt.components.hasQuest != baseEnt.components.hasQuest ||
                                (currEnt.components.hasQuest && !AreQuestsEqual(currEnt.components.quest, baseEnt.components.quest));

                            bool dialogueChanged = currEnt.components.hasDialogue != baseEnt.components.hasDialogue ||
                                (currEnt.components.hasDialogue && !AreDialoguesEqual(currEnt.components.dialogue, baseEnt.components.dialogue));

                            bool interactableChanged = currEnt.components.hasInteractable != baseEnt.components.hasInteractable ||
                                (currEnt.components.hasInteractable && !AreInteractablesEqual(currEnt.components.interactable, baseEnt.components.interactable));

                            bool triggerVolumeChanged = currEnt.components.hasTriggerVolume != baseEnt.components.hasTriggerVolume ||
                                (currEnt.components.hasTriggerVolume && !AreTriggerVolumesEqual(currEnt.components.triggerVolume, baseEnt.components.triggerVolume));

                            if (parentChanged || transformChanged || physicsChanged || ccChanged || scriptChanged ||
                                healthChanged || teamChanged || playerStateChanged || tagsChanged ||
                                inventoryChanged || itemChanged || equipmentChanged || questChanged ||
                                dialogueChanged || interactableChanged || triggerVolumeChanged) {
                                changedEntities.push_back(currEnt);
                            }
                        }
                    }
                }
            }

            // Update client's active entities tracking list
            for (const auto& guid : destroyedEntities) {
                clientState.activeEntities.erase(guid);
            }
            for (const auto& ent : changedEntities) {
                clientState.activeEntities.insert(ent.guid);
            }

            // Write number of entities in this delta payload
            uint32_t totalPayloadCount = static_cast<uint32_t>(destroyedEntities.size() + changedEntities.size());
            writer.WriteUint32(totalPayloadCount);

            // 1. Serialize destructions
            for (const auto& guid : destroyedEntities) {
                writer.WriteUint64(guid.high);
                writer.WriteUint64(guid.low);
                writer.WriteBool(true); // isDestroyed = true
            }

            // 2. Serialize creations/updates
            for (const auto& ent : changedEntities) {
                writer.WriteUint64(ent.guid.high);
                writer.WriteUint64(ent.guid.low);
                writer.WriteBool(false); // isDestroyed = false

                writer.WriteUint64(ent.parentGuid.high);
                writer.WriteUint64(ent.parentGuid.low);

                // Exist mask: what components are alive on this entity right now
                uint16_t componentExistMask = 0;
                if (ent.components.hasTransform) componentExistMask |= (1 << 0);
                if (ent.components.hasPhysics) componentExistMask |= (1 << 1);
                if (ent.components.hasCharacterController) componentExistMask |= (1 << 2);
                if (ent.components.hasScript) componentExistMask |= (1 << 3);
                if (ent.components.hasHealth) componentExistMask |= (1 << 4);
                if (ent.components.hasTeam) componentExistMask |= (1 << 5);
                if (ent.components.hasPlayerState) componentExistMask |= (1 << 6);
                if (ent.components.hasGameplayTags) componentExistMask |= (1 << 7);
                if (ent.components.hasInventory) componentExistMask |= (1 << 8);
                if (ent.components.hasItem) componentExistMask |= (1 << 9);
                if (ent.components.hasEquipment) componentExistMask |= (1 << 10);
                if (ent.components.hasQuest) componentExistMask |= (1 << 11);
                if (ent.components.hasDialogue) componentExistMask |= (1 << 12);
                if (ent.components.hasInteractable) componentExistMask |= (1 << 13);
                if (ent.components.hasTriggerVolume) componentExistMask |= (1 << 14);
                writer.WriteUint16(componentExistMask);

                // Changed mask: what components are included in this packet because they changed
                uint16_t componentChangedMask = 0;
                auto baseIt = base.entities.find(ent.guid);
                if (baseIt == base.entities.end()) {
                    // New entity: send all existing components
                    componentChangedMask = componentExistMask;
                } else {
                    const auto& baseEnt = baseIt->second;
                    if (ent.components.hasTransform && (!baseEnt.components.hasTransform || !AreTransformsEqual(ent.components.transform, baseEnt.components.transform))) {
                        componentChangedMask |= (1 << 0);
                    }
                    if (ent.components.hasPhysics && (!baseEnt.components.hasPhysics || !ArePhysicsEqual(ent.components.physics, baseEnt.components.physics))) {
                        componentChangedMask |= (1 << 1);
                    }
                    if (ent.components.hasCharacterController && (!baseEnt.components.hasCharacterController || !AreCCsEqual(ent.components.characterController, baseEnt.components.characterController))) {
                        componentChangedMask |= (1 << 2);
                    }
                    if (ent.components.hasScript && (!baseEnt.components.hasScript || ent.components.scriptPath != baseEnt.components.scriptPath)) {
                        componentChangedMask |= (1 << 3);
                    }
                    if (ent.components.hasHealth && (!baseEnt.components.hasHealth || !AreHealthsEqual(ent.components.health, baseEnt.components.health))) {
                        componentChangedMask |= (1 << 4);
                    }
                    if (ent.components.hasTeam && (!baseEnt.components.hasTeam || !AreTeamsEqual(ent.components.team, baseEnt.components.team))) {
                        componentChangedMask |= (1 << 5);
                    }
                    if (ent.components.hasPlayerState && (!baseEnt.components.hasPlayerState || !ArePlayerStatesEqual(ent.components.playerState, baseEnt.components.playerState))) {
                        componentChangedMask |= (1 << 6);
                    }
                    if (ent.components.hasGameplayTags && (!baseEnt.components.hasGameplayTags || !AreGameplayTagsEqual(ent.components.gameplayTags, baseEnt.components.gameplayTags))) {
                        componentChangedMask |= (1 << 7);
                    }
                    if (ent.components.hasInventory && (!baseEnt.components.hasInventory || !AreInventoriesEqual(ent.components.inventory, baseEnt.components.inventory))) {
                        componentChangedMask |= (1 << 8);
                    }
                    if (ent.components.hasItem && (!baseEnt.components.hasItem || !AreItemsEqual(ent.components.item, baseEnt.components.item))) {
                        componentChangedMask |= (1 << 9);
                    }
                    if (ent.components.hasEquipment && (!baseEnt.components.hasEquipment || !AreEquipmentsEqual(ent.components.equipment, baseEnt.components.equipment))) {
                        componentChangedMask |= (1 << 10);
                    }
                    if (ent.components.hasQuest && (!baseEnt.components.hasQuest || !AreQuestsEqual(ent.components.quest, baseEnt.components.quest))) {
                        componentChangedMask |= (1 << 11);
                    }
                    if (ent.components.hasDialogue && (!baseEnt.components.hasDialogue || !AreDialoguesEqual(ent.components.dialogue, baseEnt.components.dialogue))) {
                        componentChangedMask |= (1 << 12);
                    }
                    if (ent.components.hasInteractable && (!baseEnt.components.hasInteractable || !AreInteractablesEqual(ent.components.interactable, baseEnt.components.interactable))) {
                        componentChangedMask |= (1 << 13);
                    }
                    if (ent.components.hasTriggerVolume && (!baseEnt.components.hasTriggerVolume || !AreTriggerVolumesEqual(ent.components.triggerVolume, baseEnt.components.triggerVolume))) {
                        componentChangedMask |= (1 << 14);
                    }
                }

                // Write component changed count
                uint8_t changeCount = 0;
                for (int i = 0; i < 16; ++i) {
                    if (componentChangedMask & (1 << i)) changeCount++;
                }
                writer.WriteUint8(changeCount);

                // Serialize each changed component with payload sizes for safe skip
                for (int bit = 0; bit < 16; ++bit) {
                    if (componentChangedMask & (1 << bit)) {
                        writer.WriteUint8(static_cast<uint8_t>(bit));

                        // Serialize into temporary buffer to compute size
                        PacketWriter tempWriter(PacketId::Invalid);
                        EntityState dummyState = ent;
                        SerializeComponent(tempWriter, dummyState, (1 << bit));

                        uint32_t payloadSize = static_cast<uint32_t>(tempWriter.GetSize() - 6);
                        writer.WriteUint32(payloadSize);
                        writer.WriteBytes(tempWriter.GetData() + 6, payloadSize);
                    }
                }
            }

        } else {
            // Full snapshot containing only interest set
            writer.WriteUint32(static_cast<uint32_t>(currentInterestSet.size()));

            clientState.activeEntities = currentInterestSet;

            for (const auto& guid : currentInterestSet) {
                auto currIt = current.entities.find(guid);
                if (currIt == current.entities.end()) continue;
                const auto& ent = currIt->second;

                writer.WriteUint64(guid.high);
                writer.WriteUint64(guid.low);
                writer.WriteBool(false); // isDestroyed = false

                writer.WriteUint64(ent.parentGuid.high);
                writer.WriteUint64(ent.parentGuid.low);

                uint16_t componentExistMask = 0;
                if (ent.components.hasTransform) componentExistMask |= (1 << 0);
                if (ent.components.hasPhysics) componentExistMask |= (1 << 1);
                if (ent.components.hasCharacterController) componentExistMask |= (1 << 2);
                if (ent.components.hasScript) componentExistMask |= (1 << 3);
                if (ent.components.hasHealth) componentExistMask |= (1 << 4);
                if (ent.components.hasTeam) componentExistMask |= (1 << 5);
                if (ent.components.hasPlayerState) componentExistMask |= (1 << 6);
                if (ent.components.hasGameplayTags) componentExistMask |= (1 << 7);
                if (ent.components.hasInventory) componentExistMask |= (1 << 8);
                if (ent.components.hasItem) componentExistMask |= (1 << 9);
                if (ent.components.hasEquipment) componentExistMask |= (1 << 10);
                if (ent.components.hasQuest) componentExistMask |= (1 << 11);
                if (ent.components.hasDialogue) componentExistMask |= (1 << 12);
                if (ent.components.hasInteractable) componentExistMask |= (1 << 13);
                if (ent.components.hasTriggerVolume) componentExistMask |= (1 << 14);
                writer.WriteUint16(componentExistMask);

                // Write component count (all existing components are serialized)
                uint8_t count = 0;
                for (int i = 0; i < 16; ++i) {
                    if (componentExistMask & (1 << i)) count++;
                }
                writer.WriteUint8(count);

                for (int bit = 0; bit < 16; ++bit) {
                    if (componentExistMask & (1 << bit)) {
                        writer.WriteUint8(static_cast<uint8_t>(bit));

                        PacketWriter tempWriter(PacketId::Invalid);
                        EntityState dummyState = ent;
                        SerializeComponent(tempWriter, dummyState, (1 << bit));

                        uint32_t payloadSize = static_cast<uint32_t>(tempWriter.GetSize() - 6);
                        writer.WriteUint32(payloadSize);
                        writer.WriteBytes(tempWriter.GetData() + 6, payloadSize);
                    }
                }
            }
        }

        // Append last processed input sequence at the end of the packet for compatibility
        writer.WriteUint32(clientState.lastProcessedInputSequence);

        // Send replication payload to the client
        NetworkManager::Get().Send(peerId, writer, true);
    }

    m_nextSnapshotId++;
}

void ReplicationManager::ClientProcessSnapshot(PacketReader& reader) {
    ScopedTimer timer(m_lastDeserializationTimeMs);
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_registry) {
        Core::Logger::Error("Replication", "Client registry not initialized in ReplicationManager.");
        return;
    }

    uint32_t snapshotId = 0;
    bool isDelta = false;
    uint32_t baseSnapshotId = 0;
    uint32_t entityCount = 0;

    if (!reader.ReadUint32(snapshotId) ||
        !reader.ReadBool(isDelta) ||
        !reader.ReadUint32(baseSnapshotId) ||
        !reader.ReadUint32(entityCount)) {
        Core::Logger::Error("Replication", "Malformed snapshot packet: header read failed.");
        return;
    }

    std::unordered_set<Save::EntityGUID> receivedGUIDs;

    for (uint32_t i = 0; i < entityCount; ++i) {
        Save::EntityGUID guid;
        bool isDestroyed = false;

        if (!reader.ReadUint64(guid.high) ||
            !reader.ReadUint64(guid.low) ||
            !reader.ReadBool(isDestroyed)) {
            Core::Logger::Error("Replication", "Malformed snapshot packet: entity header read failed.");
            return;
        }

        if (isDestroyed) {
            if (isDelta) {
                ECS::Entity entity = m_registry->GetEntityByGUID(guid);
                Scene::SceneNode* node = nullptr;
                if (Scene::SceneManager::Get().GetRegistry() == m_registry) {
                    node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                }
                if (node) {
                    Scene::SceneManager::Get().DestroyNode(node);
                } else {
                    m_registry->DestroyEntity(entity);
                }
                m_clientReplicatedGUIDs.erase(guid);
                m_pendingParents.erase(guid);
            }
            continue;
        }

        Save::EntityGUID parentGUID;
        uint16_t componentExistMask = 0;
        uint8_t componentCount = 0;

        if (!reader.ReadUint64(parentGUID.high) ||
            !reader.ReadUint64(parentGUID.low) ||
            !reader.ReadUint16(componentExistMask) ||
            !reader.ReadUint8(componentCount)) {
            Core::Logger::Error("Replication", "Malformed snapshot packet: component metadata read failed.");
            return;
        }

        receivedGUIDs.insert(guid);

        ECS::Entity entity = m_registry->GetEntityByGUID(guid);
        if (entity == ECS::NULL_ENTITY) {
            // GUID Conflict Detection: check if GUID exists but is NOT a client-replicated entity
            // (e.g. registered externally)
            // Wait, GetEntityByGUID returned NULL_ENTITY, so there is no conflict.
            // If it had returned a valid entity, but we didn't track it in m_clientReplicatedGUIDs,
            // that would be a conflict.
            entity = m_registry->CreateEntity();
            m_registry->AssignGUID(entity, guid);
            m_clientReplicatedGUIDs.insert(guid);

            // Automatically construct SceneNode representing the replicated entity
            if (Scene::SceneManager::Get().GetRegistry() == m_registry) {
                Scene::SceneNode* node = Scene::SceneManager::Get().CreateNode("Replicated_" + guid.ToString());
                if (node) {
                    node->SetEntity(entity);
                }
            }
        } else {
            // GUID Conflict Detection
            if (m_clientReplicatedGUIDs.find(guid) == m_clientReplicatedGUIDs.end()) {
                Core::Logger::Warning("Replication", "GUID conflict detected! GUID %s belongs to a local, non-replicated entity. Re-mapping.", guid.ToString().c_str());
                // Handle: override and claim entity for replication
                m_clientReplicatedGUIDs.insert(guid);
            }
        }

        // Store pending parent relationship mapping
        m_pendingParents[guid] = parentGUID;

        // Process components
        for (uint8_t j = 0; j < componentCount; ++j) {
            uint8_t componentBit = 0;
            uint32_t payloadSize = 0;

            if (!reader.ReadUint8(componentBit) || !reader.ReadUint32(payloadSize)) {
                Core::Logger::Error("Replication", "Malformed snapshot packet: component payload metadata read failed.");
                return;
            }

            size_t nextOffset = reader.GetOffset() + payloadSize;

            if (componentBit < 16) {
                // Supported components: 0 to 14
                if (!DeserializeComponent(reader, entity, (1 << componentBit))) {
                    Core::Logger::Warning("Replication", "Failed to deserialize component %d for entity.", componentBit);
                    reader.ClearError();
                }
            } else {
                // Ignore unknown component types safely by skipping payload size
                reader.Skip(payloadSize);
            }

            if (reader.GetOffset() != nextOffset) {
                reader.Seek(nextOffset);
            }
        }

        // Prune components removed on server
        if (!(componentExistMask & (1 << 0)) && m_registry->HasComponent<Scene::TransformComponent>(entity)) {
            m_registry->RemoveComponent<Scene::TransformComponent>(entity);
        }
        if (!(componentExistMask & (1 << 1)) && m_registry->HasComponent<Physics::PhysicsComponent>(entity)) {
            m_registry->RemoveComponent<Physics::PhysicsComponent>(entity);
        }
        if (!(componentExistMask & (1 << 2)) && m_registry->HasComponent<Physics::CharacterControllerComponent>(entity)) {
            m_registry->RemoveComponent<Physics::CharacterControllerComponent>(entity);
        }
        if (!(componentExistMask & (1 << 3)) && m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
            m_registry->RemoveComponent<ECS::ScriptComponent>(entity);
        }
        if (!(componentExistMask & (1 << 4)) && m_registry->HasComponent<Gameplay::HealthComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::HealthComponent>(entity);
        }
        if (!(componentExistMask & (1 << 5)) && m_registry->HasComponent<Gameplay::TeamComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::TeamComponent>(entity);
        }
        if (!(componentExistMask & (1 << 6)) && m_registry->HasComponent<Gameplay::PlayerStateComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::PlayerStateComponent>(entity);
        }
        if (!(componentExistMask & (1 << 7)) && m_registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::GameplayTagsComponent>(entity);
        }
        if (!(componentExistMask & (1 << 8)) && m_registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::InventoryComponent>(entity);
        }
        if (!(componentExistMask & (1 << 9)) && m_registry->HasComponent<Gameplay::ItemComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::ItemComponent>(entity);
        }
        if (!(componentExistMask & (1 << 10)) && m_registry->HasComponent<Gameplay::EquipmentComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::EquipmentComponent>(entity);
        }
        if (!(componentExistMask & (1 << 11)) && m_registry->HasComponent<Gameplay::QuestComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::QuestComponent>(entity);
        }
        if (!(componentExistMask & (1 << 12)) && m_registry->HasComponent<Gameplay::DialogueComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::DialogueComponent>(entity);
        }
        if (!(componentExistMask & (1 << 13)) && m_registry->HasComponent<Gameplay::InteractableComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::InteractableComponent>(entity);
        }
        if (!(componentExistMask & (1 << 14)) && m_registry->HasComponent<Gameplay::TriggerVolumeComponent>(entity)) {
            m_registry->RemoveComponent<Gameplay::TriggerVolumeComponent>(entity);
        }
    }

    // Stale entity removal for Full Snapshots
    if (!isDelta) {
        std::vector<Save::EntityGUID> staleGUIDs;
        for (const auto& guid : m_clientReplicatedGUIDs) {
            if (receivedGUIDs.find(guid) == receivedGUIDs.end()) {
                staleGUIDs.push_back(guid);
            }
        }

        for (const auto& guid : staleGUIDs) {
            ECS::Entity entity = m_registry->GetEntityByGUID(guid);
            if (entity != ECS::NULL_ENTITY) {
                Scene::SceneNode* node = nullptr;
                if (Scene::SceneManager::Get().GetRegistry() == m_registry) {
                    node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                }
                if (node) {
                    Scene::SceneManager::Get().DestroyNode(node);
                } else {
                    m_registry->DestroyEntity(entity);
                }
            }
            m_clientReplicatedGUIDs.erase(guid);
            m_pendingParents.erase(guid);
        }
    }

    // Resolve parent hierarchy transformations
    ResolvePendingParents();

    // Reject packet if reading failed at any point
    if (reader.HasError()) {
        Core::Logger::Error("Replication", "Rejecting malformed packet snapshot due to parsing errors.");
        return;
    }

    // Read lastProcessedInputSequence from the end of the packet if available
    uint32_t lastProcessedInputSequence = 0;
    if (reader.GetBytesRemaining() >= sizeof(uint32_t)) {
        reader.ReadUint32(lastProcessedInputSequence);
    }

    // Run client prediction and reconciliation before ack
    ClientPredictionSystem::Get().OnServerSnapshotReceived(lastProcessedInputSequence, CaptureCurrentSnapshot());

    // Acknowledge receipt of snapshot to authoritative server
    ClientSendAck(snapshotId);
}

void ReplicationManager::ClientSendAck(uint32_t snapshotId) {
    PacketWriter writer(PacketId::ReplicationAck);
    writer.WriteUint32(snapshotId);
    // Send to peer ID 1 (authoritative server)
    NetworkManager::Get().Send(1, writer, true);
}

void ReplicationManager::OnPeerConnected(uint32_t peerId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clientStates[peerId] = ClientReplicationState{ 0, false };
}

void ReplicationManager::OnPeerDisconnected(uint32_t peerId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clientStates.erase(peerId);
}

void ReplicationManager::SerializeComponent(PacketWriter& writer, const EntityState& entityState, uint32_t componentBitmask) {
    auto startTime = std::chrono::high_resolution_clock::now();
    if (componentBitmask & (1 << 0)) { // Transform
        const auto& t = entityState.components.transform;
        writer.WriteFloat(t.position.x);
        writer.WriteFloat(t.position.y);
        writer.WriteFloat(t.position.z);
        
        // Write compressed quaternion rotation
        writer.WriteUint32(CompressQuat(t.rotation));
        
        writer.WriteFloat(t.scale.x);
        writer.WriteFloat(t.scale.y);
        writer.WriteFloat(t.scale.z);
    }
    if (componentBitmask & (1 << 1)) { // Physics
        const auto& p = entityState.components.physics;
        writer.WriteUint8(static_cast<uint8_t>(p.bodyType));
        writer.WriteFloat(p.mass);
        writer.WriteFloat(p.inverseMass);
        writer.WriteFloat(p.velocity.x);
        writer.WriteFloat(p.velocity.y);
        writer.WriteFloat(p.velocity.z);
        writer.WriteFloat(p.angularVelocity.x);
        writer.WriteFloat(p.angularVelocity.y);
        writer.WriteFloat(p.angularVelocity.z);
        writer.WriteFloat(p.forceAccum.x);
        writer.WriteFloat(p.forceAccum.y);
        writer.WriteFloat(p.forceAccum.z);
        writer.WriteFloat(p.torqueAccum.x);
        writer.WriteFloat(p.torqueAccum.y);
        writer.WriteFloat(p.torqueAccum.z);
        writer.WriteFloat(p.friction);
        writer.WriteFloat(p.restitution);
        writer.WriteUint32(p.collisionLayer);
        writer.WriteUint32(p.collisionMask);
        writer.WriteUint8(static_cast<uint8_t>(p.collider.type));
        writer.WriteBool(p.collider.isTrigger);
        if (p.collider.type == Physics::ColliderType::AABB) {
            auto s = std::get<Physics::AABB>(p.collider.shape);
            writer.WriteFloat(s.min.x); writer.WriteFloat(s.min.y); writer.WriteFloat(s.min.z);
            writer.WriteFloat(s.max.x); writer.WriteFloat(s.max.y); writer.WriteFloat(s.max.z);
        } else if (p.collider.type == Physics::ColliderType::Sphere) {
            auto s = std::get<Physics::Sphere>(p.collider.shape);
            writer.WriteFloat(s.center.x); writer.WriteFloat(s.center.y); writer.WriteFloat(s.center.z);
            writer.WriteFloat(s.radius);
        } else if (p.collider.type == Physics::ColliderType::Capsule) {
            auto s = std::get<Physics::Capsule>(p.collider.shape);
            writer.WriteFloat(s.center.x); writer.WriteFloat(s.center.y); writer.WriteFloat(s.center.z);
            writer.WriteFloat(s.halfHeight);
            writer.WriteFloat(s.radius);
        }
    }
    if (componentBitmask & (1 << 2)) { // CC
        const auto& c = entityState.components.characterController;
        writer.WriteUint8(static_cast<uint8_t>(c.state));
        writer.WriteFloat(c.walkSpeed);
        writer.WriteFloat(c.runSpeed);
        writer.WriteFloat(c.sprintSpeed);
        writer.WriteFloat(c.jumpForce);
        writer.WriteFloat(c.gravityMultiplier);
        writer.WriteFloat(c.moveDirection.x); writer.WriteFloat(c.moveDirection.y); writer.WriteFloat(c.moveDirection.z);
        writer.WriteBool(c.requestJump);
        writer.WriteBool(c.isGrounded);
        writer.WriteFloat(c.verticalVelocity);
        writer.WriteFloat(c.slopeLimit);
        writer.WriteFloat(c.stepHeight);
    }
    if (componentBitmask & (1 << 3)) { // Script metadata
        writer.WriteString(entityState.components.scriptPath);
    }
    if (componentBitmask & (1 << 4)) { // Health
        const auto& h = entityState.components.health;
        writer.WriteFloat(h.currentHealth);
        writer.WriteFloat(h.maxHealth);
        writer.WriteFloat(h.shield);
        writer.WriteBool(h.invulnerable);
    }
    if (componentBitmask & (1 << 5)) { // Team
        const auto& t = entityState.components.team;
        writer.WriteInt32(t.teamId);
        writer.WriteBool(t.friendlyFire);
    }
    if (componentBitmask & (1 << 6)) { // PlayerState
        const auto& p = entityState.components.playerState;
        writer.WriteString(p.playerName);
        writer.WriteUint32(p.peerId);
        writer.WriteFloat(p.score);
        writer.WriteInt32(p.teamId);
        writer.WriteFloat(p.ping);
    }
    if (componentBitmask & (1 << 7)) { // GameplayTags
        const auto& g = entityState.components.gameplayTags;
        auto rawTags = g.GetRawTags();
        writer.WriteUint32(static_cast<uint32_t>(rawTags.size()));
        for (const auto& tag : rawTags) {
            writer.WriteString(tag);
        }
    }
    if (componentBitmask & (1 << 8)) { // Inventory
        const auto& ic = entityState.components.inventory;
        writer.WriteUint32(ic.maxSlots);
        writer.WriteUint32(static_cast<uint32_t>(ic.slots.size()));
        for (const auto& slot : ic.slots) {
            writer.WriteString(slot.itemId);
            writer.WriteUint32(slot.quantity);
        }
    }
    if (componentBitmask & (1 << 9)) { // Item
        const auto& ic = entityState.components.item;
        writer.WriteString(ic.itemId);
        writer.WriteUint32(ic.quantity);
    }
    if (componentBitmask & (1 << 10)) { // Equipment
        const auto& ec = entityState.components.equipment;
        writer.WriteUint32(static_cast<uint32_t>(ec.slots.size()));
        for (const auto& item : ec.slots) {
            writer.WriteString(item);
        }
    }
    if (componentBitmask & (1 << 11)) { // Quest
        const auto& qc = entityState.components.quest;
        writer.WriteUint32(static_cast<uint32_t>(qc.completedQuests.size()));
        for (const auto& q : qc.completedQuests) {
            writer.WriteString(q);
        }
        writer.WriteUint32(static_cast<uint32_t>(qc.activeQuests.size()));
        for (const auto& [questId, state] : qc.activeQuests) {
            writer.WriteString(questId);
            writer.WriteInt32(state.currentStageIndex);
            writer.WriteBool(state.isCompleted);
            writer.WriteUint32(static_cast<uint32_t>(state.objectiveProgress.size()));
            for (const auto& [objId, count] : state.objectiveProgress) {
                writer.WriteString(objId);
                writer.WriteInt32(count);
            }
        }
    }
    if (componentBitmask & (1 << 12)) { // Dialogue
        const auto& dc = entityState.components.dialogue;
        writer.WriteString(dc.currentDialogueId);
        writer.WriteString(dc.currentNodeId);
        writer.WriteBool(dc.isInDialogue);
    }
    if (componentBitmask & (1 << 13)) { // Interactable
        const auto& ic = entityState.components.interactable;
        writer.WriteString(ic.prompt);
        writer.WriteFloat(ic.distance);
        writer.WriteBool(ic.isInteractable);
        writer.WriteString(ic.interactionType);
        writer.WriteString(ic.targetData);
        writer.WriteString(ic.onInteractLua);
    }
    if (componentBitmask & (1 << 14)) { // TriggerVolume
        const auto& tvc = entityState.components.triggerVolume;
        writer.WriteUint8(static_cast<uint8_t>(tvc.type));
        writer.WriteString(tvc.onEnterLua);
        writer.WriteString(tvc.onExitLua);
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastSerializationTimeMs.store(m_lastSerializationTimeMs.load() + std::chrono::duration<double, std::milli>(endTime - startTime).count());
}

bool ReplicationManager::DeserializeComponent(PacketReader& reader, ECS::Entity entity, uint32_t componentBitmask) {
    auto startTime = std::chrono::high_resolution_clock::now();
    bool ok = true;
    if (componentBitmask & (1 << 0)) { // Transform
        glm::vec3 pos(0.0f);
        glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scl(1.0f);
        ok &= reader.ReadFloat(pos.x); ok &= reader.ReadFloat(pos.y); ok &= reader.ReadFloat(pos.z);
        
        uint32_t compRot = 0;
        ok &= reader.ReadUint32(compRot);
        if (ok) {
            rot = DecompressQuat(compRot);
        }
        
        ok &= reader.ReadFloat(scl.x); ok &= reader.ReadFloat(scl.y); ok &= reader.ReadFloat(scl.z);
        if (ok) {
            if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
                auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                tc.position = pos; tc.rotation = rot; tc.scale = scl;
            } else {
                m_registry->AddComponent<Scene::TransformComponent>(entity, pos, rot, scl);
            }
            // Update SceneNode transforms
            if (Scene::SceneManager::Get().GetRegistry() == m_registry) {
                auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                if (node) {
                    node->SetLocalPosition(pos);
                    node->SetLocalRotation(rot);
                    node->SetLocalScale(scl);
                }
            }
        }
    }
    if (componentBitmask & (1 << 1)) { // Physics
        uint8_t bType;
        float mass, invMass;
        glm::vec3 vel, angVel, force, torque;
        float friction, restitution;
        uint32_t colLayer, colMask;
        uint8_t colType;
        bool isTrigger;

        ok &= reader.ReadUint8(bType);
        ok &= reader.ReadFloat(mass);
        ok &= reader.ReadFloat(invMass);
        ok &= reader.ReadFloat(vel.x); ok &= reader.ReadFloat(vel.y); ok &= reader.ReadFloat(vel.z);
        ok &= reader.ReadFloat(angVel.x); ok &= reader.ReadFloat(angVel.y); ok &= reader.ReadFloat(angVel.z);
        ok &= reader.ReadFloat(force.x); ok &= reader.ReadFloat(force.y); ok &= reader.ReadFloat(force.z);
        ok &= reader.ReadFloat(torque.x); ok &= reader.ReadFloat(torque.y); ok &= reader.ReadFloat(torque.z);
        ok &= reader.ReadFloat(friction);
        ok &= reader.ReadFloat(restitution);
        ok &= reader.ReadUint32(colLayer);
        ok &= reader.ReadUint32(colMask);
        ok &= reader.ReadUint8(colType);
        ok &= reader.ReadBool(isTrigger);

        Physics::Collider collider;
        collider.type = static_cast<Physics::ColliderType>(colType);
        collider.isTrigger = isTrigger;

        if (collider.type == Physics::ColliderType::AABB) {
            Physics::AABB aabb;
            ok &= reader.ReadFloat(aabb.min.x); ok &= reader.ReadFloat(aabb.min.y); ok &= reader.ReadFloat(aabb.min.z);
            ok &= reader.ReadFloat(aabb.max.x); ok &= reader.ReadFloat(aabb.max.y); ok &= reader.ReadFloat(aabb.max.z);
            collider.shape = aabb;
        } else if (collider.type == Physics::ColliderType::Sphere) {
            Physics::Sphere sphere;
            ok &= reader.ReadFloat(sphere.center.x); ok &= reader.ReadFloat(sphere.center.y); ok &= reader.ReadFloat(sphere.center.z);
            ok &= reader.ReadFloat(sphere.radius);
            collider.shape = sphere;
        } else if (collider.type == Physics::ColliderType::Capsule) {
            Physics::Capsule capsule;
            ok &= reader.ReadFloat(capsule.center.x); ok &= reader.ReadFloat(capsule.center.y); ok &= reader.ReadFloat(capsule.center.z);
            ok &= reader.ReadFloat(capsule.halfHeight);
            ok &= reader.ReadFloat(capsule.radius);
            collider.shape = capsule;
        } else {
            collider.shape = std::monostate{};
        }

        if (ok) {
            if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) {
                auto& p = m_registry->GetComponent<Physics::PhysicsComponent>(entity);
                p.bodyType = static_cast<Physics::BodyType>(bType);
                p.mass = mass;
                p.inverseMass = invMass;
                p.velocity = vel;
                p.angularVelocity = angVel;
                p.forceAccum = force;
                p.torqueAccum = torque;
                p.friction = friction;
                p.restitution = restitution;
                p.collisionLayer = colLayer;
                p.collisionMask = colMask;
                p.collider = collider;
            } else {
                auto& p = m_registry->AddComponent<Physics::PhysicsComponent>(entity);
                p.bodyType = static_cast<Physics::BodyType>(bType);
                p.mass = mass;
                p.inverseMass = invMass;
                p.velocity = vel;
                p.angularVelocity = angVel;
                p.forceAccum = force;
                p.torqueAccum = torque;
                p.friction = friction;
                p.restitution = restitution;
                p.collisionLayer = colLayer;
                p.collisionMask = colMask;
                p.collider = collider;
            }
        }
    }
    if (componentBitmask & (1 << 2)) { // CC
        uint8_t state;
        float walk, run, sprint, jump, grav;
        glm::vec3 moveDir;
        bool reqJump, grounded;
        float vertVel, slope, step;

        ok &= reader.ReadUint8(state);
        ok &= reader.ReadFloat(walk);
        ok &= reader.ReadFloat(run);
        ok &= reader.ReadFloat(sprint);
        ok &= reader.ReadFloat(jump);
        ok &= reader.ReadFloat(grav);
        ok &= reader.ReadFloat(moveDir.x); ok &= reader.ReadFloat(moveDir.y); ok &= reader.ReadFloat(moveDir.z);
        ok &= reader.ReadBool(reqJump);
        ok &= reader.ReadBool(grounded);
        ok &= reader.ReadFloat(vertVel);
        ok &= reader.ReadFloat(slope);
        ok &= reader.ReadFloat(step);

        if (ok) {
            if (m_registry->HasComponent<Physics::CharacterControllerComponent>(entity)) {
                auto& c = m_registry->GetComponent<Physics::CharacterControllerComponent>(entity);
                c.state = static_cast<Physics::MovementState>(state);
                c.walkSpeed = walk;
                c.runSpeed = run;
                c.sprintSpeed = sprint;
                c.jumpForce = jump;
                c.gravityMultiplier = grav;
                c.moveDirection = moveDir;
                c.requestJump = reqJump;
                c.isGrounded = grounded;
                c.verticalVelocity = vertVel;
                c.slopeLimit = slope;
                c.stepHeight = step;
            } else {
                auto& c = m_registry->AddComponent<Physics::CharacterControllerComponent>(entity);
                c.state = static_cast<Physics::MovementState>(state);
                c.walkSpeed = walk;
                c.runSpeed = run;
                c.sprintSpeed = sprint;
                c.jumpForce = jump;
                c.gravityMultiplier = grav;
                c.moveDirection = moveDir;
                c.requestJump = reqJump;
                c.isGrounded = grounded;
                c.verticalVelocity = vertVel;
                c.slopeLimit = slope;
                c.stepHeight = step;
            }
        }
    }
    if (componentBitmask & (1 << 3)) { // Script metadata
        std::string scriptPath;
        ok &= reader.ReadString(scriptPath);
        if (ok) {
            if (m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
                auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
                if (sc.scriptPath != scriptPath) {
                    m_registry->RemoveComponent<ECS::ScriptComponent>(entity);
                    m_registry->AddComponent<ECS::ScriptComponent>(entity, scriptPath);
                }
            } else {
                m_registry->AddComponent<ECS::ScriptComponent>(entity, scriptPath);
            }
        }
    }
    if (componentBitmask & (1 << 4)) { // Health
        float currentHealth, maxHealth, shield;
        bool invulnerable;
        ok &= reader.ReadFloat(currentHealth);
        ok &= reader.ReadFloat(maxHealth);
        ok &= reader.ReadFloat(shield);
        ok &= reader.ReadBool(invulnerable);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::HealthComponent>(entity)) {
                auto& hc = m_registry->GetComponent<Gameplay::HealthComponent>(entity);
                hc.currentHealth = currentHealth;
                hc.maxHealth = maxHealth;
                hc.shield = shield;
                hc.invulnerable = invulnerable;
            } else {
                auto& hc = m_registry->AddComponent<Gameplay::HealthComponent>(entity);
                hc.currentHealth = currentHealth;
                hc.maxHealth = maxHealth;
                hc.shield = shield;
                hc.invulnerable = invulnerable;
            }
        }
    }
    if (componentBitmask & (1 << 5)) { // Team
        int32_t teamId;
        bool friendlyFire;
        ok &= reader.ReadInt32(teamId);
        ok &= reader.ReadBool(friendlyFire);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::TeamComponent>(entity)) {
                auto& tc = m_registry->GetComponent<Gameplay::TeamComponent>(entity);
                tc.teamId = teamId;
                tc.friendlyFire = friendlyFire;
            } else {
                auto& tc = m_registry->AddComponent<Gameplay::TeamComponent>(entity);
                tc.teamId = teamId;
                tc.friendlyFire = friendlyFire;
            }
        }
    }
    if (componentBitmask & (1 << 6)) { // PlayerState
        std::string playerName;
        uint32_t peerId;
        float score;
        int32_t teamId;
        float ping;
        ok &= reader.ReadString(playerName);
        ok &= reader.ReadUint32(peerId);
        ok &= reader.ReadFloat(score);
        ok &= reader.ReadInt32(teamId);
        ok &= reader.ReadFloat(ping);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::PlayerStateComponent>(entity)) {
                auto& psc = m_registry->GetComponent<Gameplay::PlayerStateComponent>(entity);
                psc.playerName = playerName;
                psc.peerId = peerId;
                psc.score = score;
                psc.teamId = teamId;
                psc.ping = ping;
            } else {
                auto& psc = m_registry->AddComponent<Gameplay::PlayerStateComponent>(entity);
                psc.playerName = playerName;
                psc.peerId = peerId;
                psc.score = score;
                psc.teamId = teamId;
                psc.ping = ping;
            }
        }
    }
    if (componentBitmask & (1 << 7)) { // GameplayTags
        uint32_t tagCount;
        ok &= reader.ReadUint32(tagCount);
        std::vector<std::string> rawTags;
        for (uint32_t k = 0; k < tagCount; ++k) {
            std::string tagStr;
            ok &= reader.ReadString(tagStr);
            rawTags.push_back(tagStr);
        }
        if (ok) {
            if (m_registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
                auto& gtc = m_registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
                gtc.tags.clear();
                for (const auto& tagStr : rawTags) {
                    gtc.AddTag(tagStr);
                }
            } else {
                auto& gtc = m_registry->AddComponent<Gameplay::GameplayTagsComponent>(entity);
                for (const auto& tagStr : rawTags) {
                    gtc.AddTag(tagStr);
                }
            }
        }
    }
    if (componentBitmask & (1 << 8)) { // Inventory
        uint32_t maxSlots;
        uint32_t numSlots;
        ok &= reader.ReadUint32(maxSlots);
        ok &= reader.ReadUint32(numSlots);
        std::vector<Gameplay::InventorySlot> tempSlots(numSlots);
        for (uint32_t k = 0; k < numSlots; ++k) {
            ok &= reader.ReadString(tempSlots[k].itemId);
            ok &= reader.ReadUint32(tempSlots[k].quantity);
        }
        if (ok) {
            if (m_registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
                auto& ic = m_registry->GetComponent<Gameplay::InventoryComponent>(entity);
                ic.maxSlots = maxSlots;
                ic.slots = tempSlots;
            } else {
                auto& ic = m_registry->AddComponent<Gameplay::InventoryComponent>(entity, maxSlots);
                ic.slots = tempSlots;
            }
        }
    }
    if (componentBitmask & (1 << 9)) { // Item
        std::string itemId;
        uint32_t quantity;
        ok &= reader.ReadString(itemId);
        ok &= reader.ReadUint32(quantity);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::ItemComponent>(entity)) {
                auto& ic = m_registry->GetComponent<Gameplay::ItemComponent>(entity);
                ic.itemId = itemId;
                ic.quantity = quantity;
            } else {
                auto& ic = m_registry->AddComponent<Gameplay::ItemComponent>(entity);
                ic.itemId = itemId;
                ic.quantity = quantity;
            }
        }
    }
    if (componentBitmask & (1 << 10)) { // Equipment
        uint32_t numSlots;
        ok &= reader.ReadUint32(numSlots);
        std::vector<std::string> tempSlots(numSlots);
        for (uint32_t k = 0; k < numSlots; ++k) {
            ok &= reader.ReadString(tempSlots[k]);
        }
        if (ok) {
            if (m_registry->HasComponent<Gameplay::EquipmentComponent>(entity)) {
                auto& ec = m_registry->GetComponent<Gameplay::EquipmentComponent>(entity);
                for (size_t k = 0; k < (std::min)(static_cast<size_t>(numSlots), ec.slots.size()); ++k) {
                    ec.slots[k] = tempSlots[k];
                }
            } else {
                auto& ec = m_registry->AddComponent<Gameplay::EquipmentComponent>(entity);
                for (size_t k = 0; k < (std::min)(static_cast<size_t>(numSlots), ec.slots.size()); ++k) {
                    ec.slots[k] = tempSlots[k];
                }
            }
        }
    }
    if (componentBitmask & (1 << 11)) { // Quest
        uint32_t completedCount;
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
            if (m_registry->HasComponent<Gameplay::QuestComponent>(entity)) {
                auto& qc = m_registry->GetComponent<Gameplay::QuestComponent>(entity);
                qc.completedQuests = compQuests;
                qc.activeQuests = actQuests;
            } else {
                auto& qc = m_registry->AddComponent<Gameplay::QuestComponent>(entity);
                qc.completedQuests = compQuests;
                qc.activeQuests = actQuests;
            }
        }
    }
    if (componentBitmask & (1 << 12)) { // Dialogue
        std::string currentDialogueId;
        std::string currentNodeId;
        bool isInDialogue;
        ok &= reader.ReadString(currentDialogueId);
        ok &= reader.ReadString(currentNodeId);
        ok &= reader.ReadBool(isInDialogue);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::DialogueComponent>(entity)) {
                auto& dc = m_registry->GetComponent<Gameplay::DialogueComponent>(entity);
                dc.currentDialogueId = currentDialogueId;
                dc.currentNodeId = currentNodeId;
                dc.isInDialogue = isInDialogue;
            } else {
                auto& dc = m_registry->AddComponent<Gameplay::DialogueComponent>(entity);
                dc.currentDialogueId = currentDialogueId;
                dc.currentNodeId = currentNodeId;
                dc.isInDialogue = isInDialogue;
            }
        }
    }
    if (componentBitmask & (1 << 13)) { // Interactable
        std::string prompt;
        float distance;
        bool isInteractable;
        std::string interactionType;
        std::string targetData;
        std::string onInteractLua;
        ok &= reader.ReadString(prompt);
        ok &= reader.ReadFloat(distance);
        ok &= reader.ReadBool(isInteractable);
        ok &= reader.ReadString(interactionType);
        ok &= reader.ReadString(targetData);
        ok &= reader.ReadString(onInteractLua);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::InteractableComponent>(entity)) {
                auto& ic = m_registry->GetComponent<Gameplay::InteractableComponent>(entity);
                ic.prompt = prompt;
                ic.distance = distance;
                ic.isInteractable = isInteractable;
                ic.interactionType = interactionType;
                ic.targetData = targetData;
                ic.onInteractLua = onInteractLua;
            } else {
                auto& ic = m_registry->AddComponent<Gameplay::InteractableComponent>(entity);
                ic.prompt = prompt;
                ic.distance = distance;
                ic.isInteractable = isInteractable;
                ic.interactionType = interactionType;
                ic.targetData = targetData;
                ic.onInteractLua = onInteractLua;
            }
        }
    }
    if (componentBitmask & (1 << 14)) { // TriggerVolume
        uint8_t typeVal;
        std::string onEnterLua;
        std::string onExitLua;
        ok &= reader.ReadUint8(typeVal);
        ok &= reader.ReadString(onEnterLua);
        ok &= reader.ReadString(onExitLua);
        if (ok) {
            if (m_registry->HasComponent<Gameplay::TriggerVolumeComponent>(entity)) {
                auto& tvc = m_registry->GetComponent<Gameplay::TriggerVolumeComponent>(entity);
                tvc.type = static_cast<Physics::ColliderType>(typeVal);
                tvc.onEnterLua = onEnterLua;
                tvc.onExitLua = onExitLua;
            } else {
                auto& tvc = m_registry->AddComponent<Gameplay::TriggerVolumeComponent>(entity);
                tvc.type = static_cast<Physics::ColliderType>(typeVal);
                tvc.onEnterLua = onEnterLua;
                tvc.onExitLua = onExitLua;
            }
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastDeserializationTimeMs.store(m_lastDeserializationTimeMs.load() + std::chrono::duration<double, std::milli>(endTime - startTime).count());
    return ok;
}

void ReplicationManager::ResolvePendingParents() {
    if (Scene::SceneManager::Get().GetRegistry() != m_registry) {
        m_pendingParents.clear();
        return;
    }
    auto it = m_pendingParents.begin();
    while (it != m_pendingParents.end()) {
        Save::EntityGUID childGUID = it->first;
        Save::EntityGUID parentGUID = it->second;

        ECS::Entity childEntity = m_registry->GetEntityByGUID(childGUID);
        ECS::Entity parentEntity = m_registry->GetEntityByGUID(parentGUID);

        if (childEntity != ECS::NULL_ENTITY) {
            Scene::SceneNode* childNode = Scene::SceneManager::Get().GetNodeByEntity(childEntity);
            
            if (parentGUID.IsNull()) {
                // Move back to root node if child currently has a parent node that is not root
                if (childNode && childNode->GetParent() && childNode->GetParent() != Scene::SceneManager::Get().GetRootNode()) {
                    auto childUniquePtr = childNode->GetParent()->RemoveChild(childNode);
                    if (childUniquePtr) {
                        Scene::SceneManager::Get().GetRootNode()->AddChild(std::move(childUniquePtr));
                    }
                }
                it = m_pendingParents.erase(it);
                continue;
            } else if (parentEntity != ECS::NULL_ENTITY) {
                Scene::SceneNode* parentNode = Scene::SceneManager::Get().GetNodeByEntity(parentEntity);

                if (childNode && parentNode) {
                    if (childNode->GetParent() != parentNode) {
                        std::unique_ptr<Scene::SceneNode> childUniquePtr;
                        if (childNode->GetParent()) {
                            childUniquePtr = childNode->GetParent()->RemoveChild(childNode);
                        } else {
                            childUniquePtr = Scene::SceneManager::Get().GetRootNode()->RemoveChild(childNode);
                        }
                        if (childUniquePtr) {
                            parentNode->AddChild(std::move(childUniquePtr));
                        }
                    }
                    it = m_pendingParents.erase(it);
                    continue;
                }
            }
        }
        ++it;
    }
}

void ReplicationManager::ProcessIncomingPacket(uint32_t peerId, PacketReader& reader) {
    if (reader.HasError()) return;

    PacketId packetId = reader.GetPacketId();
    if (packetId == PacketId::ReplicationSnapshot) {
        ClientProcessSnapshot(reader);
    } else if (packetId == PacketId::ReplicationAck) {
        uint32_t snapshotId = 0;
        if (reader.ReadUint32(snapshotId)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_clientStates.find(peerId);
            if (it != m_clientStates.end()) {
                it->second.lastAckedSnapshotId = snapshotId;
                it->second.hasReceivedInitialSnapshot = true;
            }
        }
    } else if (packetId == PacketId::InputSync) {
        uint32_t seq = 0;
        uint64_t ts = 0;
        glm::vec3 moveDir;
        float vertVel = 0.0f;
        bool reqJump = false;
        uint32_t buttons = 0;
        float dt = 0.016f;

        if (reader.ReadUint32(seq) &&
            reader.ReadUint64(ts) &&
            reader.ReadFloat(moveDir.x) &&
            reader.ReadFloat(moveDir.y) &&
            reader.ReadFloat(moveDir.z) &&
            reader.ReadFloat(vertVel) &&
            reader.ReadBool(reqJump) &&
            reader.ReadUint32(buttons) &&
            reader.ReadFloat(dt)) {

            // Replay/Spam protection check
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_clientStates.find(peerId);
                if (it != m_clientStates.end()) {
                    if (seq <= it->second.lastProcessedInputSequence) {
                        Core::Logger::Warning("Security", "Spam/Replay: Rejected out-of-order input packet (seq %u, last %u) from peer %u.",
                            seq, it->second.lastProcessedInputSequence, peerId);
                        return;
                    }
                }
            }

            ECS::Entity playerEnt = ECS::NULL_ENTITY;
            m_registry->Each<NetworkOwnershipComponent>([&](auto entity, const NetworkOwnershipComponent& own) {
                if (own.ownerPeerId == peerId) {
                    playerEnt = entity;
                }
            });

            if (playerEnt != ECS::NULL_ENTITY &&
                m_registry->HasComponent<Physics::PhysicsComponent>(playerEnt) &&
                m_registry->HasComponent<Physics::CharacterControllerComponent>(playerEnt)) {

                auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(playerEnt);
                auto& cc = m_registry->GetComponent<Physics::CharacterControllerComponent>(playerEnt);

                cc.moveDirection = moveDir;
                cc.requestJump = reqJump;

                Physics::PhysicsWorld world;
                Physics::CharacterController controller;
                controller.Update(m_registry, playerEnt, pc, cc, world, dt);
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_clientStates.find(peerId);
            if (it != m_clientStates.end()) {
                it->second.lastProcessedInputSequence = seq;
            }
        }
    } else if (packetId == PacketId::Ping) {
        TimeSyncManager::Get().ServerProcessPing(peerId, reader);
    } else if (packetId == PacketId::Pong) {
        TimeSyncManager::Get().ClientProcessPong(reader);
    }
}

} // namespace KumariEngine::Networking
