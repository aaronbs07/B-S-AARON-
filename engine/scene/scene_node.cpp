#include "scene_node.hpp"
#include "scene_manager.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace KumariEngine::Scene {

SceneNode::SceneNode(std::string_view name)
    : m_name(name) {}

SceneNode::~SceneNode() {
    if (m_entity != ECS::NULL_ENTITY) {
        SceneManager::Get().UnregisterEntityNode(m_entity);
    }
}

void SceneNode::SetEntity(ECS::Entity entity) {
    if (m_entity != ECS::NULL_ENTITY) {
        SceneManager::Get().UnregisterEntityNode(m_entity);
    }
    m_entity = entity;
    if (m_entity != ECS::NULL_ENTITY) {
        SceneManager::Get().RegisterEntityNode(m_entity, this);
    }
}

void SceneNode::SetLocalPosition(const glm::vec3& position) {
    m_localPosition = position;
    MarkDirty();
}

void SceneNode::SetLocalRotation(const glm::quat& rotation) {
    m_localRotation = rotation;
    MarkDirty();
}

void SceneNode::SetLocalScale(const glm::vec3& scale) {
    m_localScale = scale;
    MarkDirty();
}

const glm::mat4& SceneNode::GetLocalMatrix() {
    if (m_localDirty) {
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_localPosition);
        glm::mat4 rotationMat = glm::mat4_cast(m_localRotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_localScale);
        m_localMatrix = translationMat * rotationMat * scaleMat;
        m_localDirty = false;
    }
    return m_localMatrix;
}

void SceneNode::AddChild(std::unique_ptr<SceneNode> child) {
    if (child) {
        child->SetParent(this);
        m_children.push_back(std::move(child));
    }
}

std::unique_ptr<SceneNode> SceneNode::RemoveChild(SceneNode* child) {
    if (!child) return nullptr;

    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const std::unique_ptr<SceneNode>& ptr) {
            return ptr.get() == child;
        });

    if (it != m_children.end()) {
        std::unique_ptr<SceneNode> removed = std::move(*it);
        m_children.erase(it);
        removed->SetParent(nullptr);
        return removed;
    }

    return nullptr;
}

void SceneNode::SetParent(SceneNode* parent) {
    m_parent = parent;
    MarkDirty();
}

void SceneNode::UpdateTransforms(const glm::mat4& parentWorldMatrix, bool parentDirty) {
    bool isDirty = m_localDirty || m_worldDirty || parentDirty;
    if (isDirty) {
        m_worldMatrix = parentWorldMatrix * GetLocalMatrix();
        m_worldDirty = false;
    }
    for (auto& child : m_children) {
        child->UpdateTransforms(m_worldMatrix, isDirty);
    }
}

void SceneNode::MarkDirty() {
    m_localDirty = true;
    m_worldDirty = true;
    for (auto& child : m_children) {
        child->MarkWorldDirty();
    }
}

void SceneNode::MarkWorldDirty() {
    m_worldDirty = true;
    for (auto& child : m_children) {
        child->MarkWorldDirty();
    }
}

} // namespace KumariEngine::Scene
