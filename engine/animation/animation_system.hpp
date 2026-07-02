#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include "ecs/ecs.hpp"

namespace KumariEngine::Animation {

struct AnimationEvent {
    float time = 0.0f;
    std::string name;
};

struct AnimationClip {
    std::string name;
    float duration = 1.0f;
    std::vector<AnimationEvent> events;
};

struct AnimationTransition {
    std::string toState;
    std::string conditionParam;
    bool conditionValue = true;
    float blendDuration = 0.2f;
};

struct AnimationState {
    std::string name;
    std::string clipName;
    float speed = 1.0f;
    bool loop = true;
    std::vector<AnimationTransition> transitions;
};

struct BlendTree {
    std::string parameterName;
    std::vector<std::string> clipNames;
    float currentBlendFactor = 0.0f; // range 0..1
};

struct AnimationStateMachine {
    std::unordered_map<std::string, AnimationState> states;
    std::string currentState;
    std::unordered_map<std::string, bool> boolParameters;
    std::unordered_map<std::string, float> floatParameters;
    
    // Blend/Transition parameters
    std::string transitionToState;
    float transitionTime = 0.0f;
    float transitionDuration = 0.0f;
};

struct AnimationComponent {
    AnimationStateMachine stateMachine;
    BlendTree blendTree;
    float currentTime = 0.0f;
    bool isPlaying = true;
    float speed = 1.0f;
    
    // For tracking which events were fired in the current loop
    float lastTime = 0.0f;
    
    std::function<void(const std::string&)> onEventTriggered;
};

class AnimationSystem {
public:
    static AnimationSystem& Get() {
        static AnimationSystem instance;
        return instance;
    }
    
    void Update(ECS::Registry* registry, float deltaTime);
    void RegisterClip(const std::string& name, const AnimationClip& clip);
    const AnimationClip* GetClip(const std::string& name) const;
    void ClearClips();
    
private:
    AnimationSystem() = default;
    ~AnimationSystem() = default;
    
    std::unordered_map<std::string, AnimationClip> m_clips;
};

} // namespace KumariEngine::Animation
