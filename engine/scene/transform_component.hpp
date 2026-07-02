#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace KumariEngine::Scene {

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    TransformComponent() = default;
    TransformComponent(const glm::vec3& pos, const glm::quat& rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), const glm::vec3& scl = glm::vec3(1.0f))
        : position(pos), rotation(rot), scale(scl) {}
};

} // namespace KumariEngine::Scene
