#include "toolbar_window.hpp"
#include "editor/editor_manager.hpp"
#include "editor/editor.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void ToolbarWindow::Initialize() {
    Core::Logger::Info("Editor", "Toolbar Window Initialized.");
}

void ToolbarWindow::Shutdown() {}

void ToolbarWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void ToolbarWindow::RenderUI() {
    auto& em = EditorManager::Get();
    std::string stateStr = "Edit";
    if (em.IsPlaying()) stateStr = "Play";
    else if (em.IsPaused()) stateStr = "Pause";

    Core::Logger::Info("EditorUI", "=== [Toolbar Window] ===");
    Core::Logger::Info("EditorUI", "  Mode: %s | TimeScale: %.2f", stateStr.c_str(), em.GetTimeScale());
    Core::Logger::Info("EditorUI", "  Actions: [Play] [Pause] [Stop] [Step]");
    Core::Logger::Info("EditorUI", "  File: [New Scene] [Save Scene] [Open Scene]");
}

void ToolbarWindow::PressPlay() {
    EditorManager::Get().EnterPlayMode();
}

void ToolbarWindow::PressPause() {
    EditorManager::Get().PausePlayMode();
}

void ToolbarWindow::PressStop() {
    EditorManager::Get().ExitPlayMode();
}

void ToolbarWindow::PressStep() {
    EditorManager::Get().StepFrame();
}

void ToolbarWindow::SetTimeScale(float scale) {
    EditorManager::Get().SetTimeScale(scale);
}

} // namespace KumariEngine::Editor
