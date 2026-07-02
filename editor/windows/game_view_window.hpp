#pragma once
#include "editor/window_system.hpp"
#include <glm/glm.hpp>

namespace KumariEngine::Editor {

enum class GameState {
    Edit,
    Play,
    Pause
};

class GameViewWindow : public EditorWindow {
public:
    GameViewWindow() : EditorWindow("Game View") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    GameState GetState() const { return m_state; }
    void SetState(GameState state) { m_state = state; }

    glm::vec2 GetViewportSize() const { return m_viewportSize; }
    void SetViewportSize(const glm::vec2& size) { m_viewportSize = size; }

private:
    GameState m_state = GameState::Edit;
    glm::vec2 m_viewportSize{800.0f, 600.0f};
};

} // namespace KumariEngine::Editor
