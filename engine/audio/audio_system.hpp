#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::Audio {

enum class MixerChannel {
    Master,
    Music,
    SFX,
    Dialogue
};

struct AudioMixerChannelData {
    float volume = 1.0f;
    bool muted = false;
    bool bypassed = false;
};

class AudioMixer {
public:
    void SetVolume(MixerChannel channel, float volume);
    float GetVolume(MixerChannel channel) const;
    void SetMuted(MixerChannel channel, bool muted);
    bool IsMuted(MixerChannel channel) const;
    
private:
    std::unordered_map<MixerChannel, AudioMixerChannelData> m_channels = {
        { MixerChannel::Master, { 1.0f, false, false } },
        { MixerChannel::Music, { 1.0f, false, false } },
        { MixerChannel::SFX, { 1.0f, false, false } },
        { MixerChannel::Dialogue, { 1.0f, false, false } }
    };
};

struct AudioEvent {
    std::string name;
    std::string soundPath;
    float minVolume = 0.8f;
    float maxVolume = 1.0f;
    float minPitch = 0.9f;
    float maxPitch = 1.1f;
    MixerChannel channel = MixerChannel::SFX;
};

struct AudioSourceComponent {
    std::string eventOrPath;
    bool isPlaying = false;
    bool loop = false;
    bool spatial3D = true;
    float pitch = 1.0f;
    float volume = 1.0f;
    float pan = 0.0f; // -1.0f (left) to 1.0f (right)
    
    // Attenuation bounds
    float minDistance = 1.0f;
    float maxDistance = 20.0f;
    
    // Computed values during runtime tick
    float currentPitch = 1.0f;
    float currentVolume = 1.0f;
    float currentPan = 0.0f;
};

struct AudioListenerComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
};

class AudioSystem {
public:
    static AudioSystem& Get() {
        static AudioSystem instance;
        return instance;
    }
    
    void Update(ECS::Registry* registry, float deltaTime);
    void RegisterEvent(const AudioEvent& ev);
    const AudioEvent* GetEvent(const std::string& name) const;
    
    AudioMixer& GetMixer() { return m_mixer; }
    void ClearEvents() { m_events.clear(); }
    
    // Trigger playback
    void TriggerAudioEvent(ECS::Registry* registry, ECS::Entity sourceEntity, const std::string& eventName);

private:
    AudioSystem() : m_rng(1337) {}
    ~AudioSystem() = default;
    
    AudioMixer m_mixer;
    std::unordered_map<std::string, AudioEvent> m_events;
    std::mt19937 m_rng;
};

} // namespace KumariEngine::Audio
