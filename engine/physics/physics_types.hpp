#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <variant>
#include <vector>
#include <cstdint>

namespace KumariEngine::Physics {

enum class BodyType {
    Static,
    Dynamic,
    Kinematic
};

enum class ColliderType {
    None,
    AABB,
    Sphere,
    Capsule
};

// 32-bit bitmask for collision filtering
using CollisionLayer = uint32_t;
constexpr CollisionLayer LAYER_DEFAULT = 1 << 0;
constexpr CollisionLayer LAYER_STATIC  = 1 << 1;
constexpr CollisionLayer LAYER_DYNAMIC = 1 << 2;
constexpr CollisionLayer LAYER_PLAYER  = 1 << 3;
constexpr CollisionLayer LAYER_TRIGGER = 1 << 4;

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

struct Sphere {
    glm::vec3 center{0.0f};
    float radius{0.5f};
};

struct Capsule {
    glm::vec3 center{0.0f};
    float halfHeight{0.5f}; // half distance between sphere centers
    float radius{0.5f};
};

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

struct RaycastHit {
    bool hasHit = false;
    float distance = 0.0f;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    uint32_t entity = 0;
};

struct ContactPoint {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float penetration{0.0f};
};

struct Collider {
    ColliderType type = ColliderType::None;
    bool isTrigger = false;
    std::variant<std::monostate, AABB, Sphere, Capsule> shape;
};

} // namespace KumariEngine::Physics
