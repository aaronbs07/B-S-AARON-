#include "selection_system.hpp"
#include "physics/debug_renderer.hpp"
#include "scene/transform_component.hpp"
#include "scene/scene_manager.hpp"
#include <algorithm>

namespace KumariEngine::Editor {

void SelectionSystem::Select(ECS::Entity entity) {
    m_selectedEntities.clear();
    if (entity != ECS::NULL_ENTITY) {
        m_selectedEntities.push_back(entity);
    }
    NotifyListeners();
}

void SelectionSystem::AddToSelection(ECS::Entity entity) {
    if (entity == ECS::NULL_ENTITY) return;
    if (!IsSelected(entity)) {
        m_selectedEntities.push_back(entity);
        NotifyListeners();
    }
}

void SelectionSystem::RemoveFromSelection(ECS::Entity entity) {
    auto it = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
    if (it != m_selectedEntities.end()) {
        m_selectedEntities.erase(it);
        NotifyListeners();
    }
}

void SelectionSystem::ToggleSelection(ECS::Entity entity) {
    if (IsSelected(entity)) {
        RemoveFromSelection(entity);
    } else {
        AddToSelection(entity);
    }
}

bool SelectionSystem::IsSelected(ECS::Entity entity) const {
    return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
}

void SelectionSystem::ClearSelection() {
    if (!m_selectedEntities.empty()) {
        m_selectedEntities.clear();
        NotifyListeners();
    }
}

void SelectionSystem::NotifyListeners() {
    ECS::Entity primary = GetSelected();
    for (const auto& callback : m_listeners) {
        if (callback) {
            callback(primary);
        }
    }
}

void SelectionSystem::HighlightSelection(ECS::Registry* registry) {
    if (!registry) return;

    for (ECS::Entity entity : m_selectedEntities) {
        if (!registry->IsAlive(entity)) continue;

        glm::vec3 pos{0.0f};
        glm::vec3 scale{1.0f};

        auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
        if (node) {
            pos = glm::vec3(node->GetWorldMatrix()[3]);
            scale = node->GetLocalScale();
        } else if (registry->HasComponent<Scene::TransformComponent>(entity)) {
            const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
            pos = tc.position;
            scale = tc.scale;
        } else {
            continue;
        }

        glm::vec3 highlightColor(1.0f, 0.6f, 0.0f);
        glm::vec3 extents = glm::max(scale * 0.5f, glm::vec3(0.1f));
        Physics::PhysicsDebugRenderer::Get().DrawAABB(pos - extents, pos + extents, highlightColor);
    }
}

void SelectionSystem::SelectRectangle(ECS::Registry* registry, const glm::vec2& mouseMin, const glm::vec2& mouseMax, const glm::mat4& viewProjMatrix, const glm::vec2& viewportSize) {
    if (!registry) return;

    ClearSelection();

    auto aliveEntities = registry->GetAliveEntities();
    for (auto entity : aliveEntities) {
        glm::vec3 pos{0.0f};
        auto* node = Scene::SceneManager::Get().GetNodeByEntity(entity);
        if (node) {
            pos = glm::vec3(node->GetWorldMatrix()[3]);
        } else if (registry->HasComponent<Scene::TransformComponent>(entity)) {
            pos = registry->GetComponent<Scene::TransformComponent>(entity).position;
        } else {
            continue;
        }

        glm::vec4 clipSpace = viewProjMatrix * glm::vec4(pos, 1.0f);
        if (clipSpace.w > 0.0f) {
            glm::vec3 ndcSpace = glm::vec3(clipSpace) / clipSpace.w;
            glm::vec2 screenSpace = glm::vec2(
                (ndcSpace.x + 1.0f) * 0.5f * viewportSize.x,
                (1.0f - ndcSpace.y) * 0.5f * viewportSize.y
            );
            if (screenSpace.x >= mouseMin.x && screenSpace.x <= mouseMax.x &&
                screenSpace.y >= mouseMin.y && screenSpace.y <= mouseMax.y) {
                AddToSelection(entity);
            }
        }
    }
}

} // namespace KumariEngine::Editor
