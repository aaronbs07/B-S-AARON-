#include "cinematic_preview_window.hpp"
#include "timeline/timeline.hpp"
#include "timeline/cinematic_system.hpp"
#include "editor/selection_system.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void CinematicPreviewWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Cinematic Preview Window ---");
    ECS::Entity selected = SelectionSystem::Get().GetSelected();
    auto* registry = WindowSystem::Get().GetRegistry();

    if (selected != ECS::NULL_ENTITY && registry && registry->IsAlive(selected)) {
        if (registry->HasComponent<Timeline::CinematicPlayerComponent>(selected)) {
            const auto& player = registry->GetComponent<Timeline::CinematicPlayerComponent>(selected);
            Core::Logger::Info("EditorUI", "  Timeline Name: %s", player.timelineName.c_str());
            Core::Logger::Info("EditorUI", "  Time Position: %.2fs", player.currentTime);
            Core::Logger::Info("EditorUI", "  Status: %s",
                               player.isPlaying ? (player.isPaused ? "PAUSED" : "PLAYING") : "STOPPED");

            // Diagnostic output to represent buttons
            Core::Logger::Info("EditorUI", "  [Playback Controls]:");
            Core::Logger::Info("EditorUI", "    * Button: Play   -> Triggers Playback");
            Core::Logger::Info("EditorUI", "    * Button: Pause  -> Pauses Playback");
            Core::Logger::Info("EditorUI", "    * Button: Resume -> Resumes Playback");
            Core::Logger::Info("EditorUI", "    * Button: Stop   -> Resets Playback");
            Core::Logger::Info("EditorUI", "    * Button: Skip   -> Skips to End");
        } else {
            Core::Logger::Info("EditorUI", "  Selected Entity does not have a CinematicPlayerComponent.");
        }
    } else {
        Core::Logger::Info("EditorUI", "  No entity selected to preview cinematic.");
    }
}

} // namespace KumariEngine::Editor
