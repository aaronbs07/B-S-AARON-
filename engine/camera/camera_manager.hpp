#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "camera.hpp"

namespace KumariEngine::Camera {

enum class EasingCurve {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad
};

class CameraManager {
public:
    static CameraManager& Get() {
        static CameraManager instance;
        return instance;
    }

    // Prevent copy/assignment
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // Camera Registration
    void RegisterCamera(const std::string& name, std::shared_ptr<Camera> camera);
    void UnregisterCamera(const std::string& name);
    std::shared_ptr<Camera> GetCamera(const std::string& name) const;
    void Clear();

    // Active Camera management
    void SetActiveCamera(const std::string& name);
    std::shared_ptr<Camera> GetActiveCamera() const;

    // Manual priority-based auto activation override
    void SetAutoPrioritySelection(bool enable) { m_autoPrioritySelection = enable; }
    bool IsAutoPrioritySelection() const { return m_autoPrioritySelection; }

    // Eased transition between cameras
    void BlendToCamera(const std::string& name, float duration, EasingCurve curve = EasingCurve::Linear);
    void Update(float deltaTime);

    bool IsBlending() const { return m_isBlending; }
    float GetBlendProgress() const;

private:
    CameraManager();
    ~CameraManager() = default;

    float EvaluateEasing(float t, EasingCurve curve) const;

    std::unordered_map<std::string, std::shared_ptr<Camera>> m_cameras;
    std::shared_ptr<Camera> m_manualActiveCamera;
    std::shared_ptr<Camera> m_blendCamera;

    bool m_autoPrioritySelection = true;

    // Blending state
    bool m_isBlending = false;
    std::shared_ptr<Camera> m_blendSource;
    std::shared_ptr<Camera> m_blendTarget;
    float m_blendDuration = 0.0f;
    float m_blendTimer = 0.0f;
    EasingCurve m_blendCurve = EasingCurve::Linear;
};

} // namespace KumariEngine::Camera
