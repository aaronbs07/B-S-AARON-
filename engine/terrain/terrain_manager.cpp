#include "terrain_manager.hpp"
#include "core/logger.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>

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
    return m_noiseGen.GetHeight(x, z);
}

int TerrainManager::GetBiomeAt(float x, float z) const {
    float height = m_noiseGen.GetHeight(x, z);
    return m_noiseGen.GetBiome(x, z, height);
}

} // namespace KumariEngine::Terrain
