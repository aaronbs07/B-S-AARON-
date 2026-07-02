#include "networking/LagCompensation.hpp"
#include "networking/ReplicationManager.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Networking {

void LagCompensator::Initialize(ECS::Registry* registry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = registry;
    m_originalStates.clear();
    Core::Logger::Info("LagCompensation", "LagCompensator initialized.");
}

void LagCompensator::Shutdown() {
    Restore();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = nullptr;
}

void LagCompensator::Rewind(double targetTimeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_registry) return;

    // Ensure we are in a clean state before rewinding
    if (!m_originalStates.empty()) {
        Core::Logger::Warning("LagCompensation", "Rewind called while already in a rewound state. Restoring first.");
        // We inline restore logic here to avoid deadlock
        for (const auto& [guid, orig] : m_originalStates) {
            ECS::Entity entity = m_registry->GetEntityByGUID(guid);
            if (entity != ECS::NULL_ENTITY) {
                if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
                    auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                    tc.position = orig.position;
                    tc.rotation = orig.rotation;
                }
                auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                if (node) {
                    node->SetLocalPosition(orig.position);
                    node->SetLocalRotation(orig.rotation);
                    node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                }
            }
        }
        m_originalStates.clear();
    }

    const auto& history = ReplicationManager::Get().GetSnapshotHistory();
    if (history.empty()) {
        Core::Logger::Warning("LagCompensation", "Snapshot history is empty, cannot perform lag compensation rewind.");
        return;
    }

    auto itA = history.end();
    auto itB = history.end();

    // Search for bracketing snapshots
    for (auto it = history.begin(); it != history.end(); ++it) {
        if (it->second.timestamp <= targetTimeMs) {
            itA = it;
        }
        if (it->second.timestamp > targetTimeMs && itB == history.end()) {
            itB = it;
            break;
        }
    }

    if (itA != history.end() && itB != history.end()) {
        const auto& snapA = itA->second;
        const auto& snapB = itB->second;
        double duration = snapB.timestamp - snapA.timestamp;
        double t = (duration > 1e-4) ? (targetTimeMs - snapA.timestamp) / duration : 0.0;

        for (const auto& [guid, entStateA] : snapA.entities) {
            auto itB_ent = snapB.entities.find(guid);
            if (itB_ent != snapB.entities.end()) {
                const auto& entStateB = itB_ent->second;
                if (entStateA.components.hasTransform && entStateB.components.hasTransform) {
                    ECS::Entity entity = m_registry->GetEntityByGUID(guid);
                    if (entity != ECS::NULL_ENTITY) {
                        // Save current state
                        SavedEntityState orig;
                        auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                        orig.position = node ? node->GetLocalPosition() : m_registry->GetComponent<Scene::TransformComponent>(entity).position;
                        orig.rotation = node ? node->GetLocalRotation() : m_registry->GetComponent<Scene::TransformComponent>(entity).rotation;
                        m_originalStates[guid] = orig;

                        // Interpolate position and rotation
                        glm::vec3 interpPos = glm::mix(entStateA.components.transform.position, entStateB.components.transform.position, static_cast<float>(t));
                        glm::quat interpRot = glm::slerp(entStateA.components.transform.rotation, entStateB.components.transform.rotation, static_cast<float>(t));

                        // Apply interpolated state
                        auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                        tc.position = interpPos;
                        tc.rotation = interpRot;

                        if (node) {
                            node->SetLocalPosition(interpPos);
                            node->SetLocalRotation(interpRot);
                            node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                        }
                    }
                }
            }
        }
    } else {
        // Fallback: Snap to the single closest snapshot in history
        auto closestIt = history.end();
        double minDiff = -1.0;
        for (auto it = history.begin(); it != history.end(); ++it) {
            double diff = std::abs(it->second.timestamp - targetTimeMs);
            if (minDiff < 0.0 || diff < minDiff) {
                minDiff = diff;
                closestIt = it;
            }
        }

        if (closestIt != history.end()) {
            const auto& snap = closestIt->second;
            for (const auto& [guid, entState] : snap.entities) {
                if (entState.components.hasTransform) {
                    ECS::Entity entity = m_registry->GetEntityByGUID(guid);
                    if (entity != ECS::NULL_ENTITY) {
                        SavedEntityState orig;
                        auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
                        orig.position = node ? node->GetLocalPosition() : m_registry->GetComponent<Scene::TransformComponent>(entity).position;
                        orig.rotation = node ? node->GetLocalRotation() : m_registry->GetComponent<Scene::TransformComponent>(entity).rotation;
                        m_originalStates[guid] = orig;

                        auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                        tc.position = entState.components.transform.position;
                        tc.rotation = entState.components.transform.rotation;

                        if (node) {
                            node->SetLocalPosition(entState.components.transform.position);
                            node->SetLocalRotation(entState.components.transform.rotation);
                            node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
                        }
                    }
                }
            }
        }
    }
}

void LagCompensator::Restore() {
    // If not rewinding, this block might be called outside of Rewind lock, so lock here
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_registry || m_originalStates.empty()) return;

    for (const auto& [guid, orig] : m_originalStates) {
        ECS::Entity entity = m_registry->GetEntityByGUID(guid);
        if (entity != ECS::NULL_ENTITY) {
            if (m_registry->HasComponent<Scene::TransformComponent>(entity)) {
                auto& tc = m_registry->GetComponent<Scene::TransformComponent>(entity);
                tc.position = orig.position;
                tc.rotation = orig.rotation;
            }
            auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
            if (node) {
                node->SetLocalPosition(orig.position);
                node->SetLocalRotation(orig.rotation);
                node->UpdateTransforms(node->GetParent() ? node->GetParent()->GetWorldMatrix() : glm::mat4(1.0f));
            }
        }
    }
    m_originalStates.clear();
}

} // namespace KumariEngine::Networking
