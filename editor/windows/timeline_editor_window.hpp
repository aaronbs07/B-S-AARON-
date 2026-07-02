#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class TimelineEditorWindow : public EditorWindow {
public:
    TimelineEditorWindow() : EditorWindow("Timeline Editor") {}
    void RenderUI() override;
};

} // namespace KumariEngine::Editor
