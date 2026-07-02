#include "timeline.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Timeline {

void TimelineManager::RegisterTimeline(const TimelineAsset& timeline) {
    m_timelines[timeline.name] = timeline;
    Core::Logger::Info("Timeline", "Registered Timeline: %s (Duration: %.2fs, Tracks: %zu)",
                       timeline.name.c_str(), timeline.duration, timeline.tracks.size());
}

const TimelineAsset* TimelineManager::GetTimeline(const std::string& name) const {
    auto it = m_timelines.find(name);
    if (it != m_timelines.end()) {
        return &it->second;
    }
    return nullptr;
}

void TimelineManager::Clear() {
    m_timelines.clear();
}

} // namespace KumariEngine::Timeline
