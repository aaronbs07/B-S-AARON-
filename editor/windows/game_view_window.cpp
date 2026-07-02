#include "game_view_window.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void GameViewWindow::Initialize() {
    Core::Logger::Info("Editor", "Game View Window Initialized");
}

void GameViewWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void GameViewWindow::RenderUI() {
    // Programmatic UI layout hook
}

} // namespace KumariEngine::Editor
