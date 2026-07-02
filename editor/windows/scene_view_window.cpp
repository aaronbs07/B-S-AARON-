#include "scene_view_window.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void SceneViewWindow::Initialize() {
    Core::Logger::Info("Editor", "Scene View Window Initialized");
}

void SceneViewWindow::Update(float deltaTime) {
    (void)deltaTime;
    auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
    if (activeCam) {
        m_activeCameraName = "Active Camera";
    } else {
        m_activeCameraName = "None";
    }
}

void SceneViewWindow::RenderUI() {
    // Programmatic UI layout hook
}

} // namespace KumariEngine::Editor
