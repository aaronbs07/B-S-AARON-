#include "raycast.hpp"
#include "terrain/terrain_manager.hpp"
#include "scene/scene_manager.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <variant>

namespace KumariEngine::Physics {

Ray Raycast::ViewportPointToRay(float mouseX, float mouseY, 
                                float viewportWidth, float viewportHeight,
                                const glm::mat4& viewMatrix, 
                                const glm::mat4& projMatrix) {
    // 1. Normalized Device Coordinates (-1 to 1)
    float x = (2.0f * mouseX) / viewportWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / viewportHeight; // Invert Y for screen space

    glm::vec4 clipCoords(x, y, -1.0f, 1.0f);

    // 2. Perspective division reversal (Clip space -> View space)
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::vec4 eyeCoords = invProj * clipCoords;
    eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f); // Direction vector

    // 3. View matrix reversal (View space -> World space)
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec3 rayDir = glm::normalize(glm::vec3(invView * eyeCoords));
    glm::vec3 rayOrigin = glm::vec3(invView[3]); // Translation column is camera position

    Ray ray;
    ray.origin = rayOrigin;
    ray.direction = rayDir;
    return ray;
}

bool Raycast::RaycastSphere(const Ray& ray, const Sphere& sphere, RaycastHit& outHit) {
    glm::vec3 oc = ray.origin - sphere.center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) return false;

    float sqrtD = std::sqrt(discriminant);
    float t0 = (-b - sqrtD) / (2.0f * a);
    float t1 = (-b + sqrtD) / (2.0f * a);

    float t = t0;
    if (t < 0.0f) {
        t = t1;
        if (t < 0.0f) return false;
    }

    outHit.hasHit = true;
    outHit.distance = t;
    outHit.point = ray.origin + t * ray.direction;
    outHit.normal = glm::normalize(outHit.point - sphere.center);
    return true;
}

bool Raycast::RaycastAABB(const Ray& ray, const AABB& aabb, RaycastHit& outHit) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i) {
        if (std::abs(ray.direction[i]) < 1e-6f) {
            // Ray is parallel to slab. Check if origin is inside slab
            if (ray.origin[i] < aabb.min[i] || ray.origin[i] > aabb.max[i]) {
                return false;
            }
        } else {
            float invD = 1.0f / ray.direction[i];
            float t0 = (aabb.min[i] - ray.origin[i]) * invD;
            float t1 = (aabb.max[i] - ray.origin[i]) * invD;

            if (t0 > t1) std::swap(t0, t1);

            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);

            if (tmin > tmax) return false;
        }
    }

    outHit.hasHit = true;
    outHit.distance = tmin;
    outHit.point = ray.origin + tmin * ray.direction;

    // Calculate normal based on the face hit
    glm::vec3 pc = outHit.point - (aabb.min + aabb.max) * 0.5f;
    glm::vec3 extents = (aabb.max - aabb.min) * 0.5f;
    glm::vec3 norm(0.0f);
    float minD = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i) {
        float d = std::abs(extents[i] - std::abs(pc[i]));
        if (d < minD) {
            minD = d;
            norm = glm::vec3(0.0f);
            norm[i] = (pc[i] > 0.0f) ? 1.0f : -1.0f;
        }
    }
    outHit.normal = norm;
    return true;
}

bool Raycast::RaycastCapsule(const Ray& ray, const Capsule& capsule, RaycastHit& outHit) {
    // Standard Capsule Raycast: intersect cylinder, and the two end-spheres.
    // Cylinder segment ends:
    glm::vec3 sa = capsule.center - glm::vec3(0.0f, capsule.halfHeight, 0.0f);
    glm::vec3 sb = capsule.center + glm::vec3(0.0f, capsule.halfHeight, 0.0f);

    float closestT = std::numeric_limits<float>::max();
    RaycastHit bestHit;

    // 1. Intersect end sphere A
    RaycastHit hitA;
    if (RaycastSphere(ray, Sphere{sa, capsule.radius}, hitA)) {
        if (hitA.distance < closestT) {
            closestT = hitA.distance;
            bestHit = hitA;
        }
    }

    // 2. Intersect end sphere B
    RaycastHit hitB;
    if (RaycastSphere(ray, Sphere{sb, capsule.radius}, hitB)) {
        if (hitB.distance < closestT) {
            closestT = hitB.distance;
            bestHit = hitB;
        }
    }

    // 3. Intersect cylinder body (along Y-axis segment)
    // Project ray onto XZ plane
    glm::vec2 rO(ray.origin.x, ray.origin.z);
    glm::vec2 rD(ray.direction.x, ray.direction.z);
    glm::vec2 cO(capsule.center.x, capsule.center.z);

    float a = glm::dot(rD, rD);
    if (a > 1e-6f) {
        glm::vec2 oc = rO - cO;
        float b = 2.0f * glm::dot(oc, rD);
        float c = glm::dot(oc, oc) - capsule.radius * capsule.radius;
        float disc = b * b - 4.0f * a * c;

        if (disc >= 0.0f) {
            float sqrtD = std::sqrt(disc);
            float t0 = (-b - sqrtD) / (2.0f * a);
            float t1 = (-b + sqrtD) / (2.0f * a);

            for (float t : {t0, t1}) {
                if (t >= 0.0f && t < closestT) {
                    glm::vec3 hitPoint = ray.origin + t * ray.direction;
                    // Check Y bounds
                    if (hitPoint.y >= sa.y && hitPoint.y <= sb.y) {
                        closestT = t;
                        bestHit.hasHit = true;
                        bestHit.distance = t;
                        bestHit.point = hitPoint;
                        bestHit.normal = glm::normalize(glm::vec3(hitPoint.x - capsule.center.x, 0.0f, hitPoint.z - capsule.center.z));
                    }
                }
            }
        }
    }

    if (bestHit.hasHit) {
        outHit = bestHit;
        return true;
    }
    return false;
}

bool Raycast::RaycastTerrain(const Ray& ray, float maxDistance, RaycastHit& outHit) {
    auto& tm = Terrain::TerrainManager::Get();

    // Raymarching search parameters
    float stepSize = 1.0f;
    float t = 0.0f;
    bool foundOverlap = false;

    glm::vec3 prevPoint = ray.origin;
    float prevHeight = ray.origin.y;

    while (t < maxDistance) {
        t += stepSize;
        glm::vec3 point = ray.origin + t * ray.direction;
        float height = tm.GetHeightAt(point.x, point.z);

        if (point.y < height) {
            foundOverlap = true;
            break;
        }
        prevPoint = point;
        prevHeight = height;
    }

    if (!foundOverlap) return false;

    // Binary search refinement for exact intersection point
    float t0 = t - stepSize;
    float t1 = t;
    glm::vec3 finalPoint(0.0f);

    for (int iter = 0; iter < 8; ++iter) {
        float mid = (t0 + t1) * 0.5f;
        glm::vec3 point = ray.origin + mid * ray.direction;
        float height = tm.GetHeightAt(point.x, point.z);

        if (point.y < height) {
            t1 = mid; // inside terrain
        } else {
            t0 = mid; // outside terrain
        }
        finalPoint = point;
    }

    outHit.hasHit = true;
    outHit.distance = (t0 + t1) * 0.5f;
    outHit.point = finalPoint;
    
    // Normal estimation using heightfield gradient
    float eps = 0.1f;
    float hL = tm.GetHeightAt(finalPoint.x - eps, finalPoint.z);
    float hR = tm.GetHeightAt(finalPoint.x + eps, finalPoint.z);
    float hD = tm.GetHeightAt(finalPoint.x, finalPoint.z - eps);
    float hU = tm.GetHeightAt(finalPoint.x, finalPoint.z + eps);
    outHit.normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));

    return true;
}

bool Raycast::RaycastWorld(ECS::Registry* registry, const Ray& ray, float maxDistance, 
                           RaycastHit& outHit, uint32_t ignoreEntity) {
    auto entities = registry->View<PhysicsComponent>();
    auto& sceneMgr = Scene::SceneManager::Get();

    float closestDist = maxDistance;
    bool hasHit = false;

    // Pre-cache scene graph entity positions
    std::unordered_map<ECS::Entity, glm::vec3> entityPositions;
    struct CachePositions {
        static void Run(Scene::SceneNode* node, std::unordered_map<ECS::Entity, glm::vec3>& outCache) {
            if (node->GetEntity() != ECS::NULL_ENTITY) {
                outCache[node->GetEntity()] = node->GetLocalPosition();
            }
            for (const auto& child : node->GetChildren()) {
                Run(child.get(), outCache);
            }
        }
    };
    if (sceneMgr.GetRootNode()) {
        CachePositions::Run(sceneMgr.GetRootNode(), entityPositions);
    }

    for (ECS::Entity entity : entities) {
        if (entity == ignoreEntity) continue;
        const auto& pc = registry->GetComponent<PhysicsComponent>(entity);
        if (pc.collider.type == ColliderType::None) continue;

        glm::vec3 pos = entityPositions.contains(entity) ? entityPositions[entity] : glm::vec3(0.0f);

        RaycastHit hit;
        bool result = false;

        if (pc.collider.type == ColliderType::Sphere) {
            if (auto* s = std::get_if<Sphere>(&pc.collider.shape)) {
                Sphere worldS = *s;
                worldS.center += pos;
                result = RaycastSphere(ray, worldS, hit);
            }
        } else if (pc.collider.type == ColliderType::AABB) {
            if (auto* a = std::get_if<AABB>(&pc.collider.shape)) {
                AABB worldA = *a;
                worldA.min += pos;
                worldA.max += pos;
                result = RaycastAABB(ray, worldA, hit);
            }
        } else if (pc.collider.type == ColliderType::Capsule) {
            if (auto* c = std::get_if<Capsule>(&pc.collider.shape)) {
                Capsule worldC = *c;
                worldC.center += pos;
                result = RaycastCapsule(ray, worldC, hit);
            }
        }

        if (result && hit.distance < closestDist) {
            closestDist = hit.distance;
            outHit = hit;
            outHit.entity = entity;
            hasHit = true;
        }
    }

    return hasHit;
}

} // namespace KumariEngine::Physics
