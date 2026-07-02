#include "transform_gizmo.hpp"
#include "undo_redo.hpp"
#include "scene/scene_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace KumariEngine::Editor {

void TransformGizmo::ApplyTranslation(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& worldDelta) {
    if (!registry || entity == ECS::NULL_ENTITY) return;
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (!node) return;

    glm::vec3 oldPos = node->GetLocalPosition();
    glm::quat oldRot = node->GetLocalRotation();
    glm::vec3 oldScale = node->GetLocalScale();

    // Compute parent orientation in world space
    auto* parent = node->GetParent();
    glm::quat worldRot = oldRot;
    if (parent) {
        glm::mat4 parentWorldMat = parent->GetWorldMatrix();
        glm::mat3 rotMat(parentWorldMat);
        glm::quat parentRot = glm::quat_cast(rotMat);
        worldRot = parentRot * oldRot;
    }

    glm::vec3 uX = worldRot * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 uY = worldRot * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 uZ = worldRot * glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 finalWorldDelta = worldDelta;

    if (m_space == GizmoSpace::World) {
        if (m_activeAxis == GizmoAxis::X) finalWorldDelta = glm::vec3(worldDelta.x, 0.0f, 0.0f);
        else if (m_activeAxis == GizmoAxis::Y) finalWorldDelta = glm::vec3(0.0f, worldDelta.y, 0.0f);
        else if (m_activeAxis == GizmoAxis::Z) finalWorldDelta = glm::vec3(0.0f, 0.0f, worldDelta.z);
        else if (m_activeAxis == GizmoAxis::XY) finalWorldDelta = glm::vec3(worldDelta.x, worldDelta.y, 0.0f);
        else if (m_activeAxis == GizmoAxis::YZ) finalWorldDelta = glm::vec3(0.0f, worldDelta.y, worldDelta.z);
        else if (m_activeAxis == GizmoAxis::ZX) finalWorldDelta = glm::vec3(worldDelta.x, 0.0f, worldDelta.z);
    } else { // Local space
        if (m_activeAxis == GizmoAxis::X) finalWorldDelta = glm::dot(worldDelta, uX) * uX;
        else if (m_activeAxis == GizmoAxis::Y) finalWorldDelta = glm::dot(worldDelta, uY) * uY;
        else if (m_activeAxis == GizmoAxis::Z) finalWorldDelta = glm::dot(worldDelta, uZ) * uZ;
        else if (m_activeAxis == GizmoAxis::XY) finalWorldDelta = glm::dot(worldDelta, uX) * uX + glm::dot(worldDelta, uY) * uY;
        else if (m_activeAxis == GizmoAxis::YZ) finalWorldDelta = glm::dot(worldDelta, uY) * uY + glm::dot(worldDelta, uZ) * uZ;
        else if (m_activeAxis == GizmoAxis::ZX) finalWorldDelta = glm::dot(worldDelta, uX) * uX + glm::dot(worldDelta, uZ) * uZ;
    }

    glm::mat4 parentWorldMat = parent ? parent->GetWorldMatrix() : glm::mat4(1.0f);
    glm::vec3 currentWorldPos = glm::vec3(node->GetWorldMatrix()[3]);
    glm::vec3 newWorldPos = currentWorldPos + finalWorldDelta;

    if (m_snappingEnabled) {
        newWorldPos.x = std::round(newWorldPos.x / m_translationSnap) * m_translationSnap;
        newWorldPos.y = std::round(newWorldPos.y / m_translationSnap) * m_translationSnap;
        newWorldPos.z = std::round(newWorldPos.z / m_translationSnap) * m_translationSnap;
    }

    glm::vec3 newLocalPos = glm::vec3(glm::inverse(parentWorldMat) * glm::vec4(newWorldPos, 1.0f));

    auto cmd = std::make_shared<ModifyTransformCommand>(registry, entity, oldPos, oldRot, oldScale, newLocalPos, oldRot, oldScale);
    UndoSystem::Get().Execute(cmd);
}

void TransformGizmo::ApplyRotation(ECS::Registry* registry, ECS::Entity entity, const glm::quat& deltaRotation) {
    if (!registry || entity == ECS::NULL_ENTITY) return;
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (!node) return;

    glm::vec3 oldPos = node->GetLocalPosition();
    glm::quat oldRot = node->GetLocalRotation();
    glm::vec3 oldScale = node->GetLocalScale();

    float angle = glm::angle(deltaRotation);
    glm::vec3 axis = glm::axis(deltaRotation);

    if (std::abs(angle) < 0.0001f) return;

    // Snapping
    if (m_snappingEnabled) {
        float snapRad = glm::radians(m_rotationSnap);
        angle = std::round(angle / snapRad) * snapRad;
    }

    // Apply constraints
    glm::vec3 rotAxis = axis;
    if (m_activeAxis == GizmoAxis::X) {
        rotAxis = (m_space == GizmoSpace::World) ? glm::vec3(1.0f, 0.0f, 0.0f) : (oldRot * glm::vec3(1.0f, 0.0f, 0.0f));
    } else if (m_activeAxis == GizmoAxis::Y) {
        rotAxis = (m_space == GizmoSpace::World) ? glm::vec3(0.0f, 1.0f, 0.0f) : (oldRot * glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (m_activeAxis == GizmoAxis::Z) {
        rotAxis = (m_space == GizmoSpace::World) ? glm::vec3(0.0f, 0.0f, 1.0f) : (oldRot * glm::vec3(0.0f, 0.0f, 1.0f));
    }

    glm::quat snappedDelta = glm::angleAxis(angle, glm::normalize(rotAxis));
    glm::quat newRot = snappedDelta * oldRot;

    auto cmd = std::make_shared<ModifyTransformCommand>(registry, entity, oldPos, oldRot, oldScale, oldPos, newRot, oldScale);
    UndoSystem::Get().Execute(cmd);
}

void TransformGizmo::ApplyScale(ECS::Registry* registry, ECS::Entity entity, const glm::vec3& deltaScale) {
    if (!registry || entity == ECS::NULL_ENTITY) return;
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
    if (!node) return;

    glm::vec3 oldPos = node->GetLocalPosition();
    glm::quat oldRot = node->GetLocalRotation();
    glm::vec3 oldScale = node->GetLocalScale();

    glm::vec3 constrainedDelta = deltaScale;
    if (m_activeAxis == GizmoAxis::X) constrainedDelta = glm::vec3(deltaScale.x, 0.0f, 0.0f);
    else if (m_activeAxis == GizmoAxis::Y) constrainedDelta = glm::vec3(0.0f, deltaScale.y, 0.0f);
    else if (m_activeAxis == GizmoAxis::Z) constrainedDelta = glm::vec3(0.0f, 0.0f, deltaScale.z);
    else if (m_activeAxis == GizmoAxis::XYZ) {
        float avg = (deltaScale.x + deltaScale.y + deltaScale.z) / 3.0f;
        constrainedDelta = glm::vec3(avg);
    }

    glm::vec3 newScale = oldScale + constrainedDelta;

    if (m_snappingEnabled) {
        newScale.x = std::round(newScale.x / m_scaleSnap) * m_scaleSnap;
        newScale.y = std::round(newScale.y / m_scaleSnap) * m_scaleSnap;
        newScale.z = std::round(newScale.z / m_scaleSnap) * m_scaleSnap;
    }

    newScale = glm::max(newScale, glm::vec3(0.001f));

    auto cmd = std::make_shared<ModifyTransformCommand>(registry, entity, oldPos, oldRot, oldScale, oldPos, oldRot, newScale);
    UndoSystem::Get().Execute(cmd);
}

} // namespace KumariEngine::Editor
