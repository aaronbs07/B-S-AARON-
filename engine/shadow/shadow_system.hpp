#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <volk.h>

namespace KumariEngine::Renderer {

struct ShadowAtlasRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool isAllocated = false;
};

struct ShadowCacheEntry {
    uint64_t lightGuid = 0;
    bool isStatic = true;
    bool isDirty = false;
    uint32_t lastFrameRendered = 0;
};

struct CascadeDetails {
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewProjMatrix;
    float splitDepth = 0.0f;
};

class ShadowSystem {
public:
    ShadowSystem();
    ~ShadowSystem() = default;

    void Initialize(uint32_t atlasWidth, uint32_t atlasHeight);
    void Shutdown();

    void SetCascadeCount(uint32_t count);
    uint32_t GetCascadeCount() const { return m_cascadeCount; }
    void GenerateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj, const glm::vec3& lightDir, float nearClip, float farClip);
    const std::vector<CascadeDetails>& GetCascades() const { return m_cascades; }

    glm::mat4 ApplyStableSnapping(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, float cascadeResolution);

    float CalculateSlopeScaledBias(float baseBias, float slopeFactor, float slope);
    glm::vec3 ApplyNormalOffsetBias(const glm::vec3& position, const glm::vec3& normal, float normalBias);

    bool AllocateAtlasRect(uint32_t width, uint32_t height, ShadowAtlasRect& outRect);
    void FreeAtlasRect(const ShadowAtlasRect& rect);
    void ClearAtlasAllocations();

    bool IsLightVisible(const glm::vec3& lightPos, float lightRadius, const glm::vec4 frustumPlanes[6]) const;

    bool UpdateShadowCache(uint64_t lightGuid, bool isLightStatic, bool isSceneStatic, uint32_t frameIndex);
    void InvalidateCacheEntry(uint64_t lightGuid);

    uint32_t CalculateDynamicResolution(float distanceToCamera, float maxDistance, uint32_t baseResolution);

    void SetCascadeVisualization(bool enabled) { m_cascadeVisualizationEnabled = enabled; }
    bool IsCascadeVisualizationEnabled() const { return m_cascadeVisualizationEnabled; }
    void RecordShadowDrawCall() { m_stats.shadowDrawCalls++; }
    void AddMemoryUsage(size_t bytes) { m_stats.gpuMemoryBytes += bytes; }
    void SetActiveShadowCastingLights(uint32_t count) { m_stats.activeShadowCastingLights = count; }
    
    struct ShadowStats {
        uint32_t activeShadowCastingLights = 0;
        uint32_t shadowDrawCalls = 0;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        size_t gpuMemoryBytes = 0;
    };
    const ShadowStats& GetStats() const { return m_stats; }
    void ResetStats();

private:
    uint32_t m_atlasWidth = 4096;
    uint32_t m_atlasHeight = 4096;
    uint32_t m_cascadeCount = 4;
    std::vector<CascadeDetails> m_cascades;
    bool m_cascadeVisualizationEnabled = false;

    std::vector<ShadowAtlasRect> m_atlasGrid;
    std::unordered_map<uint64_t, ShadowCacheEntry> m_shadowCache;

    ShadowStats m_stats;
};

} // namespace KumariEngine::Renderer
