#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class GameplayInspectorWindow : public EditorWindow {
public:
    GameplayInspectorWindow() : EditorWindow("Gameplay Framework Inspector") {}

    void RenderUI() override;
};

} // namespace KumariEngine::Editor
