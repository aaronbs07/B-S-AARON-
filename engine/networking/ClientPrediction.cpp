#include "networking/ClientPrediction.hpp"
#include "networking/TimeSync.hpp"
#include "networking/NetworkManager.hpp"
#include "networking/NetworkOwnershipComponent.hpp"
#include "physics/character_controller.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <variant>

namespace KumariEngine::Networking {

void ClientPredictionSystem::Initialize(ECS::Registry* registry, Physics::PhysicsWorld* world) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = registry;
    m_physicsWorld = world;

    m_inputBuffer.clear();
    m_predictedStateHistory.clear();
    m_snapshotHistory.clear();
    m_nextSequenceNumber = 1;

    Core::Logger::Info("ClientPrediction", "ClientPredictionSystem initialized.");
}

void ClientPredictionSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = nullptr;
    m_physicsWorld = nullptr;
    m_inputBuffer.clear();
    m_predictedStateHistory.clear();
    m_snapshotHistory.clear();
}

ECS::Entity ClientPredictionSystem::FindLocalPlayerEntity() const {
    if (!m_registry) return ECS::NULL_ENTITY;

    ECS::Entity localPlayer = ECS::NULL_ENTITY;
    m_registry->Each<NetworkOwnershipComponent>([&](auto entity, const NetworkOwnershipComponent& own) {
        if (own.ownerPeerId == m_localPeerId) {
            localPlayer = entity;
        }
    });
    return localPlayer;
}

void ClientPredictionSystem::Update(float dt, const InputFrame& input) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_registry || !m_physicsWorld) return;

    // 1. Client Prediction for local player
    ECS::Entity localPlayer = FindLocalPlayerEntity();
    if (localPlayer != ECS::NULL_ENTITY) {
        InputFrame frame = input;
        frame.sequenceNumber = m_nextSequenceNumber++;
        frame.timestamp = static_cast<uint64_t>(TimeSyncManager::Get().GetSyncedServerTimeMs());
        frame.deltaTime = dt;

        m_inputBuffer.push_back(frame);

        // Apply input immediately to character controller components
        auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(localPlayer);
        auto& cc = m_registry->GetComponent<Physics::CharacterControllerComponent>(localPlayer);

        cc.moveDirection = frame.moveDirection;
        cc.requestJump = frame.requestJump;

        // Run Character Controller local simulation step
        Physics::CharacterController controller;
        controller.Update(m_registry, localPlayer, pc, cc, *m_physicsWorld, dt);

        // Record predicted state
        auto* node = Scene::SceneManager::Get().GetNodeByEntity(localPlayer);
        glm::vec3 pos = node ? node->GetLocalPosition() : glm::vec3(0.0f);
        glm::quat rot = node ? node->GetLocalRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        PredictedState pred;
        pred.sequenceNumber = frame.sequenceNumber;
        pred.position = pos;
        pred.rotation = rot;
        pred.velocity = pc.velocity;
        pred.isGrounded = cc.isGrounded;
        pred.verticalVelocity = cc.verticalVelocity;

        m_predictedStateHistory.push_back(pred);

        // Send input packet to server
        PacketWriter writer(PacketId::InputSync);
        writer.WriteUint32(frame.sequenceNumber);
        writer.WriteUint64(frame.timestamp);
        writer.WriteFloat(frame.moveDirection.x);
        writer.WriteFloat(frame.moveDirection.y);
        writer.WriteFloat(frame.moveDirection.z);
        writer.WriteFloat(frame.verticalVelocity);
        writer.WriteBool(frame.requestJump);
        writer.WriteUint32(frame.actionButtonsBitmask);
        writer.WriteFloat(frame.deltaTime);

        NetworkManager::Get().Send(1, writer, false);
    }

    // 2. Entity Interpolation & Extrapolation for remote entities
    double renderTime = TimeSyncManager::Get().GetSyncedServerTimeMs() - m_interpolationDelayMs;

    // Iterate all replicated remote entities
    const auto& replicatedGUIDs = ReplicationManager::Get().GetClientReplicatedGUIDs();
    for (const auto& guid : replicatedGUIDs) {
        ECS::Entity entity = m_registry->GetEntityByGUID(guid);
        if (entity == ECS::NULL_ENTITY || entity == localPlayer) continue;

        // Find position / rotation / velocity via snapshot history
        if (m_snapshotHistory.empty()) continue;

        bool interpolated = false;

        // Find A and B snapshots bracketing renderTime
        size_t indexA = m_snapshotHistory.size();
        size_t indexB = m_snapshotHistory.size();

        for (size_t i = 0; i < m_snapshotHistory.size(); ++i) {
            if (m_snapshotHistory[i].receiveTime <= renderTime) {
                indexA = i;
            }
            if (m_snapshotHistory[i].receiveTime > renderTime && indexB == m_snapshotHistory.size()) {
                indexB = i;
                break;
            }
        }

        if (indexA < m_snapshotHistory.size() && indexB < m_snapshotHistory.size()) {
            const auto& snapA = m_snapshotHistory[indexA];
            const auto& snapB = m_snapshotHistory[indexB];

            auto itA = snapA.remoteEntities.find(guid);
            auto itB = snapB.remoteEntities.find(guid);

            if (itA != snapA.remoteEntities.end() && itB != snapB.remoteEntities.end()) {
                double duration = snapB.receiveTime - snapA.receiveTime;
                double t = (duration > 1e-4) ? (renderTime - snapA.receiveTime) / duration : 0.0;

                glm::vec3 interpPos = glm::mix(itA->second.position, itB->second.position, static_cast<float>(t));
                glm::quat interpRot = glm::slerp(itA->second.rotation, itB->second.rotation, static_cast<float>(t));
                glm::vec3 interpVel = glm::mix(itA->second.velocity, itB->second.velocity, static_cast<float>(t));

                auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                if (node) {
                    node->SetLocalPosition(interpPos);
                    node->SetLocalRotation(interpRot);
                    node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                }
                if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
                    auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                    tc.position = interpPos;
                    tc.rotation = interpRot;
                }
                if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) {
                    auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(entity);
                    pc.velocity = interpVel;
                }
                interpolated = true;
            }
        }

        // Extrapolate if renderTime is past the latest snapshot
        if (!interpolated && m_snapshotHistory.size() >= 2) {
            const auto& latestSnap = m_snapshotHistory.back();
            const auto& prevSnap = m_snapshotHistory[m_snapshotHistory.size() - 2];

            auto itL = latestSnap.remoteEntities.find(guid);
            auto itP = prevSnap.remoteEntities.find(guid);

            if (itL != latestSnap.remoteEntities.end()) {
                double elapsed = renderTime - latestSnap.receiveTime;
                if (elapsed < 0.0) elapsed = 0.0;

                // Clamp extrapolation duration
                if (elapsed > m_maxExtrapolationTimeMs) {
                    elapsed = m_maxExtrapolationTimeMs;
                }

                glm::vec3 velocity = itL->second.velocity;
                if (itP != prevSnap.remoteEntities.end()) {
                    double dtSnap = latestSnap.receiveTime - prevSnap.receiveTime;
                    if (dtSnap > 1e-4) {
                        velocity = (itL->second.position - itP->second.position) / static_cast<float>(dtSnap / 1000.0);
                    }
                }

                glm::vec3 extrapPos = itL->second.position + velocity * static_cast<float>(elapsed / 1000.0);
                glm::quat extrapRot = itL->second.rotation;

                auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                if (node) {
                    node->SetLocalPosition(extrapPos);
                    node->SetLocalRotation(extrapRot);
                    node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                }
                if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
                    auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                    tc.position = extrapPos;
                    tc.rotation = extrapRot;
                }
                if (m_registry->HasComponent<Physics::PhysicsComponent>(entity)) {
                    auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(entity);
                    pc.velocity = velocity;
                }
            }
        }
    }
}

void ClientPredictionSystem::OnServerSnapshotReceived(uint32_t lastProcessedInputSequence, const Snapshot& serverSnapshot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_registry || !m_physicsWorld) return;

    // 1. Buffer snapshot remote entity states for interpolation
    ReplicatedSnapshot snapEntry;
    snapEntry.snapshotId = serverSnapshot.id;
    snapEntry.receiveTime = TimeSyncManager::Get().GetSyncedServerTimeMs();

    ECS::Entity localPlayer = FindLocalPlayerEntity();

    for (const auto& [guid, entityState] : serverSnapshot.entities) {
        ECS::Entity ent = m_registry->GetEntityByGUID(guid);
        if (ent != ECS::NULL_ENTITY && ent == localPlayer) continue;

        if (entityState.components.hasTransform) {
            RemoteEntityState res;
            res.position = entityState.components.transform.position;
            res.rotation = entityState.components.transform.rotation;
            if (entityState.components.hasPhysics) {
                res.velocity = entityState.components.physics.velocity;
            }
            snapEntry.remoteEntities[guid] = res;
        }
    }
    m_snapshotHistory.push_back(snapEntry);

    // Limit buffer size to 120 snapshots (2 seconds at 60fps)
    if (m_snapshotHistory.size() > 120) {
        m_snapshotHistory.erase(m_snapshotHistory.begin());
    }

    // 2. Authoritative Client Reconciliation for the local player
    if (localPlayer != ECS::NULL_ENTITY) {
        Save::EntityGUID localGUID = m_registry->GetGUID(localPlayer);
        auto it = serverSnapshot.entities.find(localGUID);
        if (it != serverSnapshot.entities.end()) {
            const auto& serverEntityState = it->second;
            glm::vec3 serverPos = serverEntityState.components.transform.position;
            glm::quat serverRot = serverEntityState.components.transform.rotation;
            glm::vec3 serverVel = serverEntityState.components.hasPhysics ? serverEntityState.components.physics.velocity : glm::vec3(0.0f);
            bool serverGrounded = serverEntityState.components.hasCharacterController ? serverEntityState.components.characterController.isGrounded : false;
            float serverVertVel = serverEntityState.components.hasCharacterController ? serverEntityState.components.characterController.verticalVelocity : 0.0f;

            // Find matching prediction in history
            auto predIt = std::find_if(m_predictedStateHistory.begin(), m_predictedStateHistory.end(),
                [lastProcessedInputSequence](const PredictedState& p) {
                    return p.sequenceNumber == lastProcessedInputSequence;
                });

            if (predIt != m_predictedStateHistory.end()) {
                // Mismatch detection
                float error = glm::distance(predIt->position, serverPos);
                if (error > 0.001f) {
                    Core::Logger::Warning("ClientPrediction", "Prediction mismatch at sequence %u! Error: %.4f. Reconciling...", lastProcessedInputSequence, error);

                    // Correct local state
                    auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(localPlayer);
                    auto& cc = m_registry->GetComponent<Physics::CharacterControllerComponent>(localPlayer);

                    pc.velocity = serverVel;
                    cc.isGrounded = serverGrounded;
                    cc.verticalVelocity = serverVertVel;

                    auto* node = Scene::SceneManager::Get().GetNodeByEntity(localPlayer);
                    if (node) {
                        node->SetLocalPosition(serverPos);
                        node->SetLocalRotation(serverRot);
                        node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                    }
                    if (m_registry->HasComponent<Scene::TransformComponent>(localPlayer)) {
                        auto& tc = m_registry->GetComponent<Scene::TransformComponent>(localPlayer);
                        tc.position = serverPos;
                        tc.rotation = serverRot;
                    }

                    // Erase acknowledged elements from prediction history
                    m_predictedStateHistory.erase(m_predictedStateHistory.begin(), predIt + 1);
                    
                    // Erase acknowledged inputs from input buffer
                    auto inputIt = std::find_if(m_inputBuffer.begin(), m_inputBuffer.end(),
                        [lastProcessedInputSequence](const InputFrame& f) {
                            return f.sequenceNumber == lastProcessedInputSequence;
                        });
                    if (inputIt != m_inputBuffer.end()) {
                        m_inputBuffer.erase(m_inputBuffer.begin(), inputIt + 1);
                    }

                    // Replay unacknowledged inputs
                    Physics::CharacterController controller;
                    std::vector<PredictedState> newHistory;
                    for (auto& frame : m_inputBuffer) {
                        cc.moveDirection = frame.moveDirection;
                        cc.requestJump = frame.requestJump;

                        controller.Update(m_registry, localPlayer, pc, cc, *m_physicsWorld, frame.deltaTime);

                        // Capture new prediction
                        glm::vec3 replayedPos = node ? node->GetLocalPosition() : glm::vec3(0.0f);
                        glm::quat replayedRot = node ? node->GetLocalRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

                        PredictedState newPred;
                        newPred.sequenceNumber = frame.sequenceNumber;
                        newPred.position = replayedPos;
                        newPred.rotation = replayedRot;
                        newPred.velocity = pc.velocity;
                        newPred.isGrounded = cc.isGrounded;
                        newPred.verticalVelocity = cc.verticalVelocity;
                        newHistory.push_back(newPred);
                    }
                    m_predictedStateHistory = std::move(newHistory);
                } else {
                    // Match! Discard inputs and prediction history up to acknowledged sequence
                    m_predictedStateHistory.erase(m_predictedStateHistory.begin(), predIt + 1);

                    auto inputIt = std::find_if(m_inputBuffer.begin(), m_inputBuffer.end(),
                        [lastProcessedInputSequence](const InputFrame& f) {
                            return f.sequenceNumber == lastProcessedInputSequence;
                        });
                    if (inputIt != m_inputBuffer.end()) {
                        m_inputBuffer.erase(m_inputBuffer.begin(), inputIt + 1);
                    }

                    // Restore latest predicted position to avoid snap back to old server state
                    if (!m_predictedStateHistory.empty()) {
                        const auto& latestPred = m_predictedStateHistory.back();
                        auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(localPlayer);
                        auto& cc = m_registry->GetComponent<Physics::CharacterControllerComponent>(localPlayer);

                        pc.velocity = latestPred.velocity;
                        cc.isGrounded = latestPred.isGrounded;
                        cc.verticalVelocity = latestPred.verticalVelocity;

                        auto* node = Scene::SceneManager::Get().GetNodeByEntity(localPlayer);
                        if (node) {
                            node->SetLocalPosition(latestPred.position);
                            node->SetLocalRotation(latestPred.rotation);
                            node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                        }
                        if (m_registry->HasComponent<Scene::TransformComponent>(localPlayer)) {
                            auto& tc = m_registry->GetComponent<Scene::TransformComponent>(localPlayer);
                            tc.position = latestPred.position;
                            tc.rotation = latestPred.rotation;
                        }
                    }
                }
            } else {
                // If prediction history does not exist, snap directly to server state
                m_inputBuffer.clear();
                m_predictedStateHistory.clear();

                auto& pc = m_registry->GetComponent<Physics::PhysicsComponent>(localPlayer);
                auto& cc = m_registry->GetComponent<Physics::CharacterControllerComponent>(localPlayer);

                pc.velocity = serverVel;
                cc.isGrounded = serverGrounded;
                cc.verticalVelocity = serverVertVel;

                auto* node = Scene::SceneManager::Get().GetNodeByEntity(localPlayer);
                if (node) {
                    node->SetLocalPosition(serverPos);
                    node->SetLocalRotation(serverRot);
                    node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                }
                if (m_registry->HasComponent<Scene::TransformComponent>(localPlayer)) {
                    auto& tc = m_registry->GetComponent<Scene::TransformComponent>(localPlayer);
                    tc.position = serverPos;
                    tc.rotation = serverRot;
                }
            }
        }
    }
}

} // namespace KumariEngine::Networking
