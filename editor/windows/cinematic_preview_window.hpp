#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class CinematicPreviewWindow : public EditorWindow {
public:
    CinematicPreviewWindow() : EditorWindow("Cinematic Preview") {}
    void RenderUI() override;
};

} // namespace KumariEngine::Editor
