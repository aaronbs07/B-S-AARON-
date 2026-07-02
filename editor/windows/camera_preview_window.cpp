#include "camera_preview_window.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void CameraPreviewWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Camera Preview ---");
    auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
    if (activeCam) {
        std::string modeStr = "Free";
        auto mode = activeCam->GetMode();
        if (mode == Camera::CameraMode::FirstPerson) modeStr = "First Person";
        else if (mode == Camera::CameraMode::ThirdPerson) modeStr = "Third Person";
        else if (mode == Camera::CameraMode::Cinematic) modeStr = "Cinematic";

        glm::vec3 pos = activeCam->GetCurrentPosition();
        glm::quat rot = activeCam->GetCurrentRotation();
        float fov = activeCam->GetFov();

        Core::Logger::Info("EditorUI", "  Active Camera Mode: %s", modeStr.c_str());
        Core::Logger::Info("EditorUI", "  Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        Core::Logger::Info("EditorUI", "  Rotation (Euler): Yaw=%.1f, Pitch=%.1f, Roll=%.1f",
                           activeCam->GetYaw(), activeCam->GetPitch(), activeCam->GetRoll());
        Core::Logger::Info("EditorUI", "  FOV: %.1f | Aspect: %.2f", fov, activeCam->GetAspect());
        Core::Logger::Info("EditorUI", "  Shake Timer: %.2fs | Active Shake: %s",
                           activeCam->GetShakeTimer(), activeCam->GetShakeTimer() > 0.0f ? "Yes" : "No");
    } else {
        Core::Logger::Info("EditorUI", "  No Active Camera in CameraManager.");
    }
}

} // namespace KumariEngine::Editor
