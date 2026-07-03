#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "reflection/reflection.hpp"

namespace KumariEngine::Scene {

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    TransformComponent() = default;
    TransformComponent(const glm::vec3& pos,
                       const glm::quat& rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                       const glm::vec3& scl = glm::vec3(1.0f))
        : position(pos), rotation(rot), scale(scl) {}
};

} // namespace KumariEngine::Scene

// ---------------------------------------------------------------------------
// Reflection Registration
// ---------------------------------------------------------------------------
REFLECT_COMPONENT_BEGIN(KumariEngine::Scene::TransformComponent, "TransformComponent")
    REFLECT_PROP_BEGIN(KumariEngine::Scene::TransformComponent, position)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Position")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Transform")
    REFLECT_PROP_COMMIT(KumariEngine::Scene::TransformComponent, position)

    REFLECT_PROP_BEGIN(KumariEngine::Scene::TransformComponent, rotation)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Rotation")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Transform")
    REFLECT_PROP_COMMIT(KumariEngine::Scene::TransformComponent, rotation)

    REFLECT_PROP_BEGIN(KumariEngine::Scene::TransformComponent, scale)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Scale")
        REFLECT_META(KumariEngine::Reflection::Meta::Category,    "Transform")
    REFLECT_PROP_COMMIT(KumariEngine::Scene::TransformComponent, scale)
REFLECT_END()
