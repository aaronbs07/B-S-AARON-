#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class CameraPreviewWindow : public EditorWindow {
public:
    CameraPreviewWindow() : EditorWindow("Camera Preview") {}
    void RenderUI() override;
};

} // namespace KumariEngine::Editor
