#include "vegetation_system.hpp"
#include "terrain_chunk.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Terrain {

VegetationSystem::VegetationSystem(uint32_t seed) : m_seed(seed) {
}

void VegetationSystem::Initialize(uint32_t seed) {
    m_seed = seed;
}

float VegetationSystem::HashToFloat(int x, int z, int index) const {
    uint32_t h = m_seed;
    h ^= static_cast<uint32_t>(x) * 73856093U;
    h ^= static_cast<uint32_t>(z) * 19349663U;
    h ^= static_cast<uint32_t>(index) * 83492791U;
    h = (h ^ (h >> 16)) * 2246822507U;
    h = (h ^ (h >> 13)) * 3266489917U;
    return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

void VegetationSystem::PopulateVegetation(TerrainChunk* chunk) const {
    const float size = chunk->GetSize();
    ChunkCoord coord = chunk->GetCoord();
    const NoiseGenerator* noiseGen = chunk->GetNoiseGenerator();
    if (!noiseGen) return;

    auto& vegetation = chunk->GetVegetation();
    vegetation.clear();

    // Spawning grid
    const int gridSize = 16;
    for (int sz = 0; sz < gridSize; ++sz) {
        for (int sx = 0; sx < gridSize; ++sx) {
            // Deterministic jitter
            float jx = HashToFloat(coord.x, coord.z, sx * 13 + sz * 37 + 1);
            float jz = HashToFloat(coord.x, coord.z, sx * 13 + sz * 37 + 2);

            float xPct = (static_cast<float>(sx) + jx) / static_cast<float>(gridSize);
            float zPct = (static_cast<float>(sz) + jz) / static_cast<float>(gridSize);

            float worldX = coord.x * size + xPct * size;
            float worldZ = coord.z * size + zPct * size;

            float y = noiseGen->GetHeight(worldX, worldZ);
            if (y <= noiseGen->params.seaLevel) {
                continue; // No vegetation underwater
            }

            // Estimate slope at candidate point
            float eps = 0.5f;
            float hL = noiseGen->GetHeight(worldX - eps, worldZ);
            float hR = noiseGen->GetHeight(worldX + eps, worldZ);
            float hD = noiseGen->GetHeight(worldX, worldZ - eps);
            float hU = noiseGen->GetHeight(worldX, worldZ + eps);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
            float slope = 1.0f - normal.y; // 0 on flat, 1 on cliff

            int biome = noiseGen->GetBiome(worldX, worldZ, y);
            float spawnChance = HashToFloat(coord.x, coord.z, sx * 17 + sz * 43 + 3);

            int vegType = -1; // -1 means none
            
            if (slope > 0.35f) {
                // Steep cliffs: only rocks
                if (spawnChance < 0.08f) {
                    vegType = 3; // Rock
                }
            } else {
                // Flat/gentle ground: biome-dependent
                if (biome == 2 || biome == 3) {
                    // Plains or Hills
                    if (spawnChance < 0.03f) {
                        vegType = 0; // Tree
                    } else if (spawnChance < 0.12f) {
                        vegType = 2; // Bush
                    } else if (spawnChance < 0.45f) {
                        vegType = 1; // Grass
                    } else if (spawnChance < 0.47f) {
                        vegType = 3; // Rock
                    }
                } else if (biome == 1) {
                    // Beach
                    if (spawnChance < 0.05f) {
                        vegType = 3; // Rock
                    } else if (spawnChance < 0.15f) {
                        vegType = 1; // Grass (sparse)
                    }
                } else if (biome == 4) {
                    // Mountains (gentle spots)
                    if (spawnChance < 0.10f) {
                        vegType = 3; // Rock
                    } else if (spawnChance < 0.25f) {
                        vegType = 1; // Alpine Grass
                    }
                }
            }

            if (vegType != -1) {
                float scale = HashToFloat(coord.x, coord.z, sx * 7 + sz * 19 + 4) * 0.6f + 0.7f;
                float rot = HashToFloat(coord.x, coord.z, sx * 7 + sz * 19 + 5) * 360.0f;

                // Adjust tree scales to be larger
                if (vegType == 0) {
                    scale *= 3.0f;
                }

                VegetationInstance inst;
                inst.type = vegType;
                inst.position = glm::vec3(worldX, y, worldZ);
                inst.scale = scale;
                inst.rotation = rot;

                vegetation.push_back(inst);
            }
        }
    }
}

} // namespace KumariEngine::Terrain
