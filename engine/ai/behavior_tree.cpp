#include "behavior_tree.hpp"
#include "scene/transform_component.hpp"
#include <glm/glm.hpp>
#include <cmath>

namespace KumariEngine::AI {

// --- Composite Nodes ---

BTState SelectorNode::Tick(ECS::Registry* registry, ECS::Entity entity) {
    while (m_currentChildIndex < m_children.size()) {
        BTState state = m_children[m_currentChildIndex]->Tick(registry, entity);
        if (state == BTState::Success) {
            m_currentChildIndex = 0;
            return BTState::Success;
        } else if (state == BTState::Running) {
            return BTState::Running;
        } else {
            m_currentChildIndex++;
        }
    }
    m_currentChildIndex = 0;
    return BTState::Failure;
}

BTState SequenceNode::Tick(ECS::Registry* registry, ECS::Entity entity) {
    while (m_currentChildIndex < m_children.size()) {
        BTState state = m_children[m_currentChildIndex]->Tick(registry, entity);
        if (state == BTState::Failure) {
            m_currentChildIndex = 0;
            return BTState::Failure;
        } else if (state == BTState::Running) {
            return BTState::Running;
        } else {
            m_currentChildIndex++;
        }
    }
    m_currentChildIndex = 0;
    return BTState::Success;
}

// --- Decorator Nodes ---

BTState InverterDecorator::Tick(ECS::Registry* registry, ECS::Entity entity) {
    if (!m_child) return BTState::Failure;
    BTState state = m_child->Tick(registry, entity);
    if (state == BTState::Success) return BTState::Failure;
    if (state == BTState::Failure) return BTState::Success;
    return BTState::Running;
}

BTState SucceederDecorator::Tick(ECS::Registry* registry, ECS::Entity entity) {
    if (!m_child) return BTState::Success;
    BTState state = m_child->Tick(registry, entity);
    if (state == BTState::Running) return BTState::Running;
    return BTState::Success;
}

BTState BlackboardConditionDecorator::Tick(ECS::Registry* registry, ECS::Entity entity) {
    if (!registry || !registry->HasComponent<BlackboardComponent>(entity)) {
        return BTState::Failure;
    }
    auto& bb = registry->GetComponent<BlackboardComponent>(entity);
    bool conditionMet = false;
    
    if (m_queryType == BlackboardQueryType::Exists) {
        conditionMet = bb.HasValue(m_key);
    } else if (m_queryType == BlackboardQueryType::IsTrue) {
        conditionMet = bb.HasValue(m_key) && bb.GetValue<bool>(m_key, false);
    } else if (m_queryType == BlackboardQueryType::IsFalse) {
        conditionMet = bb.HasValue(m_key) && !bb.GetValue<bool>(m_key, true);
    } else if (m_queryType == BlackboardQueryType::EqualInt) {
        conditionMet = bb.HasValue(m_key) && (bb.GetValue<int>(m_key, 0) == std::get<int>(m_compareValue));
    } else if (m_queryType == BlackboardQueryType::EqualFloat) {
        conditionMet = bb.HasValue(m_key) && (std::abs(bb.GetValue<float>(m_key, 0.0f) - std::get<float>(m_compareValue)) < 0.001f);
    }
    
    if (conditionMet && m_child) {
        return m_child->Tick(registry, entity);
    }
    return BTState::Failure;
}

// --- Service Nodes ---

BTState ServiceNode::Tick(ECS::Registry* registry, ECS::Entity entity) {
    float dt = 0.016f;
    if (registry && registry->HasComponent<BlackboardComponent>(entity)) {
        dt = registry->GetComponent<BlackboardComponent>(entity).GetValue<float>("DeltaTime", 0.016f);
    }
    m_timeSinceLastTick += dt;
    if (m_timeSinceLastTick >= m_interval) {
        TickService(registry, entity);
        m_timeSinceLastTick = 0.0f;
    }
    if (m_child) {
        return m_child->Tick(registry, entity);
    }
    return BTState::Failure;
}

void PerceptionServiceNode::TickService(ECS::Registry* registry, ECS::Entity entity) {
    if (!registry || !registry->HasComponent<PerceptionComponent>(entity)) return;
    auto& perception = registry->GetComponent<PerceptionComponent>(entity);
    
    if (!registry->HasComponent<Scene::TransformComponent>(entity)) return;
    const auto& myTc = registry->GetComponent<Scene::TransformComponent>(entity);
    
    perception.perceivedStimuli.clear();
    
    auto transforms = registry->View<Scene::TransformComponent>();
    for (ECS::Entity other : transforms) {
        if (other == entity) continue;
        const auto& otherTc = registry->GetComponent<Scene::TransformComponent>(other);
        
        float dist = glm::distance(myTc.position, otherTc.position);
        if (dist <= perception.visionRange) {
            glm::vec3 forward = myTc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 dirToOther = glm::normalize(otherTc.position - myTc.position);
            float angle = glm::acos(glm::clamp(glm::dot(forward, dirToOther), -1.0f, 1.0f));
            float fovRad = glm::radians(perception.fieldOfView * 0.5f);
            
            if (angle <= fovRad) {
                PerceptionStimulus stimulus;
                stimulus.entity = other;
                stimulus.position = otherTc.position;
                stimulus.timeLastSeen = 0.0f;
                stimulus.isVisible = true;
                perception.perceivedStimuli.push_back(stimulus);
            }
        }
    }
    
    if (registry->HasComponent<BlackboardComponent>(entity)) {
        auto& bb = registry->GetComponent<BlackboardComponent>(entity);
        ECS::Entity closestTarget = ECS::NULL_ENTITY;
        float minDist = 1e9f;
        for (const auto& stim : perception.perceivedStimuli) {
            float dist = glm::distance(myTc.position, stim.position);
            if (dist < minDist) {
                minDist = dist;
                closestTarget = stim.entity;
            }
        }
        if (closestTarget != ECS::NULL_ENTITY) {
            bb.SetValue(m_targetBBKey, closestTarget);
        } else {
            bb.ClearValue(m_targetBBKey);
        }
    }
}

// --- Task Nodes ---

BTState WaitTaskNode::Tick(ECS::Registry* registry, ECS::Entity entity) {
    float dt = 0.016f;
    if (registry && registry->HasComponent<BlackboardComponent>(entity)) {
        dt = registry->GetComponent<BlackboardComponent>(entity).GetValue<float>("DeltaTime", 0.016f);
    }
    m_elapsedTime += dt;
    if (m_elapsedTime >= m_duration) {
        m_elapsedTime = 0.0f;
        return BTState::Success;
    }
    return BTState::Running;
}

BTState MoveToTaskNode::Tick(ECS::Registry* registry, ECS::Entity entity) {
    if (!registry || !registry->HasComponent<BlackboardComponent>(entity)) {
        return BTState::Failure;
    }
    auto& bb = registry->GetComponent<BlackboardComponent>(entity);
    glm::vec3 targetPos{0.0f};
    bool hasTarget = false;
    
    if (bb.HasValue(m_targetPositionBBKey)) {
        auto val = bb.values.at(m_targetPositionBBKey);
        if (std::holds_alternative<glm::vec3>(val)) {
            targetPos = std::get<glm::vec3>(val);
            hasTarget = true;
        } else if (std::holds_alternative<ECS::Entity>(val)) {
            ECS::Entity targetEnt = std::get<ECS::Entity>(val);
            if (registry->IsAlive(targetEnt) && registry->HasComponent<Scene::TransformComponent>(targetEnt)) {
                targetPos = registry->GetComponent<Scene::TransformComponent>(targetEnt).position;
                hasTarget = true;
            }
        }
    }
    
    if (!hasTarget) return BTState::Failure;
    
    if (registry->HasComponent<NavigationAgentComponent>(entity)) {
        auto& agent = registry->GetComponent<NavigationAgentComponent>(entity);
        agent.agentTarget = targetPos;
        
        if (registry->HasComponent<Scene::TransformComponent>(entity)) {
            const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
            float dist = glm::distance(tc.position, targetPos);
            if (dist <= agent.acceptanceRadius) {
                return BTState::Success;
            }
        }
        return BTState::Running;
    }
    
    return BTState::Failure;
}

} // namespace KumariEngine::AI
