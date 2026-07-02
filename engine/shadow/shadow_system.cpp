#include "shadow_system.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace KumariEngine::Renderer {

ShadowSystem::ShadowSystem() {
    ResetStats();
}

void ShadowSystem::Initialize(uint32_t atlasWidth, uint32_t atlasHeight) {
    m_atlasWidth = atlasWidth;
    m_atlasHeight = atlasHeight;
    ClearAtlasAllocations();
}

void ShadowSystem::Shutdown() {
    m_atlasGrid.clear();
    m_shadowCache.clear();
}

void ShadowSystem::SetCascadeCount(uint32_t count) {
    m_cascadeCount = std::clamp(count, 1u, 4u);
}

void ShadowSystem::GenerateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj, const glm::vec3& lightDir, float nearClip, float farClip) {
    m_cascades.resize(m_cascadeCount);

    // 1. Calculate split depths (using a logarithmic and uniform split blend)
    float splitBlend = 0.5f;
    std::vector<float> splits(m_cascadeCount + 1);
    splits[0] = nearClip;
    splits[m_cascadeCount] = farClip;

    for (uint32_t i = 1; i < m_cascadeCount; ++i) {
        float r = static_cast<float>(i) / static_cast<float>(m_cascadeCount);
        float logSplit = nearClip * std::pow(farClip / nearClip, r);
        float uniformSplit = nearClip + (farClip - nearClip) * r;
        splits[i] = splitBlend * logSplit + (1.0f - splitBlend) * uniformSplit;
    }

    // 2. Extract camera properties from view and projection matrices
    glm::mat4 invView = glm::inverse(cameraView);
    glm::vec3 right = glm::vec3(invView[0]);
    glm::vec3 up = glm::vec3(invView[1]);
    glm::vec3 forward = -glm::vec3(invView[2]); // Camera looks down -Z
    glm::vec3 cameraPos = glm::vec3(invView[3]);

    float tanHalfFovy = 1.0f / cameraProj[1][1];
    float aspect = cameraProj[1][1] / cameraProj[0][0];

    // Normalized light direction
    glm::vec3 nLightDir = glm::normalize(lightDir);
    glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(nLightDir, lightUp)) > 0.99f) {
        lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // 3. Construct view projection for each cascade
    for (uint32_t i = 0; i < m_cascadeCount; ++i) {
        float nearD = splits[i];
        float farD = splits[i + 1];

        // Frustum corners in camera space
        float nearH = nearD * tanHalfFovy;
        float nearW = nearH * aspect;
        float farH = farD * tanHalfFovy;
        float farW = farH * aspect;

        std::vector<glm::vec3> corners = {
            cameraPos + forward * nearD + right * nearW + up * nearH,
            cameraPos + forward * nearD - right * nearW + up * nearH,
            cameraPos + forward * nearD + right * nearW - up * nearH,
            cameraPos + forward * nearD - right * nearW - up * nearH,
            cameraPos + forward * farD + right * farW + up * farH,
            cameraPos + forward * farD - right * farW + up * farH,
            cameraPos + forward * farD + right * farW - up * farH,
            cameraPos + forward * farD - right * farW - up * farH
        };

        // Center of bounding sphere
        glm::vec3 center(0.0f);
        for (const auto& corner : corners) {
            center += corner;
        }
        center /= 8.0f;

        // Radius of bounding sphere
        float radius = 0.0f;
        for (const auto& corner : corners) {
            radius = std::max(radius, glm::distance(center, corner));
        }
        // Round up radius to avoid jittering
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Light view-projection matrices
        glm::vec3 lightPos = center - nLightDir * radius;
        glm::mat4 lView = glm::lookAt(lightPos, center, lightUp);
        glm::mat4 lProj = glm::ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius);

        // Apply stable snapping
        m_cascades[i].viewMatrix = ApplyStableSnapping(lView, lProj, 2048.0f); // Resolution: 2048
        m_cascades[i].projectionMatrix = lProj;
        m_cascades[i].viewProjMatrix = m_cascades[i].projectionMatrix * m_cascades[i].viewMatrix;
        m_cascades[i].splitDepth = farD;
    }
}

glm::mat4 ShadowSystem::ApplyStableSnapping(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, float cascadeResolution) {
    // 1. Snapping projection origin in light space to texel steps
    // Extract orthographic bounds from projection matrix
    float r_plus_l_div_r_minus_l = projMatrix[3][0];
    float t_plus_b_div_t_minus_b = projMatrix[3][1];
    (void)r_plus_l_div_r_minus_l;
    (void)t_plus_b_div_t_minus_b;

    float r_minus_l = 2.0f / projMatrix[0][0];
    float radius = r_minus_l * 0.5f;

    // View translation vector
    glm::vec3 translation = glm::vec3(viewMatrix[3]);
    float texelSize = (2.0f * radius) / cascadeResolution;

    // Snapping view matrix translation coordinates
    translation.x = std::floor(translation.x / texelSize) * texelSize;
    translation.y = std::floor(translation.y / texelSize) * texelSize;

    glm::mat4 snappedView = viewMatrix;
    snappedView[3] = glm::vec4(translation, 1.0f);

    return snappedView;
}

float ShadowSystem::CalculateSlopeScaledBias(float baseBias, float slopeFactor, float slope) {
    return baseBias + slopeFactor * slope;
}

glm::vec3 ShadowSystem::ApplyNormalOffsetBias(const glm::vec3& position, const glm::vec3& normal, float normalBias) {
    return position + glm::normalize(normal) * normalBias;
}

bool ShadowSystem::AllocateAtlasRect(uint32_t width, uint32_t height, ShadowAtlasRect& outRect) {
    // Basic structural quad-partitioning/grid allocation
    // Divide atlas into slots of size width x height
    for (auto& rect : m_atlasGrid) {
        if (!rect.isAllocated && rect.width == width && rect.height == height) {
            rect.isAllocated = true;
            outRect = rect;
            return true;
        }
    }

    // Try dynamic placement along grid cells
    uint32_t gridRows = m_atlasHeight / height;
    uint32_t gridCols = m_atlasWidth / width;
    
    for (uint32_t r = 0; r < gridRows; ++r) {
        for (uint32_t c = 0; c < gridCols; ++c) {
            uint32_t posX = c * width;
            uint32_t posY = r * height;

            // Check overlap
            bool overlap = false;
            for (const auto& existing : m_atlasGrid) {
                if (existing.isAllocated) {
                    bool xOverlap = (posX < existing.x + existing.width) && (posX + width > existing.x);
                    bool yOverlap = (posY < existing.y + existing.height) && (posY + height > existing.y);
                    if (xOverlap && yOverlap) {
                        overlap = true;
                        break;
                    }
                }
            }

            if (!overlap) {
                ShadowAtlasRect rect{};
                rect.x = posX;
                rect.y = posY;
                rect.width = width;
                rect.height = height;
                rect.isAllocated = true;
                m_atlasGrid.push_back(rect);
                outRect = rect;
                return true;
            }
        }
    }

    return false;
}

void ShadowSystem::FreeAtlasRect(const ShadowAtlasRect& rect) {
    for (auto& slot : m_atlasGrid) {
        if (slot.x == rect.x && slot.y == rect.y && slot.width == rect.width && slot.height == rect.height) {
            slot.isAllocated = false;
            break;
        }
    }
}

void ShadowSystem::ClearAtlasAllocations() {
    m_atlasGrid.clear();
}

bool ShadowSystem::IsLightVisible(const glm::vec3& lightPos, float lightRadius, const glm::vec4 frustumPlanes[6]) const {
    // Sphere vs Frustum Plane culling
    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(frustumPlanes[i]), lightPos) + frustumPlanes[i].w;
        if (distance < -lightRadius) {
            return false; // Culled
        }
    }
    return true; // Visible
}

bool ShadowSystem::UpdateShadowCache(uint64_t lightGuid, bool isLightStatic, bool isSceneStatic, uint32_t frameIndex) {
    auto it = m_shadowCache.find(lightGuid);
    if (it != m_shadowCache.end()) {
        bool cacheValid = isLightStatic && isSceneStatic && !it->second.isDirty;
        it->second.lastFrameRendered = frameIndex;
        if (cacheValid) {
            m_stats.cacheHits++;
            return true; // Cache HIT
        } else {
            it->second.isDirty = false; // Reset dirty flag as we are re-rendering
            m_stats.cacheMisses++;
            return false; // Cache MISS
        }
    }

    ShadowCacheEntry entry{};
    entry.lightGuid = lightGuid;
    entry.isStatic = isLightStatic;
    entry.isDirty = false;
    entry.lastFrameRendered = frameIndex;
    m_shadowCache[lightGuid] = entry;
    
    m_stats.cacheMisses++;
    return false; // Cache MISS
}

void ShadowSystem::InvalidateCacheEntry(uint64_t lightGuid) {
    auto it = m_shadowCache.find(lightGuid);
    if (it != m_shadowCache.end()) {
        it->second.isDirty = true;
    }
}

uint32_t ShadowSystem::CalculateDynamicResolution(float distanceToCamera, float maxDistance, uint32_t baseResolution) {
    float factor = glm::clamp(distanceToCamera / maxDistance, 0.0f, 1.0f);
    uint32_t resolution = static_cast<uint32_t>(glm::mix(static_cast<float>(baseResolution), static_cast<float>(baseResolution) / 4.0f, factor));
    
    // Scale to power-of-two boundaries
    if (resolution > 512) return 1024;
    if (resolution > 256) return 512;
    return 256;
}

void ShadowSystem::ResetStats() {
    m_stats.activeShadowCastingLights = 0;
    m_stats.shadowDrawCalls = 0;
    m_stats.cacheHits = 0;
    m_stats.cacheMisses = 0;
    m_stats.gpuMemoryBytes = 0;
}

} // namespace KumariEngine::Renderer
