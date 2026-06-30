#pragma once
#include <glm/glm.hpp>
#include "physics_types.hpp"

namespace KumariEngine::Physics {

struct PhysicsComponent {
    BodyType bodyType = BodyType::Static;
    Collider collider;

    // Rigid body parameters
    float mass = 1.0f;          // Infinite mass if 0.0f
    float inverseMass = 1.0f;   // Cached 1.0f / mass

    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    glm::vec3 forceAccum{0.0f};
    glm::vec3 torqueAccum{0.0f};

    float friction = 0.5f;
    float restitution = 0.1f;   // Bounciness

    CollisionLayer collisionLayer = LAYER_DEFAULT;
    CollisionLayer collisionMask = 0xFFFFFFFF; // Collides with everything

    // Set mass and update inverseMass
    void SetMass(float m) {
        mass = m;
        inverseMass = (m > 0.0f) ? 1.0f / m : 0.0f;
    }
};

enum class MovementState {
    Idle,
    Walking,
    Running,
    Sprinting,
    Jumping,
    Falling
};

struct CharacterControllerComponent {
    MovementState state = MovementState::Idle;
    
    // Config speeds
    float walkSpeed = 3.5f;
    float runSpeed = 6.0f;
    float sprintSpeed = 9.0f;
    float jumpForce = 6.0f;
    float gravityMultiplier = 2.0f;

    // Movement parameters
    glm::vec3 moveDirection{0.0f};
    bool requestJump = false;
    bool isGrounded = false;
    float verticalVelocity = 0.0f;

    // Constraints
    float slopeLimit = 45.0f; // maximum slope angle in degrees
    float stepHeight = 0.3f;  // step climbing height limit
};

} // namespace KumariEngine::Physics
