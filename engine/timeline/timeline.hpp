#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::Timeline {

enum class TrackType {
    Event,
    Animation,
    Audio,
    Camera
};

struct Keyframe {
    float time = 0.0f;
    std::string valueString;
    float valueFloat = 0.0f;
    glm::vec3 valueVec3 = glm::vec3(0.0f);
    glm::quat valueQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

struct Clip {
    std::string name;
    float startTime = 0.0f;
    float duration = 1.0f;
    std::vector<Keyframe> keyframes;
};

struct Track {
    std::string name;
    TrackType type = TrackType::Event;
    std::string targetEntityGuid; // Guid for stable serialization
    std::vector<Clip> clips;
    bool enabled = true;
};

struct TimelineAsset {
    std::string name;
    float duration = 5.0f;
    std::vector<Track> tracks;
};

class TimelineManager {
public:
    static TimelineManager& Get() {
        static TimelineManager instance;
        return instance;
    }

    void RegisterTimeline(const TimelineAsset& timeline);
    const TimelineAsset* GetTimeline(const std::string& name) const;
    void Clear();

private:
    TimelineManager() = default;
    ~TimelineManager() = default;

    std::unordered_map<std::string, TimelineAsset> m_timelines;
};

// ECS component for controlling cinematic timeline playback
struct CinematicPlayerComponent {
    std::string timelineName;
    bool isPlaying = false;
    bool isPaused = false;
    float currentTime = 0.0f;
    bool loop = false;
    float playbackSpeed = 1.0f;

    CinematicPlayerComponent() = default;
};

} // namespace KumariEngine::Timeline
