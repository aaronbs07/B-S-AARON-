#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace KumariEngine::Terrain {

class TerrainChunk;

class VegetationSystem {
public:
    VegetationSystem() = default;
    explicit VegetationSystem(uint32_t seed);

    void Initialize(uint32_t seed);

    // Populate a chunk with vegetation based on its heightmap and slope
    void PopulateVegetation(TerrainChunk* chunk) const;

private:
    // Helper to generate deterministic pseudo-random float [0.0, 1.0] from coordinates
    float HashToFloat(int x, int z, int index) const;

    uint32_t m_seed = 42;
};

} // namespace KumariEngine::Terrain
