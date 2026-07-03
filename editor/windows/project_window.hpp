#pragma once
#include "editor/window_system.hpp"
#include <vector>
#include <string>

namespace KumariEngine::Editor {

class ProjectWindow : public EditorWindow {
public:
    ProjectWindow() : EditorWindow("Project") {}

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    void RefreshAssets();

private:
    std::vector<std::string> m_recentScenes;
    int m_meshCount = 0;
    int m_textureCount = 0;
    int m_scriptCount = 0;
    int m_sceneCount = 0;
};

} // namespace KumariEngine::Editor
