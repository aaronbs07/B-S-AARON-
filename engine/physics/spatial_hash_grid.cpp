#include "spatial_hash_grid.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace KumariEngine::Physics {

SpatialHashGrid::SpatialHashGrid(float cellSize, uint32_t bucketCount)
    : m_cellSize(cellSize), m_nodeCount(0) {
    // Ensure bucketCount is a power of 2
    if ((bucketCount & (bucketCount - 1)) != 0) {
        bucketCount = 524288; // Default to 512k buckets
    }
    m_bucketMask = bucketCount - 1;
    m_bucketCount = bucketCount;
    m_buckets = std::make_unique<std::atomic<uint32_t>[]>(bucketCount);
    
    // Clear buckets
    Clear();
    
    // Pre-allocate node storage for 1,000,000 grid cell assignments
    m_nodes.resize(1000000);
}

void SpatialHashGrid::Clear() {
    m_nodeCount.store(0, std::memory_order_relaxed);
    
    // Fast clear of atomic array using memset
    std::memset(reinterpret_cast<void*>(m_buckets.get()), 0xFF, m_bucketCount * sizeof(uint32_t));
}

void SpatialHashGrid::Resize(uint32_t maxEntities) {
    size_t targetNodes = static_cast<size_t>(maxEntities) * 8;
    if (m_nodes.size() < targetNodes) {
        m_nodes.resize(targetNodes);
    }
}

void SpatialHashGrid::Insert(uint32_t entity, const AABB& aabb) {
    int32_t minX = static_cast<int32_t>(std::floor(aabb.min.x / m_cellSize));
    int32_t minY = static_cast<int32_t>(std::floor(aabb.min.y / m_cellSize));
    int32_t minZ = static_cast<int32_t>(std::floor(aabb.min.z / m_cellSize));

    int32_t maxX = static_cast<int32_t>(std::floor(aabb.max.x / m_cellSize));
    int32_t maxY = static_cast<int32_t>(std::floor(aabb.max.y / m_cellSize));
    int32_t maxZ = static_cast<int32_t>(std::floor(aabb.max.z / m_cellSize));

    // Clamp bounds to prevent excessive cell overlaps
    if (maxX - minX > 8) maxX = minX + 8;
    if (maxY - minY > 8) maxY = minY + 8;
    if (maxZ - minZ > 8) maxZ = minZ + 8;

    for (int32_t cx = minX; cx <= maxX; ++cx) {
        for (int32_t cy = minY; cy <= maxY; ++cy) {
            for (int32_t cz = minZ; cz <= maxZ; ++cz) {
                uint32_t idx = m_nodeCount.fetch_add(1, std::memory_order_relaxed);
                if (idx >= m_nodes.size()) {
                    // Node pool overflow, skip this cell assignment
                    continue;
                }

                uint32_t bucket = HashCell(cx, cy, cz);
                uint32_t oldHead = m_buckets[bucket].load(std::memory_order_relaxed);
                
                do {
                    m_nodes[idx].entity = entity;
                    m_nodes[idx].cx = cx;
                    m_nodes[idx].cy = cy;
                    m_nodes[idx].cz = cz;
                    m_nodes[idx].next = oldHead;
                } while (!m_buckets[bucket].compare_exchange_weak(
                    oldHead, idx, std::memory_order_release, std::memory_order_relaxed));
            }
        }
    }
}

uint32_t SpatialHashGrid::Query(const AABB& aabb, uint32_t* results, uint32_t maxResults, uint32_t selfEntity) const {
    int32_t minX = static_cast<int32_t>(std::floor(aabb.min.x / m_cellSize));
    int32_t minY = static_cast<int32_t>(std::floor(aabb.min.y / m_cellSize));
    int32_t minZ = static_cast<int32_t>(std::floor(aabb.min.z / m_cellSize));

    int32_t maxX = static_cast<int32_t>(std::floor(aabb.max.x / m_cellSize));
    int32_t maxY = static_cast<int32_t>(std::floor(aabb.max.y / m_cellSize));
    int32_t maxZ = static_cast<int32_t>(std::floor(aabb.max.z / m_cellSize));

    // Clamp bounds to prevent excessive query iterations
    if (maxX - minX > 8) maxX = minX + 8;
    if (maxY - minY > 8) maxY = minY + 8;
    if (maxZ - minZ > 8) maxZ = minZ + 8;

    uint32_t count = 0;

    for (int32_t cx = minX; cx <= maxX; ++cx) {
        for (int32_t cy = minY; cy <= maxY; ++cy) {
            for (int32_t cz = minZ; cz <= maxZ; ++cz) {
                uint32_t bucket = HashCell(cx, cy, cz);
                uint32_t nodeIdx = m_buckets[bucket].load(std::memory_order_acquire);

                while (nodeIdx != EMPTY_BUCKET && nodeIdx < m_nodes.size()) {
                    const GridNode& node = m_nodes[nodeIdx];
                    if (node.cx == cx && node.cy == cy && node.cz == cz && node.entity != selfEntity) {
                        // Check for duplicate in result list (O(K) check, where K is small)
                        bool isDuplicate = false;
                        for (uint32_t i = 0; i < count; ++i) {
                            if (results[i] == node.entity) {
                                isDuplicate = true;
                                break;
                            }
                        }
                        if (!isDuplicate) {
                            results[count++] = node.entity;
                            if (count >= maxResults) {
                                return count;
                            }
                        }
                    }
                    nodeIdx = node.next;
                }
            }
        }
    }

    return count;
}

} // namespace KumariEngine::Physics
