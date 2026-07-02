#include "particle_system.hpp"
#include "scene/transform_component.hpp"
#include <algorithm>
#include <cmath>

namespace KumariEngine::Particle {

void ParticleSystem::Update(ECS::Registry* registry, float deltaTime) {
    if (!registry) return;

    registry->Each<ParticleSystemComponent, Scene::TransformComponent>([&](ECS::Entity entity, ParticleSystemComponent& psc, Scene::TransformComponent& tc) {
        (void)entity;
        auto& settings = psc.settings;
        auto& particles = psc.particles;

        // 1. Update existing particles
        size_t i = 0;
        while (i < particles.size()) {
            auto& p = particles[i];
            p.age += deltaTime;

            if (p.age >= p.lifetime) {
                // Swap with last and pop
                if (i != particles.size() - 1) {
                    particles[i] = std::move(particles.back());
                }
                particles.pop_back();
            } else {
                // Apply gravity
                p.velocity += settings.gravity * deltaTime;
                p.position += p.velocity * deltaTime;

                // Interpolate properties
                float t = std::clamp(p.age / p.lifetime, 0.0f, 1.0f);
                p.color = glm::mix(settings.startColor, settings.endColor, t);
                p.size = glm::mix(settings.startSize, settings.endSize, t);
                i++;
            }
        }

        // 2. Spawn new particles
        if (settings.emitRate > 0.0f) {
            psc.accumulatedTime += deltaTime;
            float spawnInterval = 1.0f / settings.emitRate;

            std::uniform_real_distribution<float> speedDist(settings.minSpeed, settings.maxSpeed);
            std::uniform_real_distribution<float> lifeDist(settings.minLifetime, settings.maxLifetime);
            std::uniform_real_distribution<float> sphereDist(-1.0f, 1.0f);

            while (psc.accumulatedTime >= spawnInterval) {
                psc.accumulatedTime -= spawnInterval;

                if (particles.size() >= static_cast<size_t>(settings.maxParticles)) {
                    break;
                }

                // Random direction vector on unit sphere
                glm::vec3 dir(sphereDist(m_rng), sphereDist(m_rng), sphereDist(m_rng));
                if (glm::length(dir) > 0.001f) {
                    dir = glm::normalize(dir);
                } else {
                    dir = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                Particle newParticle;
                newParticle.position = tc.position; // spawn at emitter transform
                newParticle.velocity = dir * speedDist(m_rng);
                newParticle.lifetime = lifeDist(m_rng);
                newParticle.age = 0.0f;
                newParticle.color = settings.startColor;
                newParticle.size = settings.startSize;

                particles.push_back(newParticle);
            }
        }
    });
}

} // namespace KumariEngine::Particle
