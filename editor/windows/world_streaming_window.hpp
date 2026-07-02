#pragma once
#include "editor/window_system.hpp"

namespace KumariEngine::Editor {

class WorldStreamingWindow : public EditorWindow {
public:
    WorldStreamingWindow() : EditorWindow("World Streaming Visualization") {}
    
    void RenderUI() override;
    void ToggleChunkBorders();
};

} // namespace KumariEngine::Editor
