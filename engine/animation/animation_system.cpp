#include "animation_system.hpp"
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::Animation {

void AnimationSystem::Update(ECS::Registry* registry, float deltaTime) {
    if (!registry) return;

    registry->Each<AnimationComponent>([&](ECS::Entity entity, AnimationComponent& anim) {
        (void)entity;
        if (!anim.isPlaying) return;

        AnimationStateMachine& sm = anim.stateMachine;
        if (sm.currentState.empty() || sm.states.find(sm.currentState) == sm.states.end()) return;

        const AnimationState& state = sm.states[sm.currentState];

        // 1. Process transitions
        bool transitionStarted = false;
        for (const auto& trans : state.transitions) {
            auto boolIt = sm.boolParameters.find(trans.conditionParam);
            if (boolIt != sm.boolParameters.end() && boolIt->second == trans.conditionValue) {
                // Trigger transition
                sm.transitionToState = trans.toState;
                sm.transitionDuration = trans.blendDuration;
                sm.transitionTime = 0.0f;
                transitionStarted = true;
                break;
            }
        }

        // 2. Handle blending/active transition update
        if (!sm.transitionToState.empty()) {
            sm.transitionTime += deltaTime;
            if (sm.transitionTime >= sm.transitionDuration) {
                sm.currentState = sm.transitionToState;
                sm.transitionToState.clear();
                anim.currentTime = 0.0f;
                anim.lastTime = 0.0f;
            }
        }

        // 3. Update play time
        float prevTime = anim.currentTime;
        float stateSpeed = state.speed * anim.speed;
        anim.currentTime += deltaTime * stateSpeed;

        // Retrieve current clip/blend information
        const AnimationClip* clip = nullptr;
        if (!anim.blendTree.clipNames.empty()) {
            // Update blend tree factor
            auto floatIt = sm.floatParameters.find(anim.blendTree.parameterName);
            if (floatIt != sm.floatParameters.end()) {
                anim.blendTree.currentBlendFactor = std::clamp(floatIt->second, 0.0f, 1.0f);
            }
            // Use primary active clip based on blend factor
            size_t clipIndex = anim.blendTree.currentBlendFactor >= 0.5f ? 1 : 0;
            if (clipIndex < anim.blendTree.clipNames.size()) {
                clip = GetClip(anim.blendTree.clipNames[clipIndex]);
            }
        } else {
            clip = GetClip(state.clipName);
        }

        if (clip) {
            float duration = clip->duration;
            if (duration > 0.0f) {
                // Loop behavior
                if (anim.currentTime >= duration) {
                    if (state.loop) {
                        anim.currentTime = std::fmod(anim.currentTime, duration);
                    } else {
                        anim.currentTime = duration;
                    }
                }

                // 4. Trigger events
                if (anim.onEventTriggered) {
                    for (const auto& ev : clip->events) {
                        bool trigger = false;
                        if (anim.currentTime >= prevTime) {
                            if (ev.time >= prevTime && ev.time <= anim.currentTime) {
                                trigger = true;
                            }
                        } else {
                            // Wrapped around
                            if (ev.time >= prevTime || ev.time <= anim.currentTime) {
                                trigger = true;
                            }
                        }
                        if (trigger) {
                            anim.onEventTriggered(ev.name);
                        }
                    }
                }
            }
        }

        anim.lastTime = anim.currentTime;
    });
}

void AnimationSystem::RegisterClip(const std::string& name, const AnimationClip& clip) {
    m_clips[name] = clip;
    Core::Logger::Info("Animation", "Registered Animation Clip: %s (Duration: %.2f)", name.c_str(), clip.duration);
}

const AnimationClip* AnimationSystem::GetClip(const std::string& name) const {
    auto it = m_clips.find(name);
    if (it != m_clips.end()) {
        return &it->second;
    }
    return nullptr;
}

void AnimationSystem::ClearClips() {
    m_clips.clear();
}

} // namespace KumariEngine::Animation
