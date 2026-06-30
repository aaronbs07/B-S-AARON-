#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace KumariEngine::Input { class Input; }

namespace KumariEngine::Camera {

enum class CameraMode {
    Free,
    FirstPerson,
    ThirdPerson,
    Cinematic
};

struct CameraCollider {
    enum class Type { Sphere, AABB };
    Type type;
    glm::vec3 center{0.0f};
    float radius = 0.0f; // for Sphere
    glm::vec3 minBound{0.0f}; // for AABB
    glm::vec3 maxBound{0.0f}; // for AABB
    std::string name;
};

class Camera {
public:
    Camera();
    ~Camera() = default;

    // Camera Mode
    void SetMode(CameraMode mode);
    CameraMode GetMode() const { return m_mode; }

    // Transform properties
    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const { return m_position; }
    
    // Position/rotation interpolation state access
    const glm::vec3& GetCurrentPosition() const { return m_currentPosition; }
    const glm::quat& GetCurrentRotation() const { return m_currentRotation; }
    
    void SetRotation(const glm::quat& rotation);
    const glm::quat& GetRotation() const { return m_rotation; }

    // Roll angle (in degrees, useful for Photo Mode / Cinematic roll)
    void SetRoll(float rollDegrees);
    float GetRoll() const { return m_roll; }

    // Optics & Projection
    void SetFov(float fovDegrees);
    float GetFov() const { return m_fov; }

    void SetAspect(float aspect);
    float GetAspect() const { return m_aspect; }

    void SetNearClip(float nearClip);
    float GetNearClip() const { return m_nearClip; }

    void SetFarClip(float farClip);
    float GetFarClip() const { return m_farClip; }

    // Priority system (higher priority takes precedence in CameraManager)
    void SetPriority(int priority) { m_priority = priority; }
    int GetPriority() const { return m_priority; }

    // Mouse control
    void SetSensitivity(float sensitivity) { m_sensitivity = sensitivity; }
    float GetSensitivity() const { return m_sensitivity; }

    void SetPitch(float pitchDegrees);
    float GetPitch() const { return m_pitch; }

    void SetYaw(float yawDegrees);
    float GetYaw() const { return m_yaw; }

    // Movement & Rotation smoothing speeds
    void SetTranslationSmoothing(float speed) { m_translationSmoothing = speed; }
    float GetTranslationSmoothing() const { return m_translationSmoothing; }

    void SetRotationSmoothing(float speed) { m_rotationSmoothing = speed; }
    float GetRotationSmoothing() const { return m_rotationSmoothing; }

    // Target tracking properties
    void SetTargetPosition(const glm::vec3& targetPos) { m_targetPosition = targetPos; }
    const glm::vec3& GetTargetPosition() const { return m_targetPosition; }
    
    // Target height offset (e.g. eye height)
    void SetTargetHeightOffset(const glm::vec3& offset) { m_targetHeightOffset = offset; }
    const glm::vec3& GetTargetHeightOffset() const { return m_targetHeightOffset; }

    // Third-person shoulder offset (extra offset perpendicular to view direction)
    void SetShoulderOffset(const glm::vec3& offset) { m_shoulderOffset = offset; }
    const glm::vec3& GetShoulderOffset() const { return m_shoulderOffset; }

    // Orbit Distance (distance zoom)
    void SetOrbitDistance(float distance);
    float GetOrbitDistance() const { return m_orbitDistance; }
    
    void SetTargetOrbitDistance(float distance);
    float GetTargetOrbitDistance() const { return m_targetOrbitDistance; }
    
    void SetMinOrbitDistance(float minDist) { m_minOrbitDistance = minDist; }
    void SetMaxOrbitDistance(float maxDist) { m_maxOrbitDistance = maxDist; }

    // Automatic Repositioning (trailing behind target when target moves/turns)
    void SetAutoReposition(bool enable) { m_autoReposition = enable; }
    bool GetAutoReposition() const { return m_autoReposition; }
    void SetRepositionSpeed(float speed) { m_repositionSpeed = speed; }
    float GetRepositionSpeed() const { return m_repositionSpeed; }
    void SetTargetForward(const glm::vec3& forward) { m_targetForward = forward; }

    // Matrices (lazily recalculated when dirty)
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;

    // VR Stereo Support (Future Compatibility)
    void SetVREnabled(bool enable) { m_vrEnabled = enable; }
    bool IsVREnabled() const { return m_vrEnabled; }
    void SetEyeOffsets(const glm::vec3& leftOffset, const glm::vec3& rightOffset);
    void SetEyeProjections(const glm::mat4& leftProj, const glm::mat4& rightProj);
    glm::mat4 GetStereoViewMatrix(int eye) const; // 0 for Left, 1 for Right
    glm::mat4 GetStereoProjectionMatrix(int eye) const;

    // Photo Mode settings (Future Compatibility)
    void SetAperture(float fStop) { m_aperture = fStop; }
    float GetAperture() const { return m_aperture; }
    void SetFocalDistance(float dist) { m_focalDistance = dist; }
    float GetFocalDistance() const { return m_focalDistance; }

    // Direction vectors
    glm::vec3 GetForward() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;

    // Cinematic path keyframes
    struct CinematicKeyframe {
        float time; // timestamp in seconds
        glm::vec3 position;
        glm::quat rotation;
        float fov;
    };
    void SetCinematicPath(const std::vector<CinematicKeyframe>& path) { m_cinematicPath = path; }
    void PlayCinematic(bool loop = false);
    void StopCinematic();
    bool IsCinematicPlaying() const { return m_cinematicPlaying; }
    void SetCinematicTime(float time) { m_cinematicTime = time; }
    float GetCinematicTime() const { return m_cinematicTime; }

    // Collision Configuration
    void EnableCollision(bool enable) { m_collisionEnabled = enable; }
    bool IsCollisionEnabled() const { return m_collisionEnabled; }
    void SetCameraRadius(float radius) { m_cameraRadius = radius; }
    float GetCameraRadius() const { return m_cameraRadius; }

    void AddCollider(const CameraCollider& collider) { m_colliders.push_back(collider); }
    void ClearColliders() { m_colliders.clear(); }
    const std::vector<CameraCollider>& GetColliders() const { return m_colliders; }

    // Custom collision callback for terrain / external meshes
    using CollisionCallback = std::function<bool(const glm::vec3& start, const glm::vec3& end, float radius, glm::vec3& outHitPoint)>;
    void SetCollisionCallback(CollisionCallback callback) { m_collisionCallback = callback; }

    // Frustum Culling
    struct Plane {
        glm::vec3 normal{0.0f};
        float distance = 0.0f;

        float GetSignedDistance(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };

    struct Frustum {
        Plane planes[6]; // Near, Far, Left, Right, Top, Bottom
    };

    Frustum GetFrustum() const;
    bool IsBoxVisible(const glm::vec3& min, const glm::vec3& max) const;
    bool IsSphereVisible(const glm::vec3& center, float radius) const;

    // Debug visualization helpers
    std::vector<glm::vec3> GetFrustumCorners() const;
    std::vector<CameraCollider> GetCollisionVolumeDebug() const { return m_colliders; }

    // Update function to process camera kinematics and controls
    void Update(float deltaTime, const Input::Input* input);

private:
    void UpdateFreeCamera(float deltaTime, const Input::Input* input);
    void UpdateFirstPersonCamera(float deltaTime, const Input::Input* input);
    void UpdateThirdPersonCamera(float deltaTime, const Input::Input* input);
    void UpdateCinematicCamera(float deltaTime);

    glm::vec3 ResolveCollision(const glm::vec3& startPos, const glm::vec3& endPos) const;
    bool SweepSphere(const glm::vec3& start, const glm::vec3& end, float radius, float& outT) const;

    // Recalculates yaw and pitch from the rotation matrix
    void RecalculateAnglesFromRotation();

    // Mode
    CameraMode m_mode = CameraMode::Free;

    // Kinematics and targeting
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float m_roll = 0.0f;

    // Actual interpolated rendering values
    glm::vec3 m_currentPosition{0.0f, 0.0f, 0.0f};
    glm::quat m_currentRotation{1.0f, 0.0f, 0.0f, 0.0f};
    float m_currentFov = 60.0f;

    // Properties
    float m_fov = 60.0f;
    float m_aspect = 4.0f / 3.0f;
    float m_nearClip = 0.1f;
    float m_farClip = 1000.0f;
    int m_priority = 0;

    // Mouse variables
    float m_pitch = 0.0f;
    float m_yaw = -90.0f;
    float m_sensitivity = 0.1f;

    // Smoothing
    float m_translationSmoothing = 10.0f; // larger = faster, 0 = instant
    float m_rotationSmoothing = 10.0f;

    // Third-person parameters
    glm::vec3 m_targetPosition{0.0f};
    glm::vec3 m_targetHeightOffset{0.0f, 1.8f, 0.0f}; // Height offset of target
    glm::vec3 m_shoulderOffset{0.0f, 0.0f, 0.0f};    // Right/Up shoulder offset
    float m_orbitDistance = 5.0f;
    float m_targetOrbitDistance = 5.0f;
    float m_minOrbitDistance = 1.0f;
    float m_maxOrbitDistance = 20.0f;
    bool m_autoReposition = false;
    float m_repositionSpeed = 2.0f;
    glm::vec3 m_targetForward{0.0f, 0.0f, -1.0f};

    // VR parameters
    bool m_vrEnabled = false;
    glm::vec3 m_vrEyeOffset[2]{ glm::vec3(-0.03f, 0.0f, 0.0f), glm::vec3(0.03f, 0.0f, 0.0f) };
    glm::mat4 m_vrEyeProjection[2]{ glm::mat4(1.0f), glm::mat4(1.0f) };

    // Photo Mode properties
    float m_aperture = 1.8f;
    float m_focalDistance = 10.0f;

    // Cinematic variables
    std::vector<CinematicKeyframe> m_cinematicPath;
    bool m_cinematicPlaying = false;
    bool m_cinematicLoop = false;
    float m_cinematicTime = 0.0f;

    // Collision volumes and callbacks
    bool m_collisionEnabled = true;
    float m_cameraRadius = 0.2f;
    std::vector<CameraCollider> m_colliders;
    CollisionCallback m_collisionCallback;

    // Matrix caching
    mutable glm::mat4 m_cachedViewMatrix{1.0f};
    mutable glm::mat4 m_cachedProjectionMatrix{1.0f};
    mutable bool m_viewDirty = true;
    mutable bool m_projectionDirty = true;
};

} // namespace KumariEngine::Camera
