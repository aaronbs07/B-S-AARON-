#pragma once
#include <vector>
#include <memory>
#include <volk.h>
#include <glm/glm.hpp>
#include "noise_generator.hpp"

namespace KumariEngine::Terrain {

struct TerrainVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 biomeWeights;
};

struct CollisionMesh {
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
};

struct VegetationInstance {
    int type; // 0: Tree, 1: Grass, 2: Bush, 3: Rock
    glm::vec3 position; // World space
    float scale;
    float rotation;
};

struct ChunkCoord {
    int32_t x;
    int32_t z;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const {
        return (static_cast<std::size_t>(c.x) * 397) ^ static_cast<std::size_t>(c.z);
    }
};

struct StagingResources {
    VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingIndexBufferMemory = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};

class TerrainChunk {
public:
    TerrainChunk(int32_t chunkX, int32_t chunkZ, float chunkSize, const NoiseGenerator* noiseGen);
    ~TerrainChunk();

    // Generate mesh data on CPU (thread-safe, can run on background thread)
    void GenerateCPUData(int lod);

    // Upload CPU data to Vulkan GPU buffers (must run on main thread)
    bool UploadToGPU(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, StagingResources& outResources);

    // Rebuild index buffer when neighbor LODs change to stitch cracks
    bool RebuildIndicesForStitching(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                    int lodNorth, int lodSouth, int lodEast, int lodWest, StagingResources& outResources);

    // Release GPU buffers
    void DestroyGPUResources(VkDevice device);

    // Getters
    ChunkCoord GetCoord() const { return m_coord; }
    int GetLOD() const { return m_lod; }
    float GetSize() const { return m_chunkSize; }
    
    VkBuffer GetVertexBuffer() const { return m_vertexBuffer; }
    VkBuffer GetIndexBuffer() const { return m_indexBuffer; }
    uint32_t GetIndexCount() const { return m_indexCount; }

    const NoiseGenerator* GetNoiseGenerator() const { return m_noiseGen; }
    const std::vector<TerrainVertex>& GetVertices() const { return m_vertices; }
    const CollisionMesh& GetCollisionMesh() const { return m_collisionMesh; }
    const std::vector<VegetationInstance>& GetVegetation() const { return m_vegetation; }
    std::vector<VegetationInstance>& GetVegetation() { return m_vegetation; }

    enum class ChunkState {
        Unloaded,
        Loading,
        Loaded,
        Visible,
        Unloading
    };

    void SetState(ChunkState state) { m_state = state; }
    ChunkState GetState() const { return m_state; }

    const std::vector<float>& GetHeightmap() const { return m_heightmap; }
    const std::vector<glm::vec3>& GetNormalMap() const { return m_normalMap; }
    int GetPrimaryBiomeID() const { return m_primaryBiomeID; }

    // Bounding Box check for frustum culling
    glm::vec3 GetMinBounds() const { return m_minBounds; }
    glm::vec3 GetMaxBounds() const { return m_maxBounds; }

private:
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    ChunkCoord m_coord;
    float m_chunkSize;
    const NoiseGenerator* m_noiseGen = nullptr;
    int m_lod = 0;

    std::vector<TerrainVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    CollisionMesh m_collisionMesh;
    std::vector<VegetationInstance> m_vegetation;

    glm::vec3 m_minBounds{0.0f};
    glm::vec3 m_maxBounds{0.0f};

    // Neighbor LOD cache
    int m_lodNorth = -1;
    int m_lodSouth = -1;
    int m_lodEast = -1;
    int m_lodWest = -1;

    // Vulkan Resources
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_indexCount = 0;

    ChunkState m_state = ChunkState::Unloaded;
    std::vector<float> m_heightmap;
    std::vector<glm::vec3> m_normalMap;
    int m_primaryBiomeID = 2;

    bool m_uploaded = false;
    VkDevice m_device = VK_NULL_HANDLE;
};

} // namespace KumariEngine::Terrain
