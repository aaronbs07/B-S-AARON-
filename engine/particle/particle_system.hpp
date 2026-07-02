#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <random>
#include "ecs/ecs.hpp"

namespace KumariEngine::Particle {

struct Particle {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec4 color = glm::vec4(1.0f);
    float size = 1.0f;
    float lifetime = 1.0f;
    float age = 0.0f;
};

struct ParticleEmitterSettings {
    int maxParticles = 1000;
    float emitRate = 50.0f; // particles/sec
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    
    float minSpeed = 1.0f;
    float maxSpeed = 5.0f;
    float minLifetime = 0.5f;
    float maxLifetime = 2.0f;
    
    glm::vec4 startColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    
    float startSize = 0.5f;
    float endSize = 0.0f;
};

struct ParticleSystemComponent {
    ParticleEmitterSettings settings;
    std::vector<Particle> particles;
    float accumulatedTime = 0.0f;
};

class ParticleSystem {
public:
    static ParticleSystem& Get() {
        static ParticleSystem instance;
        return instance;
    }
    
    void Update(ECS::Registry* registry, float deltaTime);

private:
    ParticleSystem() : m_rng(1337) {}
    ~ParticleSystem() = default;
    
    std::mt19937 m_rng;
};

} // namespace KumariEngine::Particle
