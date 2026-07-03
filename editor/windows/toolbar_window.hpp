#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class ToolbarWindow : public EditorWindow {
public:
    ToolbarWindow() : EditorWindow("Toolbar") {}

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    // Simulation commands
    void PressPlay();
    void PressPause();
    void PressStop();
    void PressStep();
    void SetTimeScale(float scale);
};

} // namespace KumariEngine::Editor
