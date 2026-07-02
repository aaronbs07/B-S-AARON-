#pragma once
#include "ecs/ecs.hpp"
#include "scene/transform_component.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "networking/Packet.hpp"
#include "save/EntityGUID.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "gameplay/InventorySystem.hpp"
#include "gameplay/EquipmentSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/DialogueSystem.hpp"
#include "gameplay/InteractionSystem.hpp"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

namespace KumariEngine::Networking {

struct ClientReplicationState {
    uint32_t lastAckedSnapshotId = 0;
    bool hasReceivedInitialSnapshot = false;
    uint32_t lastProcessedInputSequence = 0;
    std::unordered_set<Save::EntityGUID> activeEntities;
};

struct ComponentStates {
    bool hasTransform = false;
    Scene::TransformComponent transform;

    bool hasPhysics = false;
    Physics::PhysicsComponent physics;

    bool hasCharacterController = false;
    Physics::CharacterControllerComponent characterController;

    bool hasScript = false;
    std::string scriptPath;

    bool hasHealth = false;
    Gameplay::HealthComponent health;

    bool hasTeam = false;
    Gameplay::TeamComponent team;

    bool hasPlayerState = false;
    Gameplay::PlayerStateComponent playerState;

    bool hasGameplayTags = false;
    Gameplay::GameplayTagsComponent gameplayTags;

    bool hasInventory = false;
    Gameplay::InventoryComponent inventory;

    bool hasItem = false;
    Gameplay::ItemComponent item;

    bool hasEquipment = false;
    Gameplay::EquipmentComponent equipment;

    bool hasQuest = false;
    Gameplay::QuestComponent quest;

    bool hasDialogue = false;
    Gameplay::DialogueComponent dialogue;

    bool hasInteractable = false;
    Gameplay::InteractableComponent interactable;

    bool hasTriggerVolume = false;
    Gameplay::TriggerVolumeComponent triggerVolume;
};

struct EntityState {
    Save::EntityGUID guid;
    Save::EntityGUID parentGuid; // Parent entity's GUID (null if none)
    bool isAlive = false;
    ComponentStates components;
};

struct Snapshot {
    uint32_t id = 0;
    double timestamp = 0.0; // Server timestamp in milliseconds
    std::unordered_map<Save::EntityGUID, EntityState> entities;
};

class ReplicationManager {
public:
    static ReplicationManager& Get() {
        static ReplicationManager instance;
        return instance;
    }

    ReplicationManager(const ReplicationManager&) = delete;
    ReplicationManager& operator=(const ReplicationManager&) = delete;

    // Lifecycle
    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Server replication tick
    void ServerUpdate();

    // Client snapshot processing
    void ClientProcessSnapshot(PacketReader& reader);

    // Client acknowledgment sender
    void ClientSendAck(uint32_t snapshotId);

    // Handle incoming packets
    void ProcessIncomingPacket(uint32_t peerId, PacketReader& reader);

    // Peer connect/disconnect notifications (Server side)
    void OnPeerConnected(uint32_t peerId);
    void OnPeerDisconnected(uint32_t peerId);

    // Registry access
    ECS::Registry* GetRegistry() const { return m_registry; }

    // Snapshot history & limits
    void SetMaxHistorySize(size_t size) { m_maxHistorySize = size; }
    size_t GetMaxHistorySize() const { return m_maxHistorySize; }

    // Access helper for tests
    uint32_t GetNextSnapshotId() const { return m_nextSnapshotId; }
    void SetNextSnapshotId(uint32_t id) { m_nextSnapshotId = id; }
    const std::map<uint32_t, Snapshot>& GetSnapshotHistory() const { return m_snapshotHistory; }
    const std::unordered_map<uint32_t, ClientReplicationState>& GetClientStates() const { return m_clientStates; }

    // Client tracked replicated GUIDs getter
    const std::unordered_set<Save::EntityGUID>& GetClientReplicatedGUIDs() const { return m_clientReplicatedGUIDs; }
    void RegisterClientReplicatedGUID(const Save::EntityGUID& guid) { m_clientReplicatedGUIDs.insert(guid); }

    // Interest Management Configuration
    using VisibilityCallback = std::function<bool(uint32_t peerId, ECS::Entity targetEntity)>;
    void SetVisibilityCallback(VisibilityCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_visibilityCallback = cb;
    }
    void SetInterestRange(float range) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interestRange = range;
    }
    float GetInterestRange() const { return m_interestRange; }

    // Performance profiling timings
    double GetLastReplicationTimeMs() const { return m_lastReplicationTimeMs.load(); }
    double GetLastSerializationTimeMs() const { return m_lastSerializationTimeMs.load(); }
    double GetLastDeserializationTimeMs() const { return m_lastDeserializationTimeMs.load(); }

public:
    ReplicationManager() = default;
    ~ReplicationManager() = default;

    // Helper: Capture snapshot of current ECS Registry state
    Snapshot CaptureCurrentSnapshot();

    // Helper: Serialize component data
    void SerializeComponent(PacketWriter& writer, const EntityState& entityState, uint32_t componentBitmask);
    
    // Helper: Deserialize component data
    bool DeserializeComponent(PacketReader& reader, ECS::Entity entity, uint32_t componentBitmask);

private:

    // Resolve pending parent-child relationships
    void ResolvePendingParents();

    ECS::Registry* m_registry = nullptr;
    std::mutex m_mutex;

    // Server state
    uint32_t m_nextSnapshotId = 1;
    size_t m_maxHistorySize = 128;
    std::map<uint32_t, Snapshot> m_snapshotHistory;
    std::unordered_map<uint32_t, ClientReplicationState> m_clientStates;

    // Client state
    std::unordered_set<Save::EntityGUID> m_clientReplicatedGUIDs;
    std::unordered_map<Save::EntityGUID, Save::EntityGUID> m_pendingParents; // childGUID -> parentGUID

    // Interest Management variables
    VisibilityCallback m_visibilityCallback = nullptr;
    float m_interestRange = 100.0f;

    // Profiling variables
    std::atomic<double> m_lastReplicationTimeMs{0.0};
    std::atomic<double> m_lastSerializationTimeMs{0.0};
    std::atomic<double> m_lastDeserializationTimeMs{0.0};
};

} // namespace KumariEngine::Networking
