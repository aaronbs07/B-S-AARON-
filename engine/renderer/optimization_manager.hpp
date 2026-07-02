#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>
#include <volk.h>

namespace KumariEngine::Scene { class SceneNode; }
namespace KumariEngine::Camera { class Camera; }

namespace KumariEngine::Renderer {

// Bounding Volumes
struct BoundingBox {
    glm::vec3 min{-0.5f};
    glm::vec3 max{0.5f};
};

struct BoundingSphere {
    glm::vec3 center{0.0f};
    float radius{0.5f};
};

// Render object description for optimization manager
struct RenderObject {
    uint32_t id = 0;
    std::string name;
    std::string meshPath;
    std::string materialPath;
    glm::mat4 transform{1.0f};
    BoundingBox localAABB;
    BoundingSphere localSphere;
    bool isTransparent = false;
    bool visible = true;
    bool castShadows = true;

    // LOD parameters
    std::vector<std::string> lodMeshPaths; // Mesh path per LOD (LOD0, LOD1, LOD2)
    uint32_t lastLodLevel = 0;
};

// Configuration settings for Phase 4 optimizations
struct OptimizationSettings {
    bool lodEnabled = true;
    bool frustumCullingEnabled = true;
    bool occlusionCullingEnabled = true;
    bool gpuInstancingEnabled = true;
    bool indirectRenderingEnabled = true;
    bool renderQueueSortingEnabled = true;
    bool streamingOptimizationEnabled = true;
    bool lodVisualizationMode = false;

    float lodHysteresis = 2.0f; // distance cushion to prevent flickering
    float lodDistances[3] = { 15.0f, 35.0f, 75.0f }; // LOD thresholds
};

// Statistics gathered during culling & draw preparation
struct OptimizationStats {
    uint32_t totalObjects = 0;
    uint32_t visibleObjects = 0;
    uint32_t frustumCulled = 0;
    uint32_t occlusionCulled = 0;
    uint32_t instanceCount = 0;
    uint32_t drawCalls = 0;
    uint32_t batchedDrawCalls = 0;
    uint32_t indirectDrawCommands = 0;
    uint32_t lodCounts[4] = {0, 0, 0, 0}; // LOD0, LOD1, LOD2, Culled/LOD3

    float cpuTimingMs = 0.0f;
    float gpuTimingMs = 0.0f;
    size_t memoryUsageBytes = 0;
};

// Plane for Frustum Culling
struct FrustumPlane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f;

    FrustumPlane() = default;
    FrustumPlane(const glm::vec4& p) {
        float len = glm::length(glm::vec3(p));
        normal = glm::vec3(p) / len;
        d = p.w / len;
    }
};

struct CameraFrustum {
    FrustumPlane planes[6];
};

// Instancing Batch structure
struct InstancingBatch {
    std::string meshPath;
    std::string materialPath;
    uint32_t lodLevel = 0;
    std::vector<glm::mat4> transforms;
    std::vector<uint32_t> objectIds;
};

// Streamed resource lifetime entry
struct StreamedResourceEntry {
    std::string path;
    std::string type; // "mesh" or "texture"
    size_t sizeBytes = 0;
    double lastAccessTime = 0.0;
    bool loaded = false;
};

// Multi-Draw Indirect structures
struct DrawIndexedIndirectCommand {
    uint32_t indexCount = 0;
    uint32_t instanceCount = 0;
    uint32_t firstIndex = 0;
    int32_t  vertexOffset = 0;
    uint32_t firstInstance = 0;
};

class OptimizationManager {
public:
    OptimizationManager();
    ~OptimizationManager();

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    // Register / Update objects dynamically
    void RegisterObject(const RenderObject& obj);
    void UnregisterObject(uint32_t id);
    void UpdateObjectTransform(uint32_t id, const glm::mat4& transform);
    void ClearObjects();

    // Primary execution call (called each frame prior to PBR passes)
    void OptimizeFrame(const Camera::Camera* camera, double currentTime, float deltaTime);

    // Getters / Setters
    OptimizationSettings& GetSettings() { return m_settings; }
    const OptimizationStats& GetStats() const { return m_stats; }
    const std::vector<InstancingBatch>& GetActiveBatches() const { return m_activeBatches; }
    const std::vector<DrawIndexedIndirectCommand>& GetIndirectCommands() const { return m_indirectCommands; }
    
    // HZB preparation and occlusion checks helper
    void PrepareHZB(VkCommandBuffer cmd, VkImage srcDepthImage, VkExtent2D extent);
    bool CheckOcclusion(const BoundingBox& worldAABB, const BoundingSphere& worldSphere) const;

    // Resource Streaming callbacks
    void RequestBackgroundAssetLoad(const std::string& path, const std::string& type, size_t size);
    void UpdateStreaming(double currentTime);
    size_t GetStreamingMemoryUsage() const;

private:
    void PerformFrustumCulling(const CameraFrustum& frustum, const glm::vec3& cameraPos);
    void PerformOcclusionCulling();
    void PerformLODSelection(const glm::vec3& cameraPos);
    void CompileInstancingBatches();
    void GenerateIndirectCommands();
    void SortRenderQueues(const glm::vec3& cameraPos);

    // Bounding volumes helpers
    BoundingBox GetWorldAABB(const RenderObject& obj) const;
    BoundingSphere GetWorldSphere(const RenderObject& obj) const;
    CameraFrustum ExtractFrustumPlanes(const glm::mat4& viewProj) const;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    OptimizationSettings m_settings;
    OptimizationStats m_stats;

    std::unordered_map<uint32_t, RenderObject> m_objects;
    std::vector<RenderObject> m_visibleObjects;
    std::vector<InstancingBatch> m_activeBatches;
    std::vector<DrawIndexedIndirectCommand> m_indirectCommands;

    // HZB Vulkan resources simulation
    VkImage m_hzbImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hzbMemory = VK_NULL_HANDLE;
    std::vector<VkImageView> m_hzbImageViews;
    VkSampler m_hzbSampler = VK_NULL_HANDLE;
    VkExtent2D m_hzbExtent{0, 0};
    uint32_t m_hzbMipLevels = 1;

    // Streaming resources map
    std::unordered_map<std::string, StreamedResourceEntry> m_streamedResources;
    double m_resourceUnloadDelay = 5.0; // Unload if unused for 5 seconds
    double m_lastFrameTime = 0.0;
};

} // namespace KumariEngine::Renderer
