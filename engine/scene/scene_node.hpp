#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::Scene {

class SceneNode {
public:
    SceneNode(std::string_view name);
    ~SceneNode();

    // Prevent copy/assignment to preserve unique child ownership
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // Local Transform Accessors
    void SetLocalPosition(const glm::vec3& position);
    const glm::vec3& GetLocalPosition() const { return m_localPosition; }

    void SetLocalRotation(const glm::quat& rotation);
    const glm::quat& GetLocalRotation() const { return m_localRotation; }

    void SetLocalScale(const glm::vec3& scale);
    const glm::vec3& GetLocalScale() const { return m_localScale; }

    // Matrix evaluation (recalculates on demand if dirty)
    const glm::mat4& GetLocalMatrix();
    const glm::mat4& GetWorldMatrix() const { return m_worldMatrix; }

    // Hierarchy operations
    void AddChild(std::unique_ptr<SceneNode> child);
    std::unique_ptr<SceneNode> RemoveChild(SceneNode* child);
    void SetParent(SceneNode* parent);

    SceneNode* GetParent() const { return m_parent; }
    const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return m_children; }

    // Transform Update Propagation
    void UpdateTransforms(const glm::mat4& parentWorldMatrix = glm::mat4(1.0f), bool parentDirty = false);
    void MarkDirty();
    void MarkWorldDirty();

    // ECS linkage
    void SetEntity(ECS::Entity entity) { m_entity = entity; }
    ECS::Entity GetEntity() const { return m_entity; }

    const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    glm::vec3 m_localPosition{0.0f};
    glm::quat m_localRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_localScale{1.0f};

    glm::mat4 m_localMatrix{1.0f};
    glm::mat4 m_worldMatrix{1.0f};

    bool m_localDirty = true;
    bool m_worldDirty = true;

    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;
    ECS::Entity m_entity = ECS::NULL_ENTITY;
};

} // namespace KumariEngine::Scene
