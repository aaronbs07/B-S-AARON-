#include "cinematic_system.hpp"
#include "timeline.hpp"
#include "audio/audio_system.hpp"
#include "camera/camera_component.hpp"
#include "camera/camera_manager.hpp"
#include "scripting/script_component.hpp"
#include "animation/animation_system.hpp"
#include "scene/transform_component.hpp"
#include "scripting/script_engine.hpp"
#include "core/event_manager.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace KumariEngine::Timeline {

static Save::EntityGUID ParseGuidString(const std::string& str) {
    Save::EntityGUID guid = Save::NULL_GUID;
    if (str.length() == 32) {
        try {
            guid.high = std::stoull(str.substr(0, 16), nullptr, 16);
            guid.low = std::stoull(str.substr(16, 16), nullptr, 16);
        } catch (...) {}
    }
    return guid;
}

static bool WasKeyframeHit(float keyTime, float previousTime, float currentTime, float duration, bool wrapped) {
    if (!wrapped) {
        return keyTime > previousTime && keyTime <= currentTime;
    } else {
        return (keyTime > previousTime && keyTime <= duration) || (keyTime >= 0.0f && keyTime <= currentTime);
    }
}

void CinematicSystem::Update(ECS::Registry* registry, float deltaTime) {
    if (!registry) return;

    registry->Each<CinematicPlayerComponent>([&](ECS::Entity entity, CinematicPlayerComponent& player) {
        if (!player.isPlaying || player.isPaused) return;

        const TimelineAsset* timeline = TimelineManager::Get().GetTimeline(player.timelineName);
        if (!timeline) {
            player.isPlaying = false;
            return;
        }

        float previousTime = player.currentTime;
        player.currentTime += deltaTime * player.playbackSpeed;

        bool wrapped = false;
        if (player.currentTime >= timeline->duration) {
            if (player.loop) {
                player.currentTime = std::fmod(player.currentTime, timeline->duration);
                wrapped = true;
            } else {
                player.currentTime = timeline->duration;
                player.isPlaying = false;
            }
        }

        // Process all tracks and keyframes
        ProcessTracks(registry, entity, previousTime, player.currentTime);

        if (wrapped && player.isPlaying) {
            // If looped, also process from 0 to current time
            // Already handled by WasKeyframeHit range check, but let's be sure
        }
    });
}

void CinematicSystem::Play(ECS::Registry* registry, ECS::Entity playerEntity) {
    if (registry && registry->HasComponent<CinematicPlayerComponent>(playerEntity)) {
        auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
        player.isPlaying = true;
        player.isPaused = false;
        player.currentTime = 0.0f;
        Core::Logger::Info("Cinematics", "Playing Cinematic: %s on Entity %u", player.timelineName.c_str(), playerEntity);
    }
}

void CinematicSystem::Pause(ECS::Registry* registry, ECS::Entity playerEntity) {
    if (registry && registry->HasComponent<CinematicPlayerComponent>(playerEntity)) {
        auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
        player.isPaused = true;
        Core::Logger::Info("Cinematics", "Paused Cinematic: %s on Entity %u", player.timelineName.c_str(), playerEntity);
    }
}

void CinematicSystem::Resume(ECS::Registry* registry, ECS::Entity playerEntity) {
    if (registry && registry->HasComponent<CinematicPlayerComponent>(playerEntity)) {
        auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
        player.isPaused = false;
        player.isPlaying = true;
        Core::Logger::Info("Cinematics", "Resumed Cinematic: %s on Entity %u", player.timelineName.c_str(), playerEntity);
    }
}

void CinematicSystem::Stop(ECS::Registry* registry, ECS::Entity playerEntity) {
    if (registry && registry->HasComponent<CinematicPlayerComponent>(playerEntity)) {
        auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
        player.isPlaying = false;
        player.isPaused = false;
        player.currentTime = 0.0f;
        Core::Logger::Info("Cinematics", "Stopped Cinematic: %s on Entity %u", player.timelineName.c_str(), playerEntity);
    }
}

void CinematicSystem::Skip(ECS::Registry* registry, ECS::Entity playerEntity) {
    if (registry && registry->HasComponent<CinematicPlayerComponent>(playerEntity)) {
        auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
        const TimelineAsset* timeline = TimelineManager::Get().GetTimeline(player.timelineName);
        if (timeline) {
            float prevTime = player.currentTime;
            player.currentTime = timeline->duration;
            player.isPlaying = false;
            player.isPaused = false;
            
            // Trigger all remaining events between current and end of timeline
            ProcessTracks(registry, playerEntity, prevTime, timeline->duration);
            Core::Logger::Info("Cinematics", "Skipped Cinematic: %s on Entity %u", player.timelineName.c_str(), playerEntity);
        }
    }
}

void CinematicSystem::ProcessTracks(ECS::Registry* registry, ECS::Entity playerEntity, float previousTime, float currentTime) {
    auto& player = registry->GetComponent<CinematicPlayerComponent>(playerEntity);
    const TimelineAsset* timeline = TimelineManager::Get().GetTimeline(player.timelineName);
    if (!timeline) return;

    bool wrapped = currentTime < previousTime;
    float duration = timeline->duration;

    for (const auto& track : timeline->tracks) {
        if (!track.enabled) continue;

        ECS::Entity targetEntity = ECS::NULL_ENTITY;
        if (!track.targetEntityGuid.empty()) {
            Save::EntityGUID targetGuid = ParseGuidString(track.targetEntityGuid);
            if (!targetGuid.IsNull()) {
                targetEntity = registry->GetEntityByGUID(targetGuid);
            }
        }

        // Process tracks differently by type
        if (track.type == TrackType::Camera) {
            // Camera Track: Interpolate transform and fov between bounding keyframes
            std::vector<Keyframe> trackKeyframes;
            for (const auto& clip : track.clips) {
                for (const auto& key : clip.keyframes) {
                    float absoluteTime = clip.startTime + key.time;
                    trackKeyframes.push_back({ absoluteTime, key.valueString, key.valueFloat, key.valueVec3, key.valueQuat });
                }
            }

            const Keyframe* prevKey = nullptr;
            const Keyframe* nextKey = nullptr;
            for (const auto& key : trackKeyframes) {
                if (key.time <= currentTime) {
                    if (!prevKey || key.time > prevKey->time) {
                        prevKey = &key;
                    }
                }
                if (key.time >= currentTime) {
                    if (!nextKey || key.time < nextKey->time) {
                        nextKey = &key;
                    }
                }
            }

            // Interpolate values
            if (prevKey && nextKey && targetEntity != ECS::NULL_ENTITY && registry->IsAlive(targetEntity)) {
                float diff = nextKey->time - prevKey->time;
                float t = (diff > 0.0001f) ? (currentTime - prevKey->time) / diff : 0.0f;
                glm::vec3 pos = glm::mix(prevKey->valueVec3, nextKey->valueVec3, t);
                glm::quat rot = glm::slerp(prevKey->valueQuat, nextKey->valueQuat, t);
                float fov = glm::mix(prevKey->valueFloat, nextKey->valueFloat, t);

                if (registry->HasComponent<Scene::TransformComponent>(targetEntity)) {
                    auto& tc = registry->GetComponent<Scene::TransformComponent>(targetEntity);
                    tc.position = pos;
                    tc.rotation = rot;
                }
                if (registry->HasComponent<Camera::CameraComponent>(targetEntity)) {
                    auto& cc = registry->GetComponent<Camera::CameraComponent>(targetEntity);
                    cc.fov = fov;
                }
            }

            // Trigger Camera sequencing/blending events
            for (const auto& key : trackKeyframes) {
                if (WasKeyframeHit(key.time, previousTime, currentTime, duration, wrapped)) {
                    if (!key.valueString.empty()) {
                        // Sequence trigger: blend to named camera
                        float blendDur = key.valueFloat > 0.0f ? key.valueFloat : 0.0f;
                        Camera::CameraManager::Get().BlendToCamera(key.valueString, blendDur);
                        Core::Logger::Info("Cinematics", "Timeline Camera Sequence: Blend to camera '%s' (duration: %.2f)",
                                           key.valueString.c_str(), blendDur);
                    }
                }
            }
        }
        else {
            // General track triggers (Event, Audio, Animation)
            for (const auto& clip : track.clips) {
                for (const auto& key : clip.keyframes) {
                    float absoluteTime = clip.startTime + key.time;
                    if (WasKeyframeHit(absoluteTime, previousTime, currentTime, duration, wrapped)) {
                        
                        if (track.type == TrackType::Audio && targetEntity != ECS::NULL_ENTITY && registry->IsAlive(targetEntity)) {
                            // Trigger AudioEvent on entity
                            Audio::AudioSystem::Get().TriggerAudioEvent(registry, targetEntity, key.valueString);
                        }
                        else if (track.type == TrackType::Animation && targetEntity != ECS::NULL_ENTITY && registry->IsAlive(targetEntity)) {
                            // Swap animation state
                            if (registry->HasComponent<Animation::AnimationComponent>(targetEntity)) {
                                auto& anim = registry->GetComponent<Animation::AnimationComponent>(targetEntity);
                                anim.stateMachine.currentState = key.valueString;
                                Core::Logger::Info("Cinematics", "Timeline Anim Track on Entity %u: State set to '%s'",
                                                   targetEntity, key.valueString.c_str());
                            }
                        }
                        else if (track.type == TrackType::Event) {
                            // 1. Dispatch custom EventManager event
                            Core::LuaCustomEvent customEvent("OnTimelineEvent", -1);
                            // We can push the custom event variables inside the event if desired, 
                            // or Lua can subscribe to this. Let's make sure script engine dispatches it too.
                            Core::EventManager::Get().DispatchEvent(customEvent);

                            // 2. Direct Lua script environment callback trigger
                            if (targetEntity != ECS::NULL_ENTITY && registry->IsAlive(targetEntity)) {
                                if (registry->HasComponent<ECS::ScriptComponent>(targetEntity)) {
                                    Scripting::ScriptEngine::Get().TriggerTimelineEvent(targetEntity, key.valueString, key.valueFloat);
                                }
                            }
                            Core::Logger::Info("Cinematics", "Timeline Event Track: Fired event '%s' (value: %.2f)",
                                               key.valueString.c_str(), key.valueFloat);
                        }
                    }
                }
            }
        }
    }
}

} // namespace KumariEngine::Timeline
