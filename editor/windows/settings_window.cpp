#include "settings_window.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void SettingsWindow::Initialize() {
    Core::Logger::Info("Editor", "Settings Window Initialized.");
}

void SettingsWindow::Shutdown() {}

void SettingsWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void SettingsWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Settings Window] ===");
    Core::Logger::Info("EditorUI", "  --- Graphics Settings ---");
    Core::Logger::Info("EditorUI", "    VSync: %s | MSAA: %dx", vsync ? "Enabled" : "Disabled", msaa);
    Core::Logger::Info("EditorUI", "    Shadow Quality: %s | LOD Bias: %.2f", shadowQuality.c_str(), lodBias);
    Core::Logger::Info("EditorUI", "  --- Editor Settings ---");
    Core::Logger::Info("EditorUI", "    Gizmo Size: %.2f", gizmoSize);
    Core::Logger::Info("EditorUI", "    Snaps -> Translate: %.2f | Rotate: %.1f deg | Scale: %.2f", translateSnap, rotateSnap, scaleSnap);
    Core::Logger::Info("EditorUI", "    Grid Snap: %s", gridSnapEnabled ? "ON" : "OFF");
}

void SettingsWindow::ApplySettings() {
    Core::Logger::Info("Editor", "Settings applied successfully.");
    // In a real editor, this would propagate values to the renderer / config files.
}

} // namespace KumariEngine::Editor
