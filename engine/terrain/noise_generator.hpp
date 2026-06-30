#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace KumariEngine::Terrain {

struct BiomeParameters {
    float plainsScale = 5.0f;
    float plainsOffset = 2.0f;
    float hillsScale = 25.0f;
    float hillsOffset = 15.0f;
    float mountainScale = 180.0f;
    float mountainOffset = 50.0f;
    float riverDepth = -6.0f;
    float seaLevel = 0.0f;
    float beachThreshold = 2.5f;
};

class NoiseGenerator {
public:
    NoiseGenerator() = default;
    explicit NoiseGenerator(uint32_t seed);

    void Initialize(uint32_t seed);
    uint32_t GetSeed() const { return m_seed; }

    // Raw 2D Perlin Noise in range [-1.0, 1.0]
    float Noise2D(float x, float y) const;
    
    // Fractal Brownian Motion (fBm) noise
    float Fbm2D(float x, float y, int octaves, float frequency, float amplitude, float lacunarity = 2.0f, float gain = 0.5f) const;

    // Ridged multifractal noise (useful for mountains)
    float RidgedNoise2D(float x, float y, int octaves, float frequency, float amplitude) const;

    // Get the final terrain height at (x, z)
    float GetHeight(float x, float z) const;

    // Get biome type at (x, z)
    // 0: Ocean/Lake, 1: Beach, 2: Plains, 3: Hills, 4: Mountains, 5: River
    int GetBiome(float x, float z, float height) const;

    // Expose config parameters
    BiomeParameters params;

private:
    uint32_t m_seed = 1337;

    // Helper functions for Perlin Noise
    float Fade(float t) const { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    float Lerp(float t, float a, float b) const { return a + t * (b - a); }
    float Grad(int hash, float x, float y) const;
    int HashCoords(int x, int y) const;
};

} // namespace KumariEngine::Terrain
