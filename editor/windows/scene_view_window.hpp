#pragma once
#include "editor/window_system.hpp"
#include <glm/glm.hpp>

namespace KumariEngine::Editor {

class SceneViewWindow : public EditorWindow {
public:
    SceneViewWindow() : EditorWindow("Scene View") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    glm::vec2 GetViewportSize() const { return m_viewportSize; }
    void SetViewportSize(const glm::vec2& size) { m_viewportSize = size; }

private:
    glm::vec2 m_viewportSize{800.0f, 600.0f};
    std::string m_activeCameraName = "None";
};

} // namespace KumariEngine::Editor
