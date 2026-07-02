#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"
#include "networking/InputSync.hpp"
#include "networking/ReplicationManager.hpp"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace KumariEngine::Physics {
    class PhysicsWorld;
}

namespace KumariEngine::Networking {

struct PredictedState {
    uint32_t sequenceNumber = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    bool isGrounded = false;
    float verticalVelocity = 0.0f;
};

struct RemoteEntityState {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
};

struct ReplicatedSnapshot {
    uint32_t snapshotId = 0;
    double receiveTime = 0.0; // Synced network time when received
    std::unordered_map<Save::EntityGUID, RemoteEntityState> remoteEntities;
};

class ClientPredictionSystem {
public:
    static ClientPredictionSystem& Get() {
        static ClientPredictionSystem instance;
        return instance;
    }

    ClientPredictionSystem(const ClientPredictionSystem&) = delete;
    ClientPredictionSystem& operator=(const ClientPredictionSystem&) = delete;

    void Initialize(ECS::Registry* registry, Physics::PhysicsWorld* world);
    void Shutdown();

    // Set configuration
    void SetInterpolationDelayMs(double delayMs) { m_interpolationDelayMs = delayMs; }
    void SetMaxExtrapolationTimeMs(double maxTimeMs) { m_maxExtrapolationTimeMs = maxTimeMs; }
    void SetLocalPeerId(uint32_t peerId) { m_localPeerId = peerId; }

    // Client-side Update tick:
    // 1. Applies local input and runs client prediction
    // 2. Interpolates/extrapolates remote entities
    void Update(float dt, const InputFrame& input);

    // Call this right after receiving a server snapshot to execute server reconciliation
    void OnServerSnapshotReceived(uint32_t lastProcessedInputSequence, const Snapshot& serverSnapshot);

    // Testing / debugging accessors
    const std::vector<InputFrame>& GetInputBuffer() const { return m_inputBuffer; }
    const std::vector<PredictedState>& GetPredictedStateHistory() const { return m_predictedStateHistory; }
    const std::vector<ReplicatedSnapshot>& GetSnapshotHistory() const { return m_snapshotHistory; }

    // Helper: Find player entity owned by local client (ownerPeerId == m_localPeerId)
    ECS::Entity FindLocalPlayerEntity() const;

private:
    ClientPredictionSystem() = default;
    ~ClientPredictionSystem() = default;

    ECS::Registry* m_registry = nullptr;
    Physics::PhysicsWorld* m_physicsWorld = nullptr;
    mutable std::mutex m_mutex;

    uint32_t m_localPeerId = 1; // Default client peer ID is 1
    uint32_t m_nextSequenceNumber = 1;

    std::vector<InputFrame> m_inputBuffer;
    std::vector<PredictedState> m_predictedStateHistory;
    std::vector<ReplicatedSnapshot> m_snapshotHistory;

    double m_interpolationDelayMs = 100.0;    // 100ms interpolation buffer
    double m_maxExtrapolationTimeMs = 500.0;   // Clamp extrapolation to 500ms
};

} // namespace KumariEngine::Networking
