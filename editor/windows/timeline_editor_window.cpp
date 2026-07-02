#include "timeline_editor_window.hpp"
#include "timeline/timeline.hpp"
#include "editor/selection_system.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void TimelineEditorWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Timeline Editor ---");
    ECS::Entity selected = SelectionSystem::Get().GetSelected();
    auto* registry = WindowSystem::Get().GetRegistry();

    if (selected != ECS::NULL_ENTITY && registry && registry->IsAlive(selected)) {
        if (registry->HasComponent<Timeline::CinematicPlayerComponent>(selected)) {
            const auto& player = registry->GetComponent<Timeline::CinematicPlayerComponent>(selected);
            Core::Logger::Info("EditorUI", "  Selected Entity: %u | Timeline: '%s'", selected, player.timelineName.c_str());
            Core::Logger::Info("EditorUI", "  Time: %.2fs | Playing: %s | Loop: %s",
                               player.currentTime, player.isPlaying ? "Yes" : "No", player.loop ? "Yes" : "No");

            const auto* timeline = Timeline::TimelineManager::Get().GetTimeline(player.timelineName);
            if (timeline) {
                Core::Logger::Info("EditorUI", "  Timeline Duration: %.2fs", timeline->duration);
                Core::Logger::Info("EditorUI", "  Tracks: %zu", timeline->tracks.size());
                for (const auto& track : timeline->tracks) {
                    std::string typeStr = "Event";
                    if (track.type == Timeline::TrackType::Camera) typeStr = "Camera";
                    else if (track.type == Timeline::TrackType::Audio) typeStr = "Audio";
                    else if (track.type == Timeline::TrackType::Animation) typeStr = "Animation";

                    Core::Logger::Info("EditorUI", "    - Track: %s (%s) | Target GUID: %s",
                                       track.name.c_str(), typeStr.c_str(), track.targetEntityGuid.c_str());
                    for (const auto& clip : track.clips) {
                        Core::Logger::Info("EditorUI", "      * Clip: %s (Start: %.2fs, Duration: %.2fs, Keyframes: %zu)",
                                           clip.name.c_str(), clip.startTime, clip.duration, clip.keyframes.size());
                        for (const auto& key : clip.keyframes) {
                            Core::Logger::Info("EditorUI", "        + Keyframe: Time=%.2fs | Value='%s' | Float=%.2f",
                                               key.time, key.valueString.c_str(), key.valueFloat);
                        }
                    }
                }
            } else {
                Core::Logger::Info("EditorUI", "  [Warning] Timeline asset '%s' not registered in TimelineManager.",
                                   player.timelineName.c_str());
            }
        } else {
            Core::Logger::Info("EditorUI", "  Selected Entity does not have a CinematicPlayerComponent.");
        }
    } else {
        Core::Logger::Info("EditorUI", "  No entity selected to edit timeline.");
    }
}

} // namespace KumariEngine::Editor
