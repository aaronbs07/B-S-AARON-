#pragma once
#include <unordered_map>
#include <memory>
#include <future>
#include <volk.h>
#include <glm/glm.hpp>
#include "terrain_chunk.hpp"
#include "noise_generator.hpp"
#include "vegetation_system.hpp"

namespace KumariEngine::Terrain {

class TerrainManager {
public:
    static TerrainManager& Get() {
        static TerrainManager instance;
        return instance;
    }

    TerrainManager(const TerrainManager&) = delete;
    TerrainManager& operator=(const TerrainManager&) = delete;

    void Initialize(uint32_t seed, float chunkSize);
    void Shutdown(VkDevice device);

    void Update(const glm::vec3& viewerPos, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);

    // Getters
    const std::unordered_map<ChunkCoord, std::shared_ptr<TerrainChunk>, ChunkCoordHash>& GetActiveChunks() const { return m_activeChunks; }
    size_t GetLoadedChunkCount() const { return m_activeChunks.size(); }
    size_t GetLoadingQueueSize() const { return m_loadingTasks.size(); }
    float GetAverageGenTime() const { return m_avgGenTimeMs; }
    size_t GetCacheHitCount() const { return m_cacheHits; }
    size_t GetCacheMissCount() const { return m_cacheMisses; }
    size_t GetMemoryEstimate() const;
    size_t GetStitchingRebuildCount() const { return m_stitchingRebuilds; }
    size_t GetStitchingUploadCount() const { return m_stitchingUploads; }
    void IncrementStitchingRebuilds() { m_stitchingRebuilds++; }
    void IncrementStitchingUploads() { m_stitchingUploads++; }
    void ResetStitchingTelemetry() { m_stitchingRebuilds = 0; m_stitchingUploads = 0; }

    float GetLastStreamingTimeMs() const { return m_lastStreamingTimeMs; }
    float GetLastGPUUploadTimeMs() const { return m_lastGPUUploadTimeMs; }
    void SetCacheSize(size_t size) { m_maxCacheSize = size; }

    float GetHeightAt(float x, float z) const;
    int GetBiomeAt(float x, float z) const;

    // Toggles
    void ToggleWireframe() { m_wireframe = !m_wireframe; }
    bool IsWireframe() const { return m_wireframe; }

    void ToggleChunkBorders() { m_chunkBorders = !m_chunkBorders; }
    bool IsChunkBordersEnabled() const { return m_chunkBorders; }

    void ToggleDebugVis() { m_debugVis = !m_debugVis; }
    bool IsDebugVisEnabled() const { return m_debugVis; }

    void SetLoadRadii(int loadRad, int unloadRad) {
        m_loadRadius = loadRad;
        m_unloadRadius = unloadRad;
    }

    float GetChunkSize() const { return m_chunkSize; }
    uint32_t GetSeed() const { return m_seed; }

    // Telemetry stats
    void SetRenderStats(size_t visibleChunks, size_t drawCalls, size_t triangleCount) {
        m_visibleChunksCount = visibleChunks;
        m_drawCallsCount = drawCalls;
        m_renderedTrianglesCount = triangleCount;
    }
    size_t GetVisibleChunksCount() const { return m_visibleChunksCount; }
    size_t GetDrawCallsCount() const { return m_drawCallsCount; }
    size_t GetRenderedTrianglesCount() const { return m_renderedTrianglesCount; }

    // Future compatibility structures
    struct RiverData {
        std::vector<glm::vec3> splinePoints;
        float width = 4.0f;
        float depth = 2.0f;
    };
    struct RoadData {
        std::vector<glm::vec3> splinePoints;
        float width = 3.0f;
        int roadType = 0; // 0: dirt, 1: paved
    };
    struct SettlementPlot {
        glm::vec3 position;
        float radius = 50.0f;
        std::string name;
        bool isCity = false;
    };
    struct CaveNode {
        glm::vec3 center;
        float radius = 5.0f;
    };

    // Editing & Save/Load interfaces
    void ApplyHeightEdit(float worldX, float worldZ, float radius, float strength);
    void ClearEdits();
    bool SaveTerrainEdits(const std::string& filepath) const;
    bool LoadTerrainEdits(const std::string& filepath);

private:
    TerrainManager() = default;
    ~TerrainManager() = default;

    int CalculateLOD(const ChunkCoord& coord, const glm::vec3& viewerPos) const;
    void UpdateStitching(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);

    uint32_t m_seed = 1337;
    float m_chunkSize = 64.0f;
    int m_loadRadius = 2;
    int m_unloadRadius = 3;

    NoiseGenerator m_noiseGen;
    VegetationSystem m_vegetationSystem;

    std::unordered_map<ChunkCoord, std::shared_ptr<TerrainChunk>, ChunkCoordHash> m_activeChunks;
    std::unordered_map<ChunkCoord, std::future<std::shared_ptr<TerrainChunk>>, ChunkCoordHash> m_loadingTasks;

    // Cache of inactive generated chunks on CPU
    std::unordered_map<ChunkCoord, std::shared_ptr<TerrainChunk>, ChunkCoordHash> m_cache;
    size_t m_maxCacheSize = 256;

    std::vector<StagingResources> m_pendingTransfers;

    // Telemetry
    float m_avgGenTimeMs = 0.0f;
    size_t m_totalGenCount = 0;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
    float m_lastStreamingTimeMs = 0.0f;
    float m_lastGPUUploadTimeMs = 0.0f;
    size_t m_stitchingRebuilds = 0;
    size_t m_stitchingUploads = 0;

    bool m_wireframe = false;
    bool m_chunkBorders = false;
    bool m_debugVis = false;

    // Telemetry fields
    size_t m_visibleChunksCount = 0;
    size_t m_drawCallsCount = 0;
    size_t m_renderedTrianglesCount = 0;

    // Future compatibility collections
    std::vector<RiverData> m_rivers;
    std::vector<RoadData> m_roads;
    std::vector<SettlementPlot> m_settlements;
    std::vector<CaveNode> m_caves;
    std::unordered_map<uint64_t, float> m_heightEdits;
};

} // namespace KumariEngine::Terrain
