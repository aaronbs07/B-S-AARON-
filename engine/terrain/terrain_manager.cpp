#include "terrain_manager.hpp"
#include "core/logger.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include "save/SaveSystem.hpp"
#include "physics/debug_renderer.hpp"

namespace KumariEngine::Terrain {

void TerrainManager::Initialize(uint32_t seed, float chunkSize) {
    m_seed = seed;
    m_chunkSize = chunkSize;
    m_noiseGen.Initialize(seed);
    m_vegetationSystem.Initialize(seed);
    Core::Logger::Info("TerrainManager", "Terrain Manager initialized with seed %d, chunk size %.1f", seed, chunkSize);
}

void TerrainManager::Shutdown(VkDevice device) {
    Core::Logger::Info("TerrainManager", "Shutting down Terrain Manager. Waiting for background tasks...");
    
    // Wait for all loading tasks
    for (auto& [coord, future] : m_loadingTasks) {
        if (future.valid()) {
            future.wait();
        }
    }
    m_loadingTasks.clear();

    // Wait for and cleanup pending Vulkan staging transfers
    if (device != VK_NULL_HANDLE) {
        std::vector<VkFence> fences;
        for (const auto& transfer : m_pendingTransfers) {
            if (transfer.fence != VK_NULL_HANDLE) {
                fences.push_back(transfer.fence);
            }
        }
        if (!fences.empty()) {
            vkWaitForFences(device, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
        }
    }

    for (auto& transfer : m_pendingTransfers) {
        if (transfer.device != VK_NULL_HANDLE) {
            if (transfer.stagingVertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(transfer.device, transfer.stagingVertexBuffer, nullptr);
            }
            if (transfer.stagingVertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(transfer.device, transfer.stagingVertexBufferMemory, nullptr);
            }
            if (transfer.stagingIndexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(transfer.device, transfer.stagingIndexBuffer, nullptr);
            }
            if (transfer.stagingIndexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(transfer.device, transfer.stagingIndexBufferMemory, nullptr);
            }
            if (transfer.commandBuffer != VK_NULL_HANDLE && transfer.commandPool != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(transfer.device, transfer.commandPool, 1, &transfer.commandBuffer);
            }
            if (transfer.fence != VK_NULL_HANDLE) {
                vkDestroyFence(transfer.device, transfer.fence, nullptr);
            }
        }
    }
    m_pendingTransfers.clear();

    // Cleanup active chunks GPU resources
    for (auto& [coord, chunk] : m_activeChunks) {
        chunk->DestroyGPUResources(device);
    }
    m_activeChunks.clear();

    // Cleanup cache GPU resources
    for (auto& [coord, chunk] : m_cache) {
        chunk->DestroyGPUResources(device);
    }
    m_cache.clear();

    Core::Logger::Info("TerrainManager", "Terrain Manager shut down cleanly.");
}

int TerrainManager::CalculateLOD(const ChunkCoord& coord, const glm::vec3& viewerPos) const {
    int32_t viewerChunkX = static_cast<int32_t>(std::floor(viewerPos.x / m_chunkSize));
    int32_t viewerChunkZ = static_cast<int32_t>(std::floor(viewerPos.z / m_chunkSize));

    int32_t distX = std::abs(coord.x - viewerChunkX);
    int32_t distZ = std::abs(coord.z - viewerChunkZ);
    int32_t dist = std::max(distX, distZ);

    if (dist <= 1) return 0;
    if (dist == 2) return 1;
    if (dist == 3) return 2;
    return 3;
}

void TerrainManager::Update(const glm::vec3& viewerPos, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_commandPool = commandPool;
    m_graphicsQueue = graphicsQueue;

    auto streamingStart = std::chrono::high_resolution_clock::now();
    m_lastGPUUploadTimeMs = 0.0f;

    // Poll and clean up finished GPU transfers
    for (auto it = m_pendingTransfers.begin(); it != m_pendingTransfers.end();) {
        if (it->device != VK_NULL_HANDLE && it->fence != VK_NULL_HANDLE) {
            if (vkGetFenceStatus(it->device, it->fence) == VK_SUCCESS) {
                // Transfer complete, clean up staging resources
                if (it->stagingVertexBuffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(it->device, it->stagingVertexBuffer, nullptr);
                }
                if (it->stagingVertexBufferMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(it->device, it->stagingVertexBufferMemory, nullptr);
                }
                if (it->stagingIndexBuffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(it->device, it->stagingIndexBuffer, nullptr);
                }
                if (it->stagingIndexBufferMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(it->device, it->stagingIndexBufferMemory, nullptr);
                }
                if (it->commandBuffer != VK_NULL_HANDLE && it->commandPool != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(it->device, it->commandPool, 1, &it->commandBuffer);
                }
                vkDestroyFence(it->device, it->fence, nullptr);

                it = m_pendingTransfers.erase(it);
            } else {
                ++it;
            }
        } else {
            // Null/Offline mode mock device transfer
            it = m_pendingTransfers.erase(it);
        }
    }

    // 1. Poll loading tasks
    for (auto it = m_loadingTasks.begin(); it != m_loadingTasks.end();) {
        auto& [coord, future] = *it;
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto chunk = future.get();
            if (chunk) {
                chunk->SetState(TerrainChunk::ChunkState::Loaded);
                StagingResources uploadResources;
                auto uploadStart = std::chrono::high_resolution_clock::now();
                bool uploadOk = chunk->UploadToGPU(device, physicalDevice, commandPool, graphicsQueue, uploadResources);
                auto uploadEnd = std::chrono::high_resolution_clock::now();
                m_lastGPUUploadTimeMs += std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
                if (uploadOk) {
                    if (uploadResources.fence != VK_NULL_HANDLE) {
                        m_pendingTransfers.push_back(uploadResources);
                    }
                    m_activeChunks[coord] = chunk;
                    chunk->SetState(TerrainChunk::ChunkState::Visible);
                }
            }
            it = m_loadingTasks.erase(it);
        } else {
            ++it;
        }
    }

    int32_t viewerChunkX = static_cast<int32_t>(std::floor(viewerPos.x / m_chunkSize));
    int32_t viewerChunkZ = static_cast<int32_t>(std::floor(viewerPos.z / m_chunkSize));

    // 2. Unload out-of-range chunks
    for (auto it = m_activeChunks.begin(); it != m_activeChunks.end();) {
        const auto& coord = it->first;
        int32_t distX = std::abs(coord.x - viewerChunkX);
        int32_t distZ = std::abs(coord.z - viewerChunkZ);
        if (std::max(distX, distZ) > m_unloadRadius) {
            auto chunk = it->second;
            chunk->SetState(TerrainChunk::ChunkState::Unloading);
            chunk->DestroyGPUResources(device);
            chunk->SetState(TerrainChunk::ChunkState::Loaded); // stored in CPU cache
            
            // Put in CPU cache
            m_cache[coord] = chunk;
            if (m_cache.size() > m_maxCacheSize) {
                auto evictIt = m_cache.begin();
                evictIt->second->SetState(TerrainChunk::ChunkState::Unloaded);
                m_cache.erase(evictIt);
            }

            it = m_activeChunks.erase(it);
        } else {
            ++it;
        }
    }

    // 3. Load nearby chunks or update LOD
    for (int32_t x = viewerChunkX - m_loadRadius; x <= viewerChunkX + m_loadRadius; ++x) {
        for (int32_t z = viewerChunkZ - m_loadRadius; z <= viewerChunkZ + m_loadRadius; ++z) {
            ChunkCoord coord{x, z};
            int targetLOD = CalculateLOD(coord, viewerPos);

            auto activeIt = m_activeChunks.find(coord);
            if (activeIt != m_activeChunks.end()) {
                // If LOD doesn't match, unload and trigger background reload
                if (activeIt->second->GetLOD() != targetLOD && m_loadingTasks.find(coord) == m_loadingTasks.end()) {
                    activeIt->second->SetState(TerrainChunk::ChunkState::Unloading);
                    activeIt->second->DestroyGPUResources(device);
                    activeIt->second->SetState(TerrainChunk::ChunkState::Loaded); // ready for reload
                    m_activeChunks.erase(activeIt);
                } else {
                    continue; // Correct LOD already loaded or loading
                }
            }

            if (m_loadingTasks.find(coord) != m_loadingTasks.end()) {
                continue; // Already loading
            }

            // Check cache
            auto cacheIt = m_cache.find(coord);
            if (cacheIt != m_cache.end()) {
                m_cacheHits++;
                auto chunk = cacheIt->second;
                m_cache.erase(cacheIt);

                if (chunk->GetLOD() == targetLOD) {
                    // Quick re-upload
                    StagingResources uploadResources;
                    auto uploadStart = std::chrono::high_resolution_clock::now();
                    bool uploadOk = chunk->UploadToGPU(device, physicalDevice, commandPool, graphicsQueue, uploadResources);
                    auto uploadEnd = std::chrono::high_resolution_clock::now();
                    m_lastGPUUploadTimeMs += std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
                    if (uploadOk) {
                        if (uploadResources.fence != VK_NULL_HANDLE) {
                            m_pendingTransfers.push_back(uploadResources);
                        }
                        m_activeChunks[coord] = chunk;
                        chunk->SetState(TerrainChunk::ChunkState::Visible);
                    }
                } else {
                    // Re-generate and upload
                    chunk->SetState(TerrainChunk::ChunkState::Loading);
                    chunk->GenerateCPUData(targetLOD);
                    m_vegetationSystem.PopulateVegetation(chunk.get());
                    chunk->SetState(TerrainChunk::ChunkState::Loaded);
                    StagingResources uploadResources;
                    auto uploadStart = std::chrono::high_resolution_clock::now();
                    bool uploadOk = chunk->UploadToGPU(device, physicalDevice, commandPool, graphicsQueue, uploadResources);
                    auto uploadEnd = std::chrono::high_resolution_clock::now();
                    m_lastGPUUploadTimeMs += std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
                    if (uploadOk) {
                        if (uploadResources.fence != VK_NULL_HANDLE) {
                            m_pendingTransfers.push_back(uploadResources);
                        }
                        m_activeChunks[coord] = chunk;
                        chunk->SetState(TerrainChunk::ChunkState::Visible);
                    }
                }
            } else {
                m_cacheMisses++;
                // Queue background load
                auto future = std::async(std::launch::async, [this, coord, targetLOD]() {
                    auto start = std::chrono::high_resolution_clock::now();
                    
                    auto chunk = std::make_shared<TerrainChunk>(coord.x, coord.z, m_chunkSize, &m_noiseGen);
                    chunk->SetState(TerrainChunk::ChunkState::Loading);
                    chunk->GenerateCPUData(targetLOD);
                    m_vegetationSystem.PopulateVegetation(chunk.get());
                    chunk->SetState(TerrainChunk::ChunkState::Loaded);
                    
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<float, std::milli> duration = end - start;

                    // Update stats
                    m_avgGenTimeMs = (m_avgGenTimeMs * m_totalGenCount + duration.count()) / (m_totalGenCount + 1);
                    m_totalGenCount++;

                    return chunk;
                });
                m_loadingTasks[coord] = std::move(future);
            }
        }
    }

    // 4. Update index buffer stitching for matching chunk boundaries
    UpdateStitching(device, physicalDevice, commandPool, graphicsQueue);

    auto streamingEnd = std::chrono::high_resolution_clock::now();
    m_lastStreamingTimeMs = std::chrono::duration<float, std::milli>(streamingEnd - streamingStart).count() - m_lastGPUUploadTimeMs;
}

void TerrainManager::UpdateStitching(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) {
    for (auto& [coord, chunk] : m_activeChunks) {
        int lod = chunk->GetLOD();

        // Get neighbors
        auto getLod = [&](int32_t nx, int32_t nz) {
            auto it = m_activeChunks.find(ChunkCoord{nx, nz});
            if (it != m_activeChunks.end()) {
                return it->second->GetLOD();
            }
            return lod; // Assume same LOD if not active
        };

        int lodN = getLod(coord.x, coord.z + 1);
        int lodS = getLod(coord.x, coord.z - 1);
        int lodE = getLod(coord.x + 1, coord.z);
        int lodW = getLod(coord.x - 1, coord.z);

        StagingResources stitchResources;
        auto uploadStart = std::chrono::high_resolution_clock::now();
        bool stitchOk = chunk->RebuildIndicesForStitching(device, physicalDevice, commandPool, graphicsQueue, lodN, lodS, lodE, lodW, stitchResources);
        auto uploadEnd = std::chrono::high_resolution_clock::now();
        m_lastGPUUploadTimeMs += std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
        if (stitchOk && stitchResources.fence != VK_NULL_HANDLE) {
            m_pendingTransfers.push_back(stitchResources);
        }
    }
}

size_t TerrainManager::GetMemoryEstimate() const {
    size_t activeCount = m_activeChunks.size();
    size_t cachedCount = m_cache.size();

    // Average size of a chunk in bytes:
    // Vertices: LOD 0: 65x65 * 48B = ~200KB
    // Indices: LOD 0: 64x64x6 * 4B = ~98KB
    // Total GPU per active chunk = ~300KB + CPU storage.
    // Cache has CPU-only data.
    size_t bytesPerActive = 300000;
    size_t bytesPerCached = 200000;

    return (activeCount * bytesPerActive) + (cachedCount * bytesPerCached);
}

float TerrainManager::GetHeightAt(float x, float z) const {
    if (m_seed == 9999) return 0.0f;

    auto getGridHeight = [this](int32_t gx, int32_t gz) -> float {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
        auto it = m_heightEdits.find(key);
        if (it != m_heightEdits.end()) {
            return it->second;
        }
        return m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
    };

    float fx = std::floor(x);
    float fz = std::floor(z);
    int32_t x0 = static_cast<int32_t>(fx);
    int32_t z0 = static_cast<int32_t>(fz);
    int32_t x1 = x0 + 1;
    int32_t z1 = z0 + 1;

    float h00 = getGridHeight(x0, z0);
    float h10 = getGridHeight(x1, z0);
    float h01 = getGridHeight(x0, z1);
    float h11 = getGridHeight(x1, z1);

    float tx = x - fx;
    float tz = z - fz;

    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;

    return h0 * (1.0f - tz) + h1 * tz;
}

int TerrainManager::GetBiomeAt(float x, float z) const {
    float height = GetHeightAt(x, z);
    return m_noiseGen.GetBiome(x, z, height);
}

glm::vec4 TerrainManager::GetLayerWeightsAt(float x, float z) const {
    auto getGridWeights = [this](int32_t gx, int32_t gz) -> glm::vec4 {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
        auto it = m_layerEdits.find(key);
        if (it != m_layerEdits.end()) {
            return it->second;
        }
        float height = m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
        int biomeType = m_noiseGen.GetBiome(static_cast<float>(gx), static_cast<float>(gz), height);
        glm::vec4 W(0.0f);
        if (biomeType == 2) W.x = 1.0f; // Plains
        else if (biomeType == 3) W.y = 1.0f; // Hills
        else if (biomeType == 4) W.z = 1.0f; // Mountains
        else if (biomeType == 1) W.w = 1.0f; // Beach
        return W;
    };

    float fx = std::floor(x);
    float fz = std::floor(z);
    int32_t x0 = static_cast<int32_t>(fx);
    int32_t z0 = static_cast<int32_t>(fz);
    int32_t x1 = x0 + 1;
    int32_t z1 = z0 + 1;

    glm::vec4 w00 = getGridWeights(x0, z0);
    glm::vec4 w10 = getGridWeights(x1, z0);
    glm::vec4 w01 = getGridWeights(x0, z1);
    glm::vec4 w11 = getGridWeights(x1, z1);

    float tx = x - fx;
    float tz = z - fz;

    glm::vec4 w0 = w00 * (1.0f - tx) + w10 * tx;
    glm::vec4 w1 = w01 * (1.0f - tx) + w11 * tx;

    return w0 * (1.0f - tz) + w1 * tz;
}

void TerrainManager::ApplyHeightEdit(float worldX, float worldZ, float radius, float strength) {
    ApplyBrush(worldX, worldZ, BrushType::RaiseLower, radius, strength, 1.0f);
}

void TerrainManager::ClearEdits() {
    m_heightEdits.clear();
    m_layerEdits.clear();
    m_editedVegetationChunks.clear();
    m_paintedVegetation.clear();
    m_roads.clear();
    m_rivers.clear();

    for (auto& [coord, chunk] : m_activeChunks) {
        chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
    }
}

void TerrainManager::ApplyBrush(float worldX, float worldZ, BrushType type, float radius, float strength, float deltaTime, float targetHeight) {
    int32_t minX = static_cast<int32_t>(std::floor(worldX - radius));
    int32_t maxX = static_cast<int32_t>(std::ceil(worldX + radius));
    int32_t minZ = static_cast<int32_t>(std::floor(worldZ - radius));
    int32_t maxZ = static_cast<int32_t>(std::ceil(worldZ + radius));

    auto getGridHeightRaw = [this](int32_t gx, int32_t gz) -> float {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
        auto it = m_heightEdits.find(key);
        if (it != m_heightEdits.end()) {
            return it->second;
        }
        return m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
    };

    auto setGridHeightRaw = [this](int32_t gx, int32_t gz, float val) {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
        m_heightEdits[key] = val;
    };

    for (int32_t gz = minZ; gz <= maxZ; ++gz) {
        for (int32_t gx = minX; gx <= maxX; ++gx) {
            float dist = glm::distance(glm::vec2(gx, gz), glm::vec2(worldX, worldZ));
            if (dist > radius) continue;

            float factor = 1.0f - (dist / radius);
            float h = getGridHeightRaw(gx, gz);

            if (type == BrushType::RaiseLower) {
                h += strength * factor * deltaTime;
            } else if (type == BrushType::Smooth) {
                float avg = (getGridHeightRaw(gx + 1, gz) + getGridHeightRaw(gx - 1, gz) +
                             getGridHeightRaw(gx, gz + 1) + getGridHeightRaw(gx, gz - 1)) * 0.25f;
                h = glm::mix(h, avg, glm::clamp(strength * factor * deltaTime, 0.0f, 1.0f));
            } else if (type == BrushType::Flatten) {
                h = glm::mix(h, targetHeight, glm::clamp(strength * factor * deltaTime, 0.0f, 1.0f));
            } else if (type == BrushType::Noise) {
                uint32_t seed = static_cast<uint32_t>(gx) * 73856093U ^ static_cast<uint32_t>(gz) * 19349663U;
                seed = (seed ^ (seed >> 16)) * 2246822507U;
                float noiseVal = (static_cast<float>(seed & 0xFFFF) / 65535.0f * 2.0f - 1.0f);
                h += noiseVal * strength * factor * deltaTime;
            } else if (type == BrushType::Erosion) {
                float talus = 0.5f;
                float maxDiff = 0.0f;
                int32_t bestNx = gx, bestNz = gz;
                int32_t dx[] = {0, 0, 1, -1};
                int32_t dz[] = {1, -1, 0, 0};
                for (int i = 0; i < 4; ++i) {
                    float nh = getGridHeightRaw(gx + dx[i], gz + dz[i]);
                    float diff = h - nh;
                    if (diff > maxDiff) {
                        maxDiff = diff;
                        bestNx = gx + dx[i];
                        bestNz = gz + dz[i];
                    }
                }
                if (maxDiff > talus) {
                    float amount = (maxDiff - talus) * 0.5f * strength * factor * deltaTime;
                    h -= amount;
                    setGridHeightRaw(bestNx, bestNz, getGridHeightRaw(bestNx, bestNz) + amount);
                }
            }

            setGridHeightRaw(gx, gz, h);
        }
    }

    // Regenerate active chunks
    for (auto& [coord, chunk] : m_activeChunks) {
        float minChunkX = coord.x * m_chunkSize;
        float maxChunkX = minChunkX + m_chunkSize;
        float minChunkZ = coord.z * m_chunkSize;
        float maxChunkZ = minChunkZ + m_chunkSize;

        if (worldX + radius >= minChunkX && worldX - radius <= maxChunkX &&
            worldZ + radius >= minChunkZ && worldZ - radius <= maxChunkZ) {
            chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
        }
    }
}

void TerrainManager::ApplyTexturePaint(float worldX, float worldZ, int targetLayer, float radius, float strength, float deltaTime) {
    if (targetLayer < 0 || targetLayer >= 4) return;

    int32_t minX = static_cast<int32_t>(std::floor(worldX - radius));
    int32_t maxX = static_cast<int32_t>(std::ceil(worldX + radius));
    int32_t minZ = static_cast<int32_t>(std::floor(worldZ - radius));
    int32_t maxZ = static_cast<int32_t>(std::ceil(worldZ + radius));

    auto getGridWeightsRaw = [this](int32_t gx, int32_t gz) -> glm::vec4 {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
        auto it = m_layerEdits.find(key);
        if (it != m_layerEdits.end()) {
            return it->second;
        }
        float height = m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
        int biomeType = m_noiseGen.GetBiome(static_cast<float>(gx), static_cast<float>(gz), height);
        glm::vec4 W(0.0f);
        if (biomeType == 2) W.x = 1.0f; // Plains
        else if (biomeType == 3) W.y = 1.0f; // Hills
        else if (biomeType == 4) W.z = 1.0f; // Mountains
        else if (biomeType == 1) W.w = 1.0f; // Beach
        return W;
    };

    for (int32_t gz = minZ; gz <= maxZ; ++gz) {
        for (int32_t gx = minX; gx <= maxX; ++gx) {
            float dist = glm::distance(glm::vec2(gx, gz), glm::vec2(worldX, worldZ));
            if (dist > radius) continue;

            float factor = 1.0f - (dist / radius);
            glm::vec4 W = getGridWeightsRaw(gx, gz);

            float delta = strength * factor * deltaTime;
            W[targetLayer] += delta;
            W[targetLayer] = glm::clamp(W[targetLayer], 0.0f, 1.0f);

            float otherSum = 0.0f;
            for (int i = 0; i < 4; ++i) {
                if (i != targetLayer) otherSum += W[i];
            }

            if (otherSum > 0.0001f) {
                float scale = (1.0f - W[targetLayer]) / otherSum;
                for (int i = 0; i < 4; ++i) {
                    if (i != targetLayer) W[i] *= scale;
                }
            } else {
                float val = (1.0f - W[targetLayer]) / 3.0f;
                for (int i = 0; i < 4; ++i) {
                    if (i != targetLayer) W[i] = val;
                }
            }

            uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
            m_layerEdits[key] = W;
        }
    }

    // Regenerate active chunks
    for (auto& [coord, chunk] : m_activeChunks) {
        float minChunkX = coord.x * m_chunkSize;
        float maxChunkX = minChunkX + m_chunkSize;
        float minChunkZ = coord.z * m_chunkSize;
        float maxChunkZ = minChunkZ + m_chunkSize;

        if (worldX + radius >= minChunkX && worldX - radius <= maxChunkX &&
            worldZ + radius >= minChunkZ && worldZ - radius <= maxChunkZ) {
            chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
        }
    }
}

void TerrainManager::ApplyVegetationPaint(float worldX, float worldZ, int vegType, float radius, float density, float minScale, float maxScale, bool eraseMode) {
    for (auto& [coord, chunk] : m_activeChunks) {
        float minChunkX = coord.x * m_chunkSize;
        float maxChunkX = minChunkX + m_chunkSize;
        float minChunkZ = coord.z * m_chunkSize;
        float maxChunkZ = minChunkZ + m_chunkSize;

        if (worldX + radius >= minChunkX && worldX - radius <= maxChunkX &&
            worldZ + radius >= minChunkZ && worldZ - radius <= maxChunkZ) {

            if (m_editedVegetationChunks.find(coord) == m_editedVegetationChunks.end()) {
                m_editedVegetationChunks.insert(coord);
                m_paintedVegetation[coord] = chunk->GetVegetation();
            }

            auto& vegList = m_paintedVegetation[coord];

            if (eraseMode) {
                vegList.erase(std::remove_if(vegList.begin(), vegList.end(), [&](const VegetationInstance& inst) {
                    return glm::distance(glm::vec2(inst.position.x, inst.position.z), glm::vec2(worldX, worldZ)) <= radius;
                }), vegList.end());
            } else {
                int spawnCount = static_cast<int>(density);
                uint32_t seedVal = static_cast<uint32_t>(coord.x * 73856093 ^ coord.z * 19349663);
                std::mt19937 rng(seedVal + vegType);
                std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

                for (int i = 0; i < spawnCount; ++i) {
                    float r = dist01(rng) * radius;
                    float angle = dist01(rng) * 2.0f * 3.14159265f;
                    float px = worldX + r * std::cos(angle);
                    float pz = worldZ + r * std::sin(angle);

                    if (px >= minChunkX && px <= maxChunkX && pz >= minChunkZ && pz <= maxChunkZ) {
                        if (dist01(rng) > (r / radius)) {
                            VegetationInstance inst;
                            inst.type = vegType;
                            inst.position = glm::vec3(px, GetHeightAt(px, pz), pz);
                            inst.scale = minScale + dist01(rng) * (maxScale - minScale);
                            inst.rotation = dist01(rng) * 360.0f;
                            vegList.push_back(inst);
                        }
                    }
                }
            }

            chunk->GetVegetation() = vegList;
        }
    }
}

void TerrainManager::CreateRoad(const std::vector<glm::vec3>& splinePoints, float width, int roadType) {
    RoadData road;
    road.splinePoints = splinePoints;
    road.width = width;
    road.roadType = roadType;
    m_roads.push_back(road);
}

void TerrainManager::CreateRiver(const std::vector<glm::vec3>& splinePoints, float width, float depth) {
    RiverData river;
    river.splinePoints = splinePoints;
    river.width = width;
    river.depth = depth;
    m_rivers.push_back(river);
}

void TerrainManager::ClearSplines() {
    m_roads.clear();
    m_rivers.clear();
}

void TerrainManager::ApplySplines() {
    auto sqrDist = [](const glm::vec3& a, const glm::vec3& b) {
        glm::vec3 diff = a - b;
        return diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
    };

    auto projectOnSpline = [&](const std::vector<glm::vec3>& pts, const glm::vec3& query, float& outDist, glm::vec3& outProj) {
        outDist = std::numeric_limits<float>::max();
        if (pts.size() < 2) return;

        for (size_t i = 0; i < pts.size() - 1; ++i) {
            glm::vec3 p0 = pts[i];
            glm::vec3 p1 = pts[i+1];
            glm::vec3 dir = p1 - p0;
            float lenSq = sqrDist(p0, p1);
            float t = 0.0f;
            if (lenSq > 0.0001f) {
                t = glm::dot(query - p0, dir) / lenSq;
                t = glm::clamp(t, 0.0f, 1.0f);
            }
            glm::vec3 proj = p0 + t * dir;
            float d = glm::distance(glm::vec2(query.x, query.z), glm::vec2(proj.x, proj.z));
            if (d < outDist) {
                outDist = d;
                outProj = proj;
            }
        }
    };

    for (const auto& road : m_roads) {
        if (road.splinePoints.size() < 2) continue;
        
        float minX = std::numeric_limits<float>::max(), maxX = -std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max(), maxZ = -std::numeric_limits<float>::max();
        for (const auto& pt : road.splinePoints) {
            minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
            minZ = std::min(minZ, pt.z); maxZ = std::max(maxZ, pt.z);
        }

        int32_t rMinX = static_cast<int32_t>(std::floor(minX - road.width));
        int32_t rMaxX = static_cast<int32_t>(std::ceil(maxX + road.width));
        int32_t rMinZ = static_cast<int32_t>(std::floor(minZ - road.width));
        int32_t rMaxZ = static_cast<int32_t>(std::ceil(maxZ + road.width));

        for (int32_t gz = rMinZ; gz <= rMaxZ; ++gz) {
            for (int32_t gx = rMinX; gx <= rMaxX; ++gx) {
                float dist = 0.0f;
                glm::vec3 proj;
                projectOnSpline(road.splinePoints, glm::vec3(gx, 0.0f, gz), dist, proj);

                float halfWidth = road.width * 0.5f;
                if (dist <= halfWidth) {
                    float factor = 1.0f - (dist / halfWidth);
                    uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
                    float currentHeight = m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
                    auto it = m_heightEdits.find(key);
                    if (it != m_heightEdits.end()) currentHeight = it->second;

                    m_heightEdits[key] = glm::mix(currentHeight, proj.y, factor);

                    glm::vec4 W(0.0f);
                    auto itW = m_layerEdits.find(key);
                    if (itW != m_layerEdits.end()) W = itW->second;
                    else {
                        float height = GetHeightAt(static_cast<float>(gx), static_cast<float>(gz));
                        int biomeType = m_noiseGen.GetBiome(static_cast<float>(gx), static_cast<float>(gz), height);
                        if (biomeType == 2) W.x = 1.0f;
                        else if (biomeType == 3) W.y = 1.0f;
                        else if (biomeType == 4) W.z = 1.0f;
                        else if (biomeType == 1) W.w = 1.0f;
                    }
                    W[1] = glm::mix(W[1], 1.0f, factor);
                    float sum = W.x + W.y + W.z + W.w;
                    if (sum > 0.0f) W /= sum;
                    m_layerEdits[key] = W;
                }
            }
        }
    }

    for (const auto& river : m_rivers) {
        if (river.splinePoints.size() < 2) continue;

        float minX = std::numeric_limits<float>::max(), maxX = -std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max(), maxZ = -std::numeric_limits<float>::max();
        for (const auto& pt : river.splinePoints) {
            minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
            minZ = std::min(minZ, pt.z); maxZ = std::max(maxZ, pt.z);
        }

        int32_t rMinX = static_cast<int32_t>(std::floor(minX - river.width));
        int32_t rMaxX = static_cast<int32_t>(std::ceil(maxX + river.width));
        int32_t rMinZ = static_cast<int32_t>(std::floor(minZ - river.width));
        int32_t rMaxZ = static_cast<int32_t>(std::ceil(maxZ + river.width));

        for (int32_t gz = rMinZ; gz <= rMaxZ; ++gz) {
            for (int32_t gx = rMinX; gx <= rMaxX; ++gx) {
                float dist = 0.0f;
                glm::vec3 proj;
                projectOnSpline(river.splinePoints, glm::vec3(gx, 0.0f, gz), dist, proj);

                float halfWidth = river.width * 0.5f;
                if (dist <= halfWidth) {
                    float factor = 1.0f - (dist / halfWidth);
                    uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(gz));
                    float currentHeight = m_noiseGen.GetHeight(static_cast<float>(gx), static_cast<float>(gz));
                    auto it = m_heightEdits.find(key);
                    if (it != m_heightEdits.end()) currentHeight = it->second;

                    float targetH = proj.y - river.depth * factor;
                    m_heightEdits[key] = glm::mix(currentHeight, targetH, factor);

                    glm::vec4 W(0.0f);
                    auto itW = m_layerEdits.find(key);
                    if (itW != m_layerEdits.end()) W = itW->second;
                    else {
                        float height = GetHeightAt(static_cast<float>(gx), static_cast<float>(gz));
                        int biomeType = m_noiseGen.GetBiome(static_cast<float>(gx), static_cast<float>(gz), height);
                        if (biomeType == 2) W.x = 1.0f;
                        else if (biomeType == 3) W.y = 1.0f;
                        else if (biomeType == 4) W.z = 1.0f;
                        else if (biomeType == 1) W.w = 1.0f;
                    }
                    W[3] = glm::mix(W[3], 1.0f, factor);
                    float sum = W.x + W.y + W.z + W.w;
                    if (sum > 0.0f) W /= sum;
                    m_layerEdits[key] = W;
                }
            }
        }
    }

    for (auto& [coord, chunk] : m_activeChunks) {
        chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
    }
}

void TerrainManager::RenderDebugVisualizations(VkCommandBuffer cmdBuf, const glm::mat4& viewProj) {
    (void)cmdBuf;
    (void)viewProj;

    if (m_chunkBorders) {
        for (const auto& [coord, chunk] : m_activeChunks) {
            Physics::PhysicsDebugRenderer::Get().DrawAABB(chunk->GetMinBounds(), chunk->GetMaxBounds(), glm::vec3(1.0f, 1.0f, 0.0f));
        }
    }

    for (const auto& road : m_roads) {
        if (road.splinePoints.size() < 2) continue;
        for (size_t i = 0; i < road.splinePoints.size() - 1; ++i) {
            Physics::PhysicsDebugRenderer::Get().DrawLine(road.splinePoints[i], road.splinePoints[i + 1], glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    for (const auto& river : m_rivers) {
        if (river.splinePoints.size() < 2) continue;
        for (size_t i = 0; i < river.splinePoints.size() - 1; ++i) {
            Physics::PhysicsDebugRenderer::Get().DrawLine(river.splinePoints[i], river.splinePoints[i + 1], glm::vec3(0.0f, 0.5f, 1.0f));
        }
    }
}

bool TerrainManager::SaveTerrainEdits(const std::string& filepath) const {
    return Save::SaveSystem::WriteSave(filepath, [this](Save::BinaryWriter& writer) {
        writer.WriteUint32(m_seed);
        writer.WriteFloat(m_chunkSize);

        writer.WriteUint32(static_cast<uint32_t>(m_heightEdits.size()));
        for (const auto& [key, height] : m_heightEdits) {
            writer.WriteUint64(key);
            writer.WriteFloat(height);
        }

        writer.WriteUint32(static_cast<uint32_t>(m_layerEdits.size()));
        for (const auto& [key, weights] : m_layerEdits) {
            writer.WriteUint64(key);
            writer.WriteFloat(weights.x);
            writer.WriteFloat(weights.y);
            writer.WriteFloat(weights.z);
            writer.WriteFloat(weights.w);
        }

        writer.WriteUint32(static_cast<uint32_t>(m_editedVegetationChunks.size()));
        for (const auto& coord : m_editedVegetationChunks) {
            writer.WriteInt32(coord.x);
            writer.WriteInt32(coord.z);
            auto it = m_paintedVegetation.find(coord);
            if (it != m_paintedVegetation.end()) {
                writer.WriteUint32(static_cast<uint32_t>(it->second.size()));
                for (const auto& inst : it->second) {
                    writer.WriteInt32(inst.type);
                    writer.WriteFloat(inst.position.x);
                    writer.WriteFloat(inst.position.y);
                    writer.WriteFloat(inst.position.z);
                    writer.WriteFloat(inst.scale);
                    writer.WriteFloat(inst.rotation);
                }
            } else {
                writer.WriteUint32(0);
            }
        }

        writer.WriteUint32(static_cast<uint32_t>(m_roads.size()));
        for (const auto& road : m_roads) {
            writer.WriteFloat(road.width);
            writer.WriteInt32(road.roadType);
            writer.WriteUint32(static_cast<uint32_t>(road.splinePoints.size()));
            for (const auto& pt : road.splinePoints) {
                writer.WriteFloat(pt.x);
                writer.WriteFloat(pt.y);
                writer.WriteFloat(pt.z);
            }
        }

        writer.WriteUint32(static_cast<uint32_t>(m_rivers.size()));
        for (const auto& river : m_rivers) {
            writer.WriteFloat(river.width);
            writer.WriteFloat(river.depth);
            writer.WriteUint32(static_cast<uint32_t>(river.splinePoints.size()));
            for (const auto& pt : river.splinePoints) {
                writer.WriteFloat(pt.x);
                writer.WriteFloat(pt.y);
                writer.WriteFloat(pt.z);
            }
        }

        return true;
    });
}

bool TerrainManager::LoadTerrainEdits(const std::string& filepath) {
    return Save::SaveSystem::ReadSave(filepath, [this](Save::BinaryReader& reader, uint32_t version) {
        (void)version;
        uint32_t seed;
        float chunkSize;
        if (!reader.ReadUint32(seed) || !reader.ReadFloat(chunkSize)) return false;
        m_seed = seed;
        m_chunkSize = chunkSize;

        uint32_t hCount;
        if (!reader.ReadUint32(hCount)) return false;
        m_heightEdits.clear();
        for (uint32_t i = 0; i < hCount; ++i) {
            uint64_t key;
            float h;
            if (!reader.ReadUint64(key) || !reader.ReadFloat(h)) return false;
            m_heightEdits[key] = h;
        }

        uint32_t lCount;
        if (!reader.ReadUint32(lCount)) return false;
        m_layerEdits.clear();
        for (uint32_t i = 0; i < lCount; ++i) {
            uint64_t key;
            float rx, ry, rz, rw;
            if (!reader.ReadUint64(key) || !reader.ReadFloat(rx) || !reader.ReadFloat(ry) || !reader.ReadFloat(rz) || !reader.ReadFloat(rw)) return false;
            m_layerEdits[key] = glm::vec4(rx, ry, rz, rw);
        }

        uint32_t vCount;
        if (!reader.ReadUint32(vCount)) return false;
        m_editedVegetationChunks.clear();
        m_paintedVegetation.clear();
        for (uint32_t i = 0; i < vCount; ++i) {
            ChunkCoord coord;
            if (!reader.ReadInt32(coord.x) || !reader.ReadInt32(coord.z)) return false;
            m_editedVegetationChunks.insert(coord);
            uint32_t instCount;
            if (!reader.ReadUint32(instCount)) return false;
            std::vector<VegetationInstance> insts;
            insts.reserve(instCount);
            for (uint32_t j = 0; j < instCount; ++j) {
                int type;
                float px, py, pz, scale, rot;
                if (!reader.ReadInt32(type) || !reader.ReadFloat(px) || !reader.ReadFloat(py) || !reader.ReadFloat(pz) || !reader.ReadFloat(scale) || !reader.ReadFloat(rot)) return false;
                VegetationInstance inst;
                inst.type = type;
                inst.position = glm::vec3(px, py, pz);
                inst.scale = scale;
                inst.rotation = rot;
                insts.push_back(inst);
            }
            m_paintedVegetation[coord] = insts;
        }

        uint32_t roadCount;
        if (!reader.ReadUint32(roadCount)) return false;
        m_roads.clear();
        for (uint32_t i = 0; i < roadCount; ++i) {
            float width;
            int rType;
            uint32_t ptCount;
            if (!reader.ReadFloat(width) || !reader.ReadInt32(rType) || !reader.ReadUint32(ptCount)) return false;
            std::vector<glm::vec3> pts(ptCount);
            for (uint32_t j = 0; j < ptCount; ++j) {
                if (!reader.ReadFloat(pts[j].x) || !reader.ReadFloat(pts[j].y) || !reader.ReadFloat(pts[j].z)) return false;
            }
            RoadData road;
            road.width = width;
            road.roadType = rType;
            road.splinePoints = pts;
            m_roads.push_back(road);
        }

        uint32_t riverCount;
        if (!reader.ReadUint32(riverCount)) return false;
        m_rivers.clear();
        for (uint32_t i = 0; i < riverCount; ++i) {
            float width, depth;
            uint32_t ptCount;
            if (!reader.ReadFloat(width) || !reader.ReadFloat(depth) || !reader.ReadUint32(ptCount)) return false;
            std::vector<glm::vec3> pts(ptCount);
            for (uint32_t j = 0; j < ptCount; ++j) {
                if (!reader.ReadFloat(pts[j].x) || !reader.ReadFloat(pts[j].y) || !reader.ReadFloat(pts[j].z)) return false;
            }
            RiverData river;
            river.width = width;
            river.depth = depth;
            river.splinePoints = pts;
            m_rivers.push_back(river);
        }

        for (auto& [coord, chunk] : m_activeChunks) {
            chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
        }

        return true;
    });
}

void TerrainManager::RegenerateActiveChunks() {
    for (auto& [coord, chunk] : m_activeChunks) {
        chunk->Regenerate(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue);
    }
}

} // namespace KumariEngine::Terrain
