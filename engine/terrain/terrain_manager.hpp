#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
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

    glm::vec4 GetLayerWeightsAt(float x, float z) const;

    // Brush Tool Types
    enum class BrushType {
        RaiseLower,
        Smooth,
        Flatten,
        Noise,
        Erosion
    };

    // Brush paint commands
    void ApplyBrush(float worldX, float worldZ, BrushType type, float radius, float strength, float deltaTime, float targetHeight = 0.0f);
    void ApplyTexturePaint(float worldX, float worldZ, int targetLayer, float radius, float strength, float deltaTime);
    void ApplyVegetationPaint(float worldX, float worldZ, int vegType, float radius, float density, float minScale, float maxScale, bool eraseMode);

    // Spline-based Road/River creations
    void CreateRoad(const std::vector<glm::vec3>& splinePoints, float width, int roadType);
    void CreateRiver(const std::vector<glm::vec3>& splinePoints, float width, float depth);
    void ClearSplines();
    void ApplySplines();

    // Streaming boundary rendering
    void RenderDebugVisualizations(VkCommandBuffer cmdBuf, const glm::mat4& viewProj);

    // Accessors for serialization and undo/redo
    const std::unordered_map<uint64_t, float>& GetHeightEdits() const { return m_heightEdits; }
    void SetHeightEdits(const std::unordered_map<uint64_t, float>& edits) { m_heightEdits = edits; }

    const std::unordered_map<uint64_t, glm::vec4>& GetLayerEdits() const { return m_layerEdits; }
    void SetLayerEdits(const std::unordered_map<uint64_t, glm::vec4>& edits) { m_layerEdits = edits; }

    const std::unordered_set<ChunkCoord, ChunkCoordHash>& GetEditedVegetationChunks() const { return m_editedVegetationChunks; }
    void SetEditedVegetationChunks(const std::unordered_set<ChunkCoord, ChunkCoordHash>& chunks) { m_editedVegetationChunks = chunks; }

    const std::unordered_map<ChunkCoord, std::vector<VegetationInstance>, ChunkCoordHash>& GetPaintedVegetation() const { return m_paintedVegetation; }
    void SetPaintedVegetation(const std::unordered_map<ChunkCoord, std::vector<VegetationInstance>, ChunkCoordHash>& veg) { m_paintedVegetation = veg; }

    const std::vector<RoadData>& GetRoads() const { return m_roads; }
    void SetRoads(const std::vector<RoadData>& roads) { m_roads = roads; }

    const std::vector<RiverData>& GetRivers() const { return m_rivers; }
    void SetRivers(const std::vector<RiverData>& rivers) { m_rivers = rivers; }
    void PopulateChunkVegetation(TerrainChunk* chunk) const { m_vegetationSystem.PopulateVegetation(chunk); }
    void RegenerateActiveChunks();

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
    std::unordered_map<uint64_t, glm::vec4> m_layerEdits;
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_editedVegetationChunks;
    std::unordered_map<ChunkCoord, std::vector<VegetationInstance>, ChunkCoordHash> m_paintedVegetation;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
};

} // namespace KumariEngine::Terrain
