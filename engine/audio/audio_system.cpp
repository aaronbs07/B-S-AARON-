#include "audio_system.hpp"
#include "scene/transform_component.hpp"
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::Audio {

// --- AudioMixer implementation ---

void AudioMixer::SetVolume(MixerChannel channel, float volume) {
    m_channels[channel].volume = std::clamp(volume, 0.0f, 1.0f);
}

float AudioMixer::GetVolume(MixerChannel channel) const {
    auto it = m_channels.find(channel);
    if (it != m_channels.end()) {
        return it->second.muted ? 0.0f : it->second.volume;
    }
    return 0.0f;
}

void AudioMixer::SetMuted(MixerChannel channel, bool muted) {
    m_channels[channel].muted = muted;
}

bool AudioMixer::IsMuted(MixerChannel channel) const {
    auto it = m_channels.find(channel);
    if (it != m_channels.end()) {
        return it->second.muted;
    }
    return false;
}


// --- AudioSystem implementation ---

void AudioSystem::RegisterEvent(const AudioEvent& ev) {
    m_events[ev.name] = ev;
    Core::Logger::Info("Audio", "Registered Audio Event: %s (Sound: %s)", ev.name.c_str(), ev.soundPath.c_str());
}

const AudioEvent* AudioSystem::GetEvent(const std::string& name) const {
    auto it = m_events.find(name);
    if (it != m_events.end()) {
        return &it->second;
    }
    return nullptr;
}

void AudioSystem::TriggerAudioEvent(ECS::Registry* registry, ECS::Entity sourceEntity, const std::string& eventName) {
    if (!registry || !registry->HasComponent<AudioSourceComponent>(sourceEntity)) return;

    auto& src = registry->GetComponent<AudioSourceComponent>(sourceEntity);
    const AudioEvent* ev = GetEvent(eventName);

    if (ev) {
        src.eventOrPath = ev->soundPath;
        
        // Randomize pitch and volume
        std::uniform_real_distribution<float> volDist(ev->minVolume, ev->maxVolume);
        std::uniform_real_distribution<float> pitchDist(ev->minPitch, ev->maxPitch);
        
        src.currentVolume = volDist(m_rng) * src.volume;
        src.currentPitch = pitchDist(m_rng) * src.pitch;
        src.isPlaying = true;
        
        Core::Logger::Info("Audio", "Triggered Audio Event: %s (Volume: %.2f, Pitch: %.2f)", 
                           eventName.c_str(), src.currentVolume, src.currentPitch);
    } else {
        // Fallback to playing raw path if no event found
        src.eventOrPath = eventName;
        src.currentVolume = src.volume;
        src.currentPitch = src.pitch;
        src.isPlaying = true;
    }
}

void AudioSystem::Update(ECS::Registry* registry, float deltaTime) {
    (void)deltaTime;
    if (!registry) return;

    // 1. Locate active listener position and orientation
    glm::vec3 listenerPos(0.0f);
    glm::vec3 listenerForward(0.0f, 0.0f, -1.0f);
    glm::vec3 listenerUp(0.0f, 1.0f, 0.0f);
    bool hasListener = false;

    registry->Each<AudioListenerComponent>([&](ECS::Entity entity, AudioListenerComponent& listener) {
        if (registry->HasComponent<Scene::TransformComponent>(entity)) {
            const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
            listener.position = tc.position;
            listener.forward = tc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            listener.up = tc.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }
        listenerPos = listener.position;
        listenerForward = listener.forward;
        listenerUp = listener.up;
        hasListener = true;
    });

    // 2. Process all playing audio sources
    registry->Each<AudioSourceComponent, Scene::TransformComponent>([&](ECS::Entity entity, AudioSourceComponent& src, Scene::TransformComponent& tc) {
        (void)entity;
        if (!src.isPlaying) return;

        // Calculate 3D Spatial Attenuation and Panning
        float attenuation = 1.0f;
        float finalPan = src.pan;
        if (src.spatial3D && hasListener) {
            float dist = glm::distance(listenerPos, tc.position);
            if (dist <= src.minDistance) {
                attenuation = 1.0f;
            } else if (dist >= src.maxDistance) {
                attenuation = 0.0f;
            } else {
                // Linear Rolloff
                attenuation = 1.0f - (dist - src.minDistance) / (src.maxDistance - src.minDistance);
            }

            // 3D Panning: dot product of listener right and source vector
            glm::vec3 right = glm::cross(listenerForward, listenerUp);
            if (glm::length(right) > 0.001f) {
                right = glm::normalize(right);
            } else {
                right = glm::vec3(1.0f, 0.0f, 0.0f);
            }
            glm::vec3 toSource = tc.position - listenerPos;
            float toSourceLen = glm::length(toSource);
            if (toSourceLen > 0.001f) {
                glm::vec3 toSourceNorm = toSource / toSourceLen;
                finalPan = glm::dot(toSourceNorm, right);
            } else {
                finalPan = 0.0f;
            }
        }

        // Apply channel routing volumes from Mixer
        float masterVol = m_mixer.GetVolume(MixerChannel::Master);
        
        // Find channel routing (default to SFX)
        MixerChannel route = MixerChannel::SFX;
        const AudioEvent* ev = GetEvent(src.eventOrPath);
        if (ev) {
            route = ev->channel;
        }
        
        float channelVol = m_mixer.GetVolume(route);
        float finalVol = src.currentVolume * attenuation * channelVol * masterVol;
        
        // Update volume and pan on source
        src.volume = finalVol;
        src.currentPan = finalPan;
    });
}

} // namespace KumariEngine::Audio
