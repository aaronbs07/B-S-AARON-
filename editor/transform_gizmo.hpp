#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::Editor {

enum class GizmoMode { Translate, Rotate, Scale };
enum class GizmoSpace { Local, World };
enum class GizmoAxis { None, X, Y, Z, XY, YZ, ZX, XYZ };

class TransformGizmo {
public:
    static TransformGizmo& Get() {
        static TransformGizmo instance;
        return instance;
    }

    TransformGizmo(const TransformGizmo&) = delete;
    TransformGizmo& operator=(const TransformGizmo&) = delete;

    void SetMode(GizmoMode mode) { m_mode = mode; }
    GizmoMode GetMode() const { return m_mode; }

    void SetSpace(GizmoSpace space) { m_space = space; }
    GizmoSpace GetSpace() const { return m_space; }

    void SetActiveAxis(GizmoAxis axis) { m_activeAxis = axis; }
    GizmoAxis GetActiveAxis() const { return m_activeAxis; }

    // Snapping config
    void SetSnappingEnabled(bool enabled) { m_snappingEnabled = enabled; }
    bool IsSnappingEnabled() const { return m_snappingEnabled; }

    void SetTranslationSnap(float snap) { m_translationSnap = snap; }
    float GetTranslationSnap() const { return m_translationSnap; }

    void SetRotationSnap(float snapDegrees) { m_rotationSnap = snapDegrees; }
    float GetRotationSnap() const { return m_rotationSnap; }

    void SetScaleSnap(float snap) { m_scaleSnap = snap; }
    float GetScaleSnap() const { return m_scaleSnap; }

    // Transform Application helpers (execute a ModifyTransformCommand on execution)
    void ApplyTranslation(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& worldDelta);
    void ApplyRotation(ECS::Registry* registry, ECS::Entity entity, const glm::quat& deltaRotation);
    void ApplyScale(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& deltaScale);

private:
    TransformGizmo() = default;
    ~TransformGizmo() = default;

    GizmoMode m_mode = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::Local;
    GizmoAxis m_activeAxis = GizmoAxis::None;

    bool m_snappingEnabled = false;
    float m_translationSnap = 1.0f;
    float m_rotationSnap = 15.0f; // in degrees
    float m_scaleSnap = 0.1f;
};

} // namespace KumariEngine::Editor
