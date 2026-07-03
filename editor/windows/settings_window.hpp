#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class SettingsWindow : public EditorWindow {
public:
    SettingsWindow() : EditorWindow("Settings") {}

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    // Configuration values
    bool vsync = true;
    int msaa = 4;
    std::string shadowQuality = "High";
    float lodBias = 1.0f;

    float gizmoSize = 1.0f;
    float translateSnap = 0.25f;
    float rotateSnap = 15.0f;
    float scaleSnap = 0.1f;
    bool gridSnapEnabled = true;

    void ApplySettings();
};

} // namespace KumariEngine::Editor
