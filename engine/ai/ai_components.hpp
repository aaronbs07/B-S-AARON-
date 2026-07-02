#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::AI {

// --- Blackboard Value & Component ---
using BlackboardValue = std::variant<int, float, bool, std::string, glm::vec3, ECS::Entity>;

struct BlackboardComponent {
    std::unordered_map<std::string, BlackboardValue> values;

    bool HasValue(const std::string& key) const {
        return values.find(key) != values.end();
    }

    template<typename T>
    T GetValue(const std::string& key, const T& defaultValue = T()) const {
        auto it = values.find(key);
        if (it != values.end()) {
            if (std::holds_alternative<T>(it->second)) {
                return std::get<T>(it->second);
            }
        }
        return defaultValue;
    }

    template<typename T>
    void SetValue(const std::string& key, const T& value) {
        values[key] = value;
    }

    void ClearValue(const std::string& key) {
        values.erase(key);
    }
};

// --- Perception System Components ---
struct PerceptionStimulus {
    ECS::Entity entity = ECS::NULL_ENTITY;
    glm::vec3 position{0.0f};
    float timeLastSeen = 0.0f;
    bool isVisible = false;
};

struct PerceptionComponent {
    float fieldOfView = 90.0f;    // field of view in degrees
    float visionRange = 25.0f;    // maximum visual range
    float hearingRange = 12.0f;   // auditory detection range
    
    std::vector<PerceptionStimulus> perceivedStimuli;
};

// --- Navigation Agent Component ---
struct NavigationAgentComponent {
    glm::vec3 agentTarget{0.0f};
    float agentSpeed = 3.5f;
    std::vector<glm::vec3> currentPath;
    size_t pathIndex = 0;
    
    float avoidanceRadius = 1.5f;
    bool useAvoidance = true;
    float acceptanceRadius = 0.5f;
};

// --- General AI Component ---
class BehaviorTree;

struct AIComponent {
    std::shared_ptr<BehaviorTree> behaviorTree;
    ECS::Entity controllerEntity = ECS::NULL_ENTITY;
    std::string currentState = "Idle";
};

} // namespace KumariEngine::AI
