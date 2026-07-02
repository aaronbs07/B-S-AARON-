#include "camera.hpp"
#include "input/input.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace KumariEngine::Camera {

Camera::Camera() {
    RecalculateAnglesFromRotation();
}

void Camera::SetMode(CameraMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        // Snap current to target to avoid massive interpolations when switching modes
        m_currentPosition = m_position;
        m_currentRotation = m_rotation;
        m_viewDirty = true;
        
        if (m_mode == CameraMode::ThirdPerson) {
            m_orbitDistance = m_targetOrbitDistance;
        }
        RecalculateAnglesFromRotation();
    }
}

void Camera::SetPosition(const glm::vec3& position) {
    m_position = position;
    m_viewDirty = true;
}

void Camera::SetRotation(const glm::quat& rotation) {
    m_rotation = glm::normalize(rotation);
    m_viewDirty = true;
    RecalculateAnglesFromRotation();
}

void Camera::SetRoll(float rollDegrees) {
    m_roll = rollDegrees;
    m_viewDirty = true;
    
    // Reconstruct rotation
    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;
}

void Camera::SetFov(float fovDegrees) {
    m_fov = fovDegrees;
    m_projectionDirty = true;
}

void Camera::SetAspect(float aspect) {
    m_aspect = aspect;
    m_projectionDirty = true;
}

void Camera::SetNearClip(float nearClip) {
    m_nearClip = nearClip;
    m_projectionDirty = true;
}

void Camera::SetFarClip(float farClip) {
    m_farClip = farClip;
    m_projectionDirty = true;
}

void Camera::SetPitch(float pitchDegrees) {
    m_pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);
    m_viewDirty = true;
    
    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;
}

void Camera::SetYaw(float yawDegrees) {
    m_yaw = std::fmod(yawDegrees, 360.0f);
    m_viewDirty = true;
    
    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;
}

void Camera::SetOrbitDistance(float distance) {
    m_orbitDistance = std::clamp(distance, m_minOrbitDistance, m_maxOrbitDistance);
    m_viewDirty = true;
}

void Camera::SetTargetOrbitDistance(float distance) {
    m_targetOrbitDistance = std::clamp(distance, m_minOrbitDistance, m_maxOrbitDistance);
}

void Camera::SetEyeOffsets(const glm::vec3& leftOffset, const glm::vec3& rightOffset) {
    m_vrEyeOffset[0] = leftOffset;
    m_vrEyeOffset[1] = rightOffset;
    m_viewDirty = true;
}

void Camera::SetEyeProjections(const glm::mat4& leftProj, const glm::mat4& rightProj) {
    m_vrEyeProjection[0] = leftProj;
    m_vrEyeProjection[1] = rightProj;
    m_projectionDirty = true;
}

glm::mat4 Camera::GetViewMatrix() const {
    if (m_viewDirty || m_shakeTimer > 0.0f) {
        glm::vec3 shakeOffset(0.0f);
        if (m_shakeTimer > 0.0f && m_shakeDuration > 0.0f) {
            float currentIntensity = m_shakeIntensity * (m_shakeTimer / m_shakeDuration);
            float timeVal = static_cast<float>(glfwGetTime()) * m_shakeSpeed;
            shakeOffset.x = std::sin(timeVal) * currentIntensity;
            shakeOffset.y = std::cos(timeVal * 1.2f) * currentIntensity;
            shakeOffset.z = std::sin(timeVal * 0.8f) * currentIntensity;
        }
        glm::vec3 pos = m_currentPosition + shakeOffset;
        glm::vec3 forward = m_currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = m_currentRotation * glm::vec3(0.0f, 1.0f, 0.0f);
        m_cachedViewMatrix = glm::lookAt(pos, pos + forward, up);
        if (m_shakeTimer <= 0.0f) {
            m_viewDirty = false;
        }
    }
    return m_cachedViewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix() const {
    if (m_projectionDirty) {
        m_cachedProjectionMatrix = glm::perspective(glm::radians(m_currentFov), m_aspect, m_nearClip, m_farClip);
        m_projectionDirty = false;
    }
    return m_cachedProjectionMatrix;
}

glm::mat4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

glm::mat4 Camera::GetStereoViewMatrix(int eye) const {
    if (!m_vrEnabled) return GetViewMatrix();
    // Offset camera position perpendicular to view direction
    glm::vec3 right = m_currentRotation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = m_currentRotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 forward = m_currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    
    // Apply eye offset locally
    glm::vec3 eyePos = m_currentPosition + right * m_vrEyeOffset[eye].x + up * m_vrEyeOffset[eye].y + forward * m_vrEyeOffset[eye].z;
    return glm::lookAt(eyePos, eyePos + forward, up);
}

glm::mat4 Camera::GetStereoProjectionMatrix(int eye) const {
    if (!m_vrEnabled) return GetProjectionMatrix();
    return m_vrEyeProjection[eye];
}

glm::vec3 Camera::GetForward() const {
    return m_rotation * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 Camera::GetUp() const {
    return m_rotation * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 Camera::GetRight() const {
    return m_rotation * glm::vec3(1.0f, 0.0f, 0.0f);
}

void Camera::PlayCinematic(bool loop) {
    m_cinematicPlaying = true;
    m_cinematicLoop = loop;
    m_cinematicTime = 0.0f;
    SetMode(CameraMode::Cinematic);
}

void Camera::StopCinematic() {
    m_cinematicPlaying = false;
}

void Camera::StartShake(float intensity, float duration, float speed) {
    m_shakeIntensity = intensity;
    m_shakeDuration = duration;
    m_shakeTimer = duration;
    m_shakeSpeed = speed;
    m_viewDirty = true;
}

void Camera::RecalculateAnglesFromRotation() {
    // Extract yaw and pitch from normalized rotation quaternion
    glm::vec3 forward = m_rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    
    // Pitch: angle with the XZ plane
    m_pitch = glm::degrees(std::asin(forward.y));
    
    // Yaw: angle on the XZ plane
    m_yaw = glm::degrees(std::atan2(-forward.z, forward.x)) - 90.0f;
    if (m_yaw < 0.0f) m_yaw += 360.0f;
}

void Camera::Update(float deltaTime, const Input::Input* input) {
    if (deltaTime <= 0.0f) return;

    // Decay camera shake
    if (m_shakeTimer > 0.0f) {
        m_shakeTimer -= deltaTime;
        if (m_shakeTimer < 0.0f) {
            m_shakeTimer = 0.0f;
            m_shakeIntensity = 0.0f;
        }
        m_viewDirty = true;
    }

    // Update active camera modes
    switch (m_mode) {
        case CameraMode::Free:
            UpdateFreeCamera(deltaTime, input);
            break;
        case CameraMode::FirstPerson:
            UpdateFirstPersonCamera(deltaTime, input);
            break;
        case CameraMode::ThirdPerson:
            UpdateThirdPersonCamera(deltaTime, input);
            break;
        case CameraMode::Cinematic:
            UpdateCinematicCamera(deltaTime);
            break;
    }

    // Resolve static collision (sliding/push-out) for modes other than Third-Person
    if (m_collisionEnabled && m_mode != CameraMode::ThirdPerson) {
        m_position = ResolveCollision(m_position, m_position);
    }

    // Apply smooth frame-rate independent interpolation (Dampening)
    bool transformChanged = false;
    
    if (m_translationSmoothing <= 0.0f) {
        if (m_currentPosition != m_position) {
            m_currentPosition = m_position;
            transformChanged = true;
        }
    } else {
        float t = 1.0f - std::exp(-m_translationSmoothing * deltaTime);
        glm::vec3 nextPos = glm::mix(m_currentPosition, m_position, t);
        if (glm::distance(m_currentPosition, nextPos) > 1e-5f) {
            m_currentPosition = nextPos;
            transformChanged = true;
        }
    }

    if (m_rotationSmoothing <= 0.0f) {
        if (m_currentRotation != m_rotation) {
            m_currentRotation = m_rotation;
            transformChanged = true;
        }
    } else {
        float t = 1.0f - std::exp(-m_rotationSmoothing * deltaTime);
        glm::quat nextRot = glm::slerp(m_currentRotation, m_rotation, t);
        if (glm::dot(m_currentRotation, nextRot) < 0.99999f) {
            m_currentRotation = nextRot;
            transformChanged = true;
        }
    }

    // Smoothly interpolate FOV
    float fovT = 1.0f - std::exp(-10.0f * deltaTime); // fixed speed for FOV responsiveness
    float nextFov = glm::mix(m_currentFov, m_fov, fovT);
    if (std::abs(m_currentFov - nextFov) > 1e-4f) {
        m_currentFov = nextFov;
        m_projectionDirty = true;
    }

    if (transformChanged) {
        m_viewDirty = true;
    }
}

void Camera::UpdateFreeCamera(float deltaTime, const Input::Input* input) {
    if (!input) return;

    // Mouse look
    double dx = 0.0, dy = 0.0;
    input->GetMouseDelta(dx, dy);

    m_yaw += static_cast<float>(dx) * m_sensitivity;
    m_pitch -= static_cast<float>(dy) * m_sensitivity;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    m_yaw = std::fmod(m_yaw, 360.0f);
    if (m_yaw < 0.0f) m_yaw += 360.0f;

    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;

    // Keyboard movement
    float speed = 8.0f;
    if (input->IsKeyDown(GLFW_KEY_LEFT_SHIFT)) speed *= 2.5f;

    glm::vec3 moveDir(0.0f);
    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();

    if (input->IsKeyDown(GLFW_KEY_W)) moveDir += forward;
    if (input->IsKeyDown(GLFW_KEY_S)) moveDir -= forward;
    if (input->IsKeyDown(GLFW_KEY_D)) moveDir += right;
    if (input->IsKeyDown(GLFW_KEY_A)) moveDir -= right;
    if (input->IsKeyDown(GLFW_KEY_E)) moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
    if (input->IsKeyDown(GLFW_KEY_Q)) moveDir -= glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
        glm::vec3 nextPos = m_position + moveDir * speed * deltaTime;
        m_position = ResolveCollision(m_position, nextPos);
    }
}

void Camera::UpdateFirstPersonCamera(float deltaTime, const Input::Input* input) {
    (void)deltaTime;
    if (!input) return;

    // Mouse look
    double dx = 0.0, dy = 0.0;
    input->GetMouseDelta(dx, dy);

    m_yaw += static_cast<float>(dx) * m_sensitivity;
    m_pitch -= static_cast<float>(dy) * m_sensitivity;
    m_pitch = std::clamp(m_pitch, -85.0f, 85.0f); // limit slightly more for FP
    m_yaw = std::fmod(m_yaw, 360.0f);
    if (m_yaw < 0.0f) m_yaw += 360.0f;

    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;

    // First person locks position to target + offset
    m_position = m_targetPosition + m_targetHeightOffset;
    
    // Zoom support via FOV modification (e.g. holding key zoom)
    if (input->IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        m_fov = 35.0f; // zoom in
    } else {
        m_fov = 60.0f; // default FOV
    }
}

void Camera::UpdateThirdPersonCamera(float deltaTime, const Input::Input* input) {
    // Mouse look orbits target
    if (input) {
        double dx = 0.0, dy = 0.0;
        input->GetMouseDelta(dx, dy);

        m_yaw += static_cast<float>(dx) * m_sensitivity;
        m_pitch -= static_cast<float>(dy) * m_sensitivity;
        m_pitch = std::clamp(m_pitch, -75.0f, 75.0f); // avoid ground clipping
        m_yaw = std::fmod(m_yaw, 360.0f);
        if (m_yaw < 0.0f) m_yaw += 360.0f;
    }

    // Auto-reposition behind target if moving / requested
    if (m_autoReposition && glm::length(m_targetForward) > 0.01f) {
        float targetYaw = glm::degrees(std::atan2(-m_targetForward.z, m_targetForward.x)) - 90.0f;
        if (targetYaw < 0.0f) targetYaw += 360.0f;
        
        // Find shortest path between yaw and targetYaw
        float diff = targetYaw - m_yaw;
        while (diff < -180.0f) diff += 360.0f;
        while (diff > 180.0f) diff -= 360.0f;
        
        m_yaw += diff * m_repositionSpeed * deltaTime;
        m_yaw = std::fmod(m_yaw, 360.0f);
        if (m_yaw < 0.0f) m_yaw += 360.0f;
    }

    glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll = glm::angleAxis(glm::radians(m_roll), glm::vec3(0.0f, 0.0f, 1.0f));
    m_rotation = qYaw * qPitch * qRoll;

    // Pivot is target + targetHeightOffset
    glm::vec3 pivot = m_targetPosition + m_targetHeightOffset;
    
    // Add shoulder offset in local camera coordinates
    glm::vec3 cameraRight = m_rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = m_rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 cameraForward = m_rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    
    glm::vec3 modifiedPivot = pivot + cameraRight * m_shoulderOffset.x + cameraUp * m_shoulderOffset.y + cameraForward * m_shoulderOffset.z;

    // Zoom distance interpolation
    m_orbitDistance = glm::mix(m_orbitDistance, m_targetOrbitDistance, 1.0f - std::exp(-10.0f * deltaTime));

    // Desired camera position
    glm::vec3 desiredPosition = modifiedPivot - cameraForward * m_orbitDistance;

    // Collision check from pivot to desired position
    if (m_collisionEnabled) {
        m_position = ResolveCollision(modifiedPivot, desiredPosition);
    } else {
        m_position = desiredPosition;
    }
}

void Camera::UpdateCinematicCamera(float deltaTime) {
    if (!m_cinematicPlaying || m_cinematicPath.empty()) return;

    m_cinematicTime += deltaTime;
    
    // Find keyframe interval
    size_t prevIdx = 0;
    size_t nextIdx = 0;
    
    if (m_cinematicTime >= m_cinematicPath.back().time) {
        if (m_cinematicLoop) {
            m_cinematicTime = std::fmod(m_cinematicTime, m_cinematicPath.back().time);
        } else {
            // Cap at the end
            m_cinematicPlaying = false;
            m_position = m_cinematicPath.back().position;
            m_rotation = m_cinematicPath.back().rotation;
            m_fov = m_cinematicPath.back().fov;
            return;
        }
    }

    for (size_t i = 0; i < m_cinematicPath.size() - 1; ++i) {
        if (m_cinematicTime >= m_cinematicPath[i].time && m_cinematicTime <= m_cinematicPath[i + 1].time) {
            prevIdx = i;
            nextIdx = i + 1;
            break;
        }
    }

    const auto& prevKey = m_cinematicPath[prevIdx];
    const auto& nextKey = m_cinematicPath[nextIdx];
    
    float denom = nextKey.time - prevKey.time;
    float t = (denom > 0.0001f) ? (m_cinematicTime - prevKey.time) / denom : 0.0f;

    // Interpolate
    m_position = glm::mix(prevKey.position, nextKey.position, t);
    m_rotation = glm::slerp(prevKey.rotation, nextKey.rotation, t);
    m_fov = glm::mix(prevKey.fov, nextKey.fov, t);
    
    RecalculateAnglesFromRotation();
}

bool Camera::SweepSphere(const glm::vec3& start, const glm::vec3& end, float radius, float& outT) const {
    glm::vec3 ray = end - start;
    float rayLen = glm::length(ray);
    if (rayLen < 0.0001f) return false;
    glm::vec3 rayDir = ray / rayLen;

    float minT = 1.0f;
    bool hitOccurred = false;

    for (const auto& col : m_colliders) {
        if (col.type == CameraCollider::Type::Sphere) {
            // Ray vs Sphere
            glm::vec3 v = start - col.center;
            float A = glm::dot(ray, ray);
            float B = 2.0f * glm::dot(ray, v);
            float C = glm::dot(v, v) - (col.radius + radius) * (col.radius + radius);
            float disc = B * B - 4.0f * A * C;
            
            if (disc >= 0.0f) {
                float t1 = (-B - std::sqrt(disc)) / (2.0f * A);
                if (t1 >= 0.0f && t1 <= 1.0f) {
                    if (t1 < minT) {
                        minT = t1;
                        hitOccurred = true;
                    }
                }
                float t2 = (-B + std::sqrt(disc)) / (2.0f * A);
                if (t2 >= 0.0f && t2 <= 1.0f) {
                    if (t2 < minT) {
                        minT = t2;
                        hitOccurred = true;
                    }
                }
            }
        } else if (col.type == CameraCollider::Type::AABB) {
            // Ray vs AABB (expanded by sphere radius)
            glm::vec3 minB = col.minBound - glm::vec3(radius);
            glm::vec3 maxB = col.maxBound + glm::vec3(radius);
            
            float tEnter = 0.0f;
            float tExit = 1.0f;
            bool localHit = true;

            for (int i = 0; i < 3; ++i) {
                if (std::abs(ray[i]) < 1e-6f) {
                    if (start[i] < minB[i] || start[i] > maxB[i]) {
                        localHit = false;
                        break;
                    }
                } else {
                    float t1 = (minB[i] - start[i]) / ray[i];
                    float t2 = (maxB[i] - start[i]) / ray[i];
                    float tNear = std::min(t1, t2);
                    float tFar = std::max(t1, t2);
                    tEnter = std::max(tEnter, tNear);
                    tExit = std::min(tExit, tFar);
                }
            }

            if (localHit && tEnter <= tExit && tEnter >= 0.0f && tEnter <= 1.0f) {
                if (tEnter < minT) {
                    minT = tEnter;
                    hitOccurred = true;
                }
            }
        }
    }

    if (hitOccurred) {
        outT = minT;
        return true;
    }
    return false;
}

glm::vec3 Camera::ResolveCollision(const glm::vec3& startPos, const glm::vec3& endPos) const {
    // 1. Check custom collision callback first
    if (m_collisionCallback) {
        glm::vec3 hitPoint;
        if (m_collisionCallback(startPos, endPos, m_cameraRadius, hitPoint)) {
            // Push hit point slightly back along normal / ray direction
            glm::vec3 toStart = startPos - endPos;
            if (glm::length(toStart) > 0.001f) {
                return hitPoint + glm::normalize(toStart) * m_cameraRadius;
            }
            return hitPoint;
        }
    }

    // 2. Check local colliders for Spring Arm shortening (Third-Person) or push-back (Free/FP)
    float t = 1.0f;
    if (SweepSphere(startPos, endPos, m_cameraRadius, t)) {
        // Shorten target position
        glm::vec3 offset = endPos - startPos;
        // Move camera close but keep safe margin
        return startPos + offset * std::max(0.01f, t - 0.02f);
    }

    // 3. For Free / FP camera, we check if the end point itself penetrates any volume, and slide it
    glm::vec3 finalPos = endPos;
    for (const auto& col : m_colliders) {
        if (col.type == CameraCollider::Type::Sphere) {
            float dist = glm::distance(finalPos, col.center);
            float minDist = col.radius + m_cameraRadius;
            if (dist < minDist) {
                if (dist > 0.0001f) {
                    finalPos = col.center + (finalPos - col.center) * (minDist / dist);
                } else {
                    finalPos += glm::vec3(0.0f, minDist, 0.0f); // default push up
                }
            }
        } else if (col.type == CameraCollider::Type::AABB) {
            // Closest point on AABB
            glm::vec3 closestP = glm::clamp(finalPos, col.minBound, col.maxBound);
            float dist = glm::distance(finalPos, closestP);
            if (dist < m_cameraRadius) {
                if (dist > 0.0001f) {
                    // Push away from closest point
                    finalPos = closestP + glm::normalize(finalPos - closestP) * m_cameraRadius;
                } else {
                    // Inside box: find closest face and push out
                    float dx1 = finalPos.x - col.minBound.x;
                    float dx2 = col.maxBound.x - finalPos.x;
                    float dy1 = finalPos.y - col.minBound.y;
                    float dy2 = col.maxBound.y - finalPos.y;
                    float dz1 = finalPos.z - col.minBound.z;
                    float dz2 = col.maxBound.z - finalPos.z;
                    
                    float minOverlap = std::min({dx1, dx2, dy1, dy2, dz1, dz2});
                    if (minOverlap == dx1) finalPos.x -= m_cameraRadius;
                    else if (minOverlap == dx2) finalPos.x += m_cameraRadius;
                    else if (minOverlap == dy1) finalPos.y -= m_cameraRadius;
                    else if (minOverlap == dy2) finalPos.y += m_cameraRadius;
                    else if (minOverlap == dz1) finalPos.z -= m_cameraRadius;
                    else finalPos.z += m_cameraRadius;
                }
            }
        }
    }

    return finalPos;
}

Camera::Frustum Camera::GetFrustum() const {
    glm::mat4 vp = GetViewProjectionMatrix();
    
    Frustum f;
    // Extract planes from rows of VP matrix
    // GLM row index is second bracket in col-major: vp[col][row]
    glm::vec4 r0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    glm::vec4 r1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    glm::vec4 r2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    glm::vec4 r3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

    // Left Plane: row3 + row0
    glm::vec4 left = r3 + r0;
    f.planes[0] = { glm::vec3(left), left.w };

    // Right Plane: row3 - row0
    glm::vec4 right = r3 - r0;
    f.planes[1] = { glm::vec3(right), right.w };

    // Bottom Plane: row3 + row1
    glm::vec4 bottom = r3 + r1;
    f.planes[2] = { glm::vec3(bottom), bottom.w };

    // Top Plane: row3 - row1
    glm::vec4 top = r3 - r1;
    f.planes[3] = { glm::vec3(top), top.w };

    // Near Plane: row2 (maps near clip space to z=0 in Vulkan)
    f.planes[4] = { glm::vec3(r2), r2.w };

    // Far Plane: row3 - row2 (maps far clip space to z=w in Vulkan)
    glm::vec4 farPlane = r3 - r2;
    f.planes[5] = { glm::vec3(farPlane), farPlane.w };

    // Normalize all planes so plane distances represent exact world space units
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(f.planes[i].normal);
        if (len > 0.00001f) {
            f.planes[i].normal /= len;
            f.planes[i].distance /= len;
        }
    }

    return f;
}

bool Camera::IsBoxVisible(const glm::vec3& min, const glm::vec3& max) const {
    Frustum f = GetFrustum();

    // Check all 6 planes
    for (int i = 0; i < 6; ++i) {
        // Find p-vertex (closest to plane normal direction)
        glm::vec3 p = min;
        if (f.planes[i].normal.x >= 0.0f) p.x = max.x;
        if (f.planes[i].normal.y >= 0.0f) p.y = max.y;
        if (f.planes[i].normal.z >= 0.0f) p.z = max.z;

        if (f.planes[i].GetSignedDistance(p) < 0.0f) {
            // Box is completely on the outside half-space of this plane
            return false;
        }
    }
    return true;
}

bool Camera::IsSphereVisible(const glm::vec3& center, float radius) const {
    Frustum f = GetFrustum();

    for (int i = 0; i < 6; ++i) {
        if (f.planes[i].GetSignedDistance(center) < -radius) {
            // Sphere center is further than radius behind the plane
            return false;
        }
    }
    return true;
}

std::vector<glm::vec3> Camera::GetFrustumCorners() const {
    // Vulkan Clip space box
    std::vector<glm::vec3> ndcCorners = {
        {-1.0f, -1.0f, 0.0f}, // Near Bottom Left
        { 1.0f, -1.0f, 0.0f}, // Near Bottom Right
        { 1.0f,  1.0f, 0.0f}, // Near Top Right
        {-1.0f,  1.0f, 0.0f}, // Near Top Left
        {-1.0f, -1.0f, 1.0f}, // Far Bottom Left
        { 1.0f, -1.0f, 1.0f}, // Far Bottom Right
        { 1.0f,  1.0f, 1.0f}, // Far Top Right
        {-1.0f,  1.0f, 1.0f}  // Far Top Left
    };

    glm::mat4 invVP = glm::inverse(GetViewProjectionMatrix());
    std::vector<glm::vec3> worldCorners;
    worldCorners.reserve(8);

    for (const auto& ndc : ndcCorners) {
        glm::vec4 worldH = invVP * glm::vec4(ndc, 1.0f);
        worldCorners.push_back(glm::vec3(worldH) / worldH.w);
    }

    return worldCorners;
}

} // namespace KumariEngine::Camera
