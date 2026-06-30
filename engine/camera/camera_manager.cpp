#include "camera_manager.hpp"
#include <algorithm>
#include <cmath>

namespace KumariEngine::Camera {

CameraManager::CameraManager() {
    m_blendCamera = std::make_shared<Camera>();
}

void CameraManager::RegisterCamera(const std::string& name, std::shared_ptr<Camera> camera) {
    if (camera) {
        m_cameras[name] = camera;
    }
}

void CameraManager::UnregisterCamera(const std::string& name) {
    auto it = m_cameras.find(name);
    if (it != m_cameras.end()) {
        if (m_manualActiveCamera == it->second) {
            m_manualActiveCamera.reset();
        }
        if (m_blendSource == it->second) {
            m_blendSource.reset();
        }
        if (m_blendTarget == it->second) {
            m_blendTarget.reset();
            m_isBlending = false;
        }
        m_cameras.erase(it);
    }
}

std::shared_ptr<Camera> CameraManager::GetCamera(const std::string& name) const {
    auto it = m_cameras.find(name);
    if (it != m_cameras.end()) {
        return it->second;
    }
    return nullptr;
}

void CameraManager::Clear() {
    m_cameras.clear();
    m_manualActiveCamera.reset();
    m_blendSource.reset();
    m_blendTarget.reset();
    m_isBlending = false;
}

void CameraManager::SetActiveCamera(const std::string& name) {
    auto cam = GetCamera(name);
    if (cam) {
        m_manualActiveCamera = cam;
        m_isBlending = false; // Cancel ongoing blend
    }
}

std::shared_ptr<Camera> CameraManager::GetActiveCamera() const {
    if (m_isBlending) {
        return m_blendCamera;
    }

    if (m_manualActiveCamera) {
        return m_manualActiveCamera;
    }

    if (m_autoPrioritySelection && !m_cameras.empty()) {
        // Find highest priority camera
        std::shared_ptr<Camera> highest = nullptr;
        int maxPriority = -999999;
        for (const auto& [name, cam] : m_cameras) {
            if (cam->GetPriority() > maxPriority) {
                maxPriority = cam->GetPriority();
                highest = cam;
            }
        }
        return highest;
    }

    return nullptr;
}

void CameraManager::BlendToCamera(const std::string& name, float duration, EasingCurve curve) {
    auto target = GetCamera(name);
    if (!target) return;

    auto source = GetActiveCamera();
    if (!source || source == target || duration <= 0.0f) {
        m_manualActiveCamera = target;
        m_isBlending = false;
        return;
    }

    m_blendSource = source;
    m_blendTarget = target;
    m_blendDuration = duration;
    m_blendTimer = 0.0f;
    m_blendCurve = curve;
    m_isBlending = true;

    // Initialize blend camera properties from source
    m_blendCamera->SetAspect(source->GetAspect());
    m_blendCamera->SetNearClip(source->GetNearClip());
    m_blendCamera->SetFarClip(source->GetFarClip());
    m_blendCamera->SetFov(source->GetFov());
    m_blendCamera->SetPosition(source->GetCurrentPosition());
    m_blendCamera->SetRotation(source->GetCurrentRotation());
}

float CameraManager::GetBlendProgress() const {
    if (!m_isBlending || m_blendDuration <= 0.0f) return 1.0f;
    return std::clamp(m_blendTimer / m_blendDuration, 0.0f, 1.0f);
}

float CameraManager::EvaluateEasing(float t, EasingCurve curve) const {
    switch (curve) {
        case EasingCurve::Linear:
            return t;
        case EasingCurve::EaseInQuad:
            return t * t;
        case EasingCurve::EaseOutQuad:
            return t * (2.0f - t);
        case EasingCurve::EaseInOutQuad:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
    }
    return t;
}

void CameraManager::Update(float deltaTime) {
    // Update all cameras so their smooth positions/rotations progress
    for (auto& [name, cam] : m_cameras) {
        cam->Update(deltaTime, nullptr);
    }

    if (!m_isBlending) return;

    m_blendTimer += deltaTime;
    float progress = GetBlendProgress();
    float t = EvaluateEasing(progress, m_blendCurve);

    // Interpolate camera parameters
    glm::vec3 pos = glm::mix(m_blendSource->GetCurrentPosition(), m_blendTarget->GetCurrentPosition(), t);
    glm::quat rot = glm::slerp(m_blendSource->GetCurrentRotation(), m_blendTarget->GetCurrentRotation(), t);
    float fov = glm::mix(m_blendSource->GetFov(), m_blendTarget->GetFov(), t);

    m_blendCamera->SetPosition(pos);
    m_blendCamera->SetRotation(rot);
    m_blendCamera->SetFov(fov);
    m_blendCamera->SetAspect(m_blendTarget->GetAspect());
    m_blendCamera->SetNearClip(m_blendTarget->GetNearClip());
    m_blendCamera->SetFarClip(m_blendTarget->GetFarClip());

    // Update internal blend camera tick (translation/rotation smoothing = 0 so it snaps to exactly our blended values)
    m_blendCamera->SetTranslationSmoothing(0.0f);
    m_blendCamera->SetRotationSmoothing(0.0f);
    m_blendCamera->Update(deltaTime, nullptr);

    if (progress >= 1.0f) {
        m_manualActiveCamera = m_blendTarget;
        m_isBlending = false;
    }
}

} // namespace KumariEngine::Camera
