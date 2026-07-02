#include "terrain_chunk.hpp"
#include "terrain_manager.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace KumariEngine::Terrain {

TerrainChunk::TerrainChunk(int32_t chunkX, int32_t chunkZ, float chunkSize, const NoiseGenerator* noiseGen)
    : m_coord{chunkX, chunkZ}, m_chunkSize{chunkSize}, m_noiseGen{noiseGen} {
}

TerrainChunk::~TerrainChunk() {
    if (m_device != VK_NULL_HANDLE) {
        DestroyGPUResources(m_device);
    }
}

void TerrainChunk::GenerateCPUData(int lod) {
    m_lod = lod;
    m_vertices.clear();
    m_indices.clear();

    const int gridMax = 64;
    int step = 1 << m_lod;
    int gridWidth = (gridMax / step) + 1;

    m_vertices.reserve(gridWidth * gridWidth);
    m_heightmap.clear();
    m_heightmap.reserve(gridWidth * gridWidth);
    m_normalMap.clear();
    m_normalMap.reserve(gridWidth * gridWidth);

    m_minBounds = glm::vec3(std::numeric_limits<float>::max());
    m_maxBounds = glm::vec3(-std::numeric_limits<float>::max());

    float startX = m_coord.x * m_chunkSize;
    float startZ = m_coord.z * m_chunkSize;

    // Calculate primary biome at chunk center
    float centerX = startX + m_chunkSize * 0.5f;
    float centerZ = startZ + m_chunkSize * 0.5f;
    float centerH = TerrainManager::Get().GetHeightAt(centerX, centerZ);
    m_primaryBiomeID = m_noiseGen->GetBiome(centerX, centerZ, centerH);

    // 1. Generate vertices
    for (int gz = 0; gz <= gridMax; gz += step) {
        float zPct = static_cast<float>(gz) / static_cast<float>(gridMax);
        float worldZ = startZ + zPct * m_chunkSize;

        for (int gx = 0; gx <= gridMax; gx += step) {
            float xPct = static_cast<float>(gx) / static_cast<float>(gridMax);
            float worldX = startX + xPct * m_chunkSize;

            float height = TerrainManager::Get().GetHeightAt(worldX, worldZ);
            m_heightmap.push_back(height);

            // Bounding box updates
            m_minBounds = glm::min(m_minBounds, glm::vec3(worldX, height, worldZ));
            m_maxBounds = glm::max(m_maxBounds, glm::vec3(worldX, height, worldZ));

            // Estimate normal using central difference
            float eps = 0.5f;
            float hL = TerrainManager::Get().GetHeightAt(worldX - eps, worldZ);
            float hR = TerrainManager::Get().GetHeightAt(worldX + eps, worldZ);
            float hD = TerrainManager::Get().GetHeightAt(worldX, worldZ - eps);
            float hU = TerrainManager::Get().GetHeightAt(worldX, worldZ + eps);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
            m_normalMap.push_back(normal);

            // UVs
            glm::vec2 uv(xPct, zPct);

            // Biome weights
            glm::vec4 biomeWeights = TerrainManager::Get().GetLayerWeightsAt(worldX, worldZ);

            TerrainVertex vertex;
            vertex.pos = glm::vec3(worldX, height, worldZ);
            vertex.normal = normal;
            vertex.uv = uv;
            vertex.biomeWeights = biomeWeights;

            m_vertices.push_back(vertex);
        }
    }

    // 2. Generate standard indices (which will be overwritten by stitching if needed)
    for (int cz = 0; cz < gridWidth - 1; ++cz) {
        for (int cx = 0; cx < gridWidth - 1; ++cx) {
            uint32_t i0 = cz * gridWidth + cx;
            uint32_t i1 = cz * gridWidth + (cx + 1);
            uint32_t i2 = (cz + 1) * gridWidth + cx;
            uint32_t i3 = (cz + 1) * gridWidth + (cx + 1);

            // CW winding
            m_indices.push_back(i0);
            m_indices.push_back(i2);
            m_indices.push_back(i1);

            m_indices.push_back(i1);
            m_indices.push_back(i2);
            m_indices.push_back(i3);
        }
    }
    m_indexCount = static_cast<uint32_t>(m_indices.size());

    // 3. Generate Collision Mesh (at LOD 1 equivalent or LOD 2 to keep it small)
    m_collisionMesh.vertices.clear();
    m_collisionMesh.indices.clear();
    int colStep = 4; // LOD 2 equivalent
    int colWidth = (gridMax / colStep) + 1;
    m_collisionMesh.vertices.reserve(colWidth * colWidth);
    for (int cz = 0; cz <= gridMax; cz += colStep) {
        float zPct = static_cast<float>(cz) / static_cast<float>(gridMax);
        float worldZ = startZ + zPct * m_chunkSize;
        for (int cx = 0; cx <= gridMax; cx += colStep) {
            float xPct = static_cast<float>(cx) / static_cast<float>(gridMax);
            float worldX = startX + xPct * m_chunkSize;
            float height = TerrainManager::Get().GetHeightAt(worldX, worldZ);
            m_collisionMesh.vertices.push_back(glm::vec3(worldX, height, worldZ));
        }
    }
    for (int cz = 0; cz < colWidth - 1; ++cz) {
        for (int cx = 0; cx < colWidth - 1; ++cx) {
            uint32_t i0 = cz * colWidth + cx;
            uint32_t i1 = cz * colWidth + (cx + 1);
            uint32_t i2 = (cz + 1) * colWidth + cx;
            uint32_t i3 = (cz + 1) * colWidth + (cx + 1);

            m_collisionMesh.indices.push_back(i0);
            m_collisionMesh.indices.push_back(i2);
            m_collisionMesh.indices.push_back(i1);

            m_collisionMesh.indices.push_back(i1);
            m_collisionMesh.indices.push_back(i2);
            m_collisionMesh.indices.push_back(i3);
        }
    }
}

bool TerrainChunk::RebuildIndicesForStitching(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                            int lodNorth, int lodSouth, int lodEast, int lodWest, StagingResources& outResources) {
    if (lodNorth == m_lodNorth && lodSouth == m_lodSouth && lodEast == m_lodEast && lodWest == m_lodWest) {
        if (device == VK_NULL_HANDLE || m_indexBuffer != VK_NULL_HANDLE) {
            return true; // Already up-to-date
        }
    }

    TerrainManager::Get().IncrementStitchingRebuilds();

    m_lodNorth = lodNorth;
    m_lodSouth = lodSouth;
    m_lodEast = lodEast;
    m_lodWest = lodWest;

    const int gridMax = 64;
    int step = 1 << m_lod;
    int gridWidth = (gridMax / step) + 1;

    std::vector<uint32_t> newIndices;
    std::vector<bool> processed(gridWidth * gridWidth, false);

    auto markProcessed = [&](int cx, int cz) {
        if (cx >= 0 && cx < gridWidth && cz >= 0 && cz < gridWidth) {
            processed[cz * gridWidth + cx] = true;
        }
    };

    auto isProcessed = [&](int cx, int cz) -> bool {
        if (cx >= 0 && cx < gridWidth && cz >= 0 && cz < gridWidth) {
            return static_cast<bool>(processed[cz * gridWidth + cx]);
        }
        return true;
    };

    // 1. North boundary (cz == gridWidth - 2)
    if (lodNorth > m_lod) {
        int cz = gridWidth - 2;
        for (int cx = 0; cx < gridWidth - 1; cx += 2) {
            uint32_t A = cx + cz * gridWidth;
            uint32_t B = (cx + 1) + cz * gridWidth;
            uint32_t C = (cx + 2) + cz * gridWidth;
            uint32_t D = cx + (cz + 1) * gridWidth;
            uint32_t E = (cx + 2) + (cz + 1) * gridWidth;

            // CW winding
            newIndices.push_back(A); newIndices.push_back(D); newIndices.push_back(B);
            newIndices.push_back(B); newIndices.push_back(D); newIndices.push_back(E);
            newIndices.push_back(B); newIndices.push_back(E); newIndices.push_back(C);

            markProcessed(cx, cz);
            markProcessed(cx + 1, cz);
        }
    }

    // 2. South boundary (cz == 0)
    if (lodSouth > m_lod) {
        int cz = 0;
        for (int cx = 0; cx < gridWidth - 1; cx += 2) {
            uint32_t A = cx + cz * gridWidth;
            uint32_t B = (cx + 2) + cz * gridWidth;
            uint32_t C = cx + (cz + 1) * gridWidth;
            uint32_t D = (cx + 1) + (cz + 1) * gridWidth;
            uint32_t E = (cx + 2) + (cz + 1) * gridWidth;

            newIndices.push_back(A); newIndices.push_back(C); newIndices.push_back(D);
            newIndices.push_back(A); newIndices.push_back(D); newIndices.push_back(B);
            newIndices.push_back(B); newIndices.push_back(D); newIndices.push_back(E);

            markProcessed(cx, cz);
            markProcessed(cx + 1, cz);
        }
    }

    // 3. East boundary (cx == gridWidth - 2)
    if (lodEast > m_lod) {
        int cx = gridWidth - 2;
        for (int cz = 0; cz < gridWidth - 1; cz += 2) {
            if (isProcessed(cx, cz) || isProcessed(cx, cz + 1)) continue;

            uint32_t A = cx + cz * gridWidth;
            uint32_t B = cx + (cz + 1) * gridWidth;
            uint32_t C = cx + (cz + 2) * gridWidth;
            uint32_t D = (cx + 1) + cz * gridWidth;
            uint32_t E = (cx + 1) + (cz + 2) * gridWidth;

            newIndices.push_back(A); newIndices.push_back(B); newIndices.push_back(D);
            newIndices.push_back(B); newIndices.push_back(E); newIndices.push_back(D);
            newIndices.push_back(B); newIndices.push_back(C); newIndices.push_back(E);

            markProcessed(cx, cz);
            markProcessed(cx, cz + 1);
        }
    }

    // 4. West boundary (cx == 0)
    if (lodWest > m_lod) {
        int cx = 0;
        for (int cz = 0; cz < gridWidth - 1; cz += 2) {
            if (isProcessed(cx, cz) || isProcessed(cx, cz + 1)) continue;

            uint32_t A = cx + cz * gridWidth;
            uint32_t B = cx + (cz + 2) * gridWidth;
            uint32_t C = (cx + 1) + cz * gridWidth;
            uint32_t D = (cx + 1) + (cz + 1) * gridWidth;
            uint32_t E = (cx + 1) + (cz + 2) * gridWidth;

            newIndices.push_back(A); newIndices.push_back(D); newIndices.push_back(C);
            newIndices.push_back(A); newIndices.push_back(B); newIndices.push_back(D);
            newIndices.push_back(B); newIndices.push_back(E); newIndices.push_back(D);

            markProcessed(cx, cz);
            markProcessed(cx, cz + 1);
        }
    }

    // 5. Fill remaining cells
    for (int cz = 0; cz < gridWidth - 1; ++cz) {
        for (int cx = 0; cx < gridWidth - 1; ++cx) {
            if (isProcessed(cx, cz)) continue;

            uint32_t i0 = cz * gridWidth + cx;
            uint32_t i1 = cz * gridWidth + (cx + 1);
            uint32_t i2 = (cz + 1) * gridWidth + cx;
            uint32_t i3 = (cz + 1) * gridWidth + (cx + 1);

            newIndices.push_back(i0); newIndices.push_back(i2); newIndices.push_back(i1);
            newIndices.push_back(i1); newIndices.push_back(i2); newIndices.push_back(i3);
        }
    }

    m_indices = std::move(newIndices);
    m_indexCount = static_cast<uint32_t>(m_indices.size());

    // If already uploaded, we need to recreate the index buffer on GPU
    if (m_uploaded && device != VK_NULL_HANDLE) {
        TerrainManager::Get().IncrementStitchingUploads();
        // Destroy old index buffer
        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
            m_indexBufferMemory = VK_NULL_HANDLE;
        }

        // Re-upload index buffer
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

        outResources.device = device;
        outResources.commandPool = commandPool;

        if (!CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           outResources.stagingIndexBuffer, outResources.stagingIndexBufferMemory)) {
            Core::Logger::Error("TerrainChunk", "Failed to create staging index buffer for stitching.");
            return false;
        }

        void* data;
        vkMapMemory(device, outResources.stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
        std::memcpy(data, m_indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, outResources.stagingIndexBufferMemory);

        if (!CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory)) {
            vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
            vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
            outResources.stagingIndexBuffer = VK_NULL_HANDLE;
            outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
            Core::Logger::Error("TerrainChunk", "Failed to create GPU index buffer for stitching.");
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &outResources.commandBuffer) != VK_SUCCESS) {
            vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
            vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
            outResources.stagingIndexBuffer = VK_NULL_HANDLE;
            outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
            m_indexBufferMemory = VK_NULL_HANDLE;
            Core::Logger::Error("TerrainChunk", "Failed to allocate command buffer for stitching copy.");
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(outResources.commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(outResources.commandBuffer, outResources.stagingIndexBuffer, m_indexBuffer, 1, &copyRegion);

        vkEndCommandBuffer(outResources.commandBuffer);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = 0;

        if (vkCreateFence(device, &fenceInfo, nullptr, &outResources.fence) != VK_SUCCESS) {
            vkFreeCommandBuffers(device, commandPool, 1, &outResources.commandBuffer);
            outResources.commandBuffer = VK_NULL_HANDLE;
            vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
            vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
            outResources.stagingIndexBuffer = VK_NULL_HANDLE;
            outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
            m_indexBufferMemory = VK_NULL_HANDLE;
            Core::Logger::Error("TerrainChunk", "Failed to create fence for stitching copy.");
            return false;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &outResources.commandBuffer;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, outResources.fence) != VK_SUCCESS) {
            vkDestroyFence(device, outResources.fence, nullptr);
            outResources.fence = VK_NULL_HANDLE;
            vkFreeCommandBuffers(device, commandPool, 1, &outResources.commandBuffer);
            outResources.commandBuffer = VK_NULL_HANDLE;
            vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
            vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
            outResources.stagingIndexBuffer = VK_NULL_HANDLE;
            outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
            m_indexBufferMemory = VK_NULL_HANDLE;
            Core::Logger::Error("TerrainChunk", "Failed to submit stitching copy.");
            return false;
        }
    }

    return true;
}

bool TerrainChunk::UploadToGPU(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, StagingResources& outResources) {
    if (m_uploaded) return true;
    if (device == VK_NULL_HANDLE) {
        m_uploaded = true;
        return true;
    }

    outResources.device = device;
    outResources.commandPool = commandPool;

    VkDeviceSize vertexBufferSize = sizeof(TerrainVertex) * m_vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();

    // 1. Create Staging Vertex Buffer
    if (!CreateBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       outResources.stagingVertexBuffer, outResources.stagingVertexBufferMemory)) {
        return false;
    }

    void* data;
    vkMapMemory(device, outResources.stagingVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    std::memcpy(data, m_vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(device, outResources.stagingVertexBufferMemory);

    // 2. Create GPU Vertex Buffer
    if (!CreateBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory)) {
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    // 3. Create Staging Index Buffer
    if (!CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       outResources.stagingIndexBuffer, outResources.stagingIndexBufferMemory)) {
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    vkMapMemory(device, outResources.stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
    std::memcpy(data, m_indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device, outResources.stagingIndexBufferMemory);

    // 4. Create GPU Index Buffer
    if (!CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory)) {
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
        outResources.stagingIndexBuffer = VK_NULL_HANDLE;
        outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &outResources.commandBuffer) != VK_SUCCESS) {
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
        outResources.stagingIndexBuffer = VK_NULL_HANDLE;
        outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
        m_indexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(outResources.commandBuffer, &beginInfo);

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.srcOffset = 0;
    vertexCopyRegion.dstOffset = 0;
    vertexCopyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(outResources.commandBuffer, outResources.stagingVertexBuffer, m_vertexBuffer, 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.srcOffset = 0;
    indexCopyRegion.dstOffset = 0;
    indexCopyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(outResources.commandBuffer, outResources.stagingIndexBuffer, m_indexBuffer, 1, &indexCopyRegion);

    vkEndCommandBuffer(outResources.commandBuffer);

    // Create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    if (vkCreateFence(device, &fenceInfo, nullptr, &outResources.fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &outResources.commandBuffer);
        outResources.commandBuffer = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
        outResources.stagingIndexBuffer = VK_NULL_HANDLE;
        outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
        m_indexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &outResources.commandBuffer;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, outResources.fence) != VK_SUCCESS) {
        vkDestroyFence(device, outResources.fence, nullptr);
        outResources.fence = VK_NULL_HANDLE;
        vkFreeCommandBuffers(device, commandPool, 1, &outResources.commandBuffer);
        outResources.commandBuffer = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingVertexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingVertexBufferMemory, nullptr);
        outResources.stagingVertexBuffer = VK_NULL_HANDLE;
        outResources.stagingVertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        m_vertexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, outResources.stagingIndexBuffer, nullptr);
        vkFreeMemory(device, outResources.stagingIndexBufferMemory, nullptr);
        outResources.stagingIndexBuffer = VK_NULL_HANDLE;
        outResources.stagingIndexBufferMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
        m_indexBufferMemory = VK_NULL_HANDLE;
        return false;
    }

    m_uploaded = true;
    m_device = device;
    return true;
}

void TerrainChunk::DestroyGPUResources(VkDevice device) {
    if (!m_uploaded) return;

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBufferMemory = VK_NULL_HANDLE;
    }

    m_uploaded = false;
}

uint32_t TerrainChunk::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

bool TerrainChunk::CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return true;
}

void TerrainChunk::CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

bool TerrainChunk::Regenerate(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) {
    GenerateCPUData(m_lod);

    if (m_uploaded && device != VK_NULL_HANDLE) {
        DestroyGPUResources(device);
        
        StagingResources uploadResources;
        bool ok = UploadToGPU(device, physicalDevice, commandPool, graphicsQueue, uploadResources);
        if (ok) {
            if (uploadResources.fence != VK_NULL_HANDLE) {
                vkWaitForFences(device, 1, &uploadResources.fence, VK_TRUE, UINT64_MAX);
                vkDestroyFence(device, uploadResources.fence, nullptr);
                uploadResources.fence = VK_NULL_HANDLE;
            }
            if (uploadResources.stagingVertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, uploadResources.stagingVertexBuffer, nullptr);
            }
            if (uploadResources.stagingVertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, uploadResources.stagingVertexBufferMemory, nullptr);
            }
            if (uploadResources.stagingIndexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, uploadResources.stagingIndexBuffer, nullptr);
            }
            if (uploadResources.stagingIndexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, uploadResources.stagingIndexBufferMemory, nullptr);
            }
            if (uploadResources.commandBuffer != VK_NULL_HANDLE && uploadResources.commandPool != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(device, uploadResources.commandPool, 1, &uploadResources.commandBuffer);
            }
        }
        return ok;
    }
    return true;
}

} // namespace KumariEngine::Terrain
