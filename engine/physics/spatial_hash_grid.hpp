#pragma once
#include <vector>
#include <atomic>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include "physics_types.hpp"

namespace KumariEngine::Physics {

struct GridNode {
    uint32_t entity;
    int32_t cx, cy, cz;
    uint32_t next;
};

class SpatialHashGrid {
public:
    SpatialHashGrid(float cellSize = 2.0f, uint32_t bucketCount = 524288);
    ~SpatialHashGrid() = default;

    void Clear();
    void Insert(uint32_t entity, const AABB& aabb);

    // Queries overlapping entities. Writes up to maxResults into results array.
    // Returns number of unique overlapping entities found.
    uint32_t Query(const AABB& aabb, uint32_t* results, uint32_t maxResults, uint32_t selfEntity) const;

    float GetCellSize() const { return m_cellSize; }
    void Resize(uint32_t maxEntities);

private:
    inline uint32_t HashCell(int32_t cx, int32_t cy, int32_t cz) const {
        uint32_t h1 = 73856093;
        uint32_t h2 = 19349663;
        uint32_t h3 = 83492791;
        return ((static_cast<uint32_t>(cx) * h1) ^ 
                (static_cast<uint32_t>(cy) * h2) ^ 
                (static_cast<uint32_t>(cz) * h3)) & m_bucketMask;
    }

    float m_cellSize;
    uint32_t m_bucketMask;
    
    // Lock-free buckets and contiguous node storage
    std::unique_ptr<std::atomic<uint32_t>[]> m_buckets;
    uint32_t m_bucketCount;
    std::vector<GridNode> m_nodes;
    std::atomic<uint32_t> m_nodeCount;

    static constexpr uint32_t EMPTY_BUCKET = 0xFFFFFFFF;
};

} // namespace KumariEngine::Physics
