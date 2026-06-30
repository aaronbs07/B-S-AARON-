#include "noise_generator.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Terrain {

NoiseGenerator::NoiseGenerator(uint32_t seed) {
    Initialize(seed);
}

void NoiseGenerator::Initialize(uint32_t seed) {
    m_seed = seed;
}

int NoiseGenerator::HashCoords(int x, int y) const {
    uint32_t h = m_seed;
    h ^= static_cast<uint32_t>(x) * 2654435761U;
    h ^= static_cast<uint32_t>(y) * 2246822519U;
    h = (h ^ (h >> 16)) * 2246822507U;
    h = (h ^ (h >> 13)) * 3266489917U;
    return static_cast<int>(h & 255);
}

float NoiseGenerator::Grad(int hash, float x, float y) const {
    switch (hash & 7) {
        case 0: return x;
        case 1: return -x;
        case 2: return y;
        case 3: return -y;
        case 4: return x + y;
        case 5: return -x + y;
        case 6: return x - y;
        case 7: return -x - y;
    }
    return 0.0f;
}

float NoiseGenerator::Noise2D(float x, float y) const {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    float u = Fade(fx);
    float v = Fade(fy);

    int aa = HashCoords(ix, iy);
    int ab = HashCoords(ix, iy + 1);
    int ba = HashCoords(ix + 1, iy);
    int bb = HashCoords(ix + 1, iy + 1);

    float x1 = Lerp(u, Grad(aa, fx, fy), Grad(ba, fx - 1.0f, fy));
    float x2 = Lerp(u, Grad(ab, fx, fy - 1.0f), Grad(bb, fx - 1.0f, fy - 1.0f));

    // Scale to approx [-1, 1]
    return Lerp(v, x1, x2) * 1.4142f;
}

float NoiseGenerator::Fbm2D(float x, float y, int octaves, float frequency, float amplitude, float lacunarity, float gain) const {
    float total = 0.0f;
    float maxVal = 0.0f;
    float freq = frequency;
    float amp = amplitude;

    for (int i = 0; i < octaves; ++i) {
        total += Noise2D(x * freq, y * freq) * amp;
        maxVal += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return total / maxVal;
}

float NoiseGenerator::RidgedNoise2D(float x, float y, int octaves, float frequency, float amplitude) const {
    float total = 0.0f;
    float maxVal = 0.0f;
    float freq = frequency;
    float amp = amplitude;
    float weight = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        float noise = Noise2D(x * freq, y * freq);
        noise = 1.0f - std::abs(noise); // Ridge
        noise = noise * noise;          // Sharp ridge
        noise *= weight;
        weight = noise * 1.5f;          // Feedback
        weight = std::clamp(weight, 0.0f, 1.0f);

        total += noise * amp;
        maxVal += amp;
        freq *= 2.0f;
        amp *= 0.5f;
    }

    return total / maxVal;
}

float NoiseGenerator::GetHeight(float x, float z) const {
    // Sample very low frequency noise for biomes
    float biomeNoise = Fbm2D(x, z, 3, 0.0003f, 1.0f);
    float biomeVal = biomeNoise * 0.5f + 0.5f; // Map to [0, 1]

    // Sample height profiles for different biomes
    float plainsH = Fbm2D(x, z, 3, 0.005f, 1.0f) * params.plainsScale + params.plainsOffset;
    float hillsH = Fbm2D(x, z, 4, 0.002f, 1.0f) * params.hillsScale + params.hillsOffset;
    float mountainH = RidgedNoise2D(x, z, 5, 0.001f, 1.0f) * params.mountainScale + params.mountainOffset;
    
    // Valley select
    float valleyNoise = Fbm2D(x, z, 3, 0.004f, 1.0f);
    float valleyH = valleyNoise * 8.0f - 4.0f;

    float height = 0.0f;

    // Blend biomes
    if (biomeVal < 0.3f) {
        // Blend plains and valleys
        float t = biomeVal / 0.3f;
        float baseH = (valleyNoise < -0.1f) ? valleyH : plainsH;
        height = Lerp(t, baseH, plainsH);
    } else if (biomeVal < 0.6f) {
        // Blend plains and hills
        float t = (biomeVal - 0.3f) / 0.3f;
        float smoothT = t * t * (3.0f - 2.0f * t);
        height = Lerp(smoothT, plainsH, hillsH);
    } else {
        // Blend hills and mountains
        float t = (biomeVal - 0.6f) / 0.4f;
        float smoothT = t * t * (3.0f - 2.0f * t);
        height = Lerp(smoothT, hillsH, mountainH);
    }

    // Carve Rivers
    float riverNoise = Fbm2D(x, z, 3, 0.0015f, 1.0f);
    float absRiver = std::abs(riverNoise);
    if (absRiver < 0.04f) {
        float riverWeight = 1.0f - (absRiver / 0.04f);
        // Smoothstep
        riverWeight = riverWeight * riverWeight * (3.0f - 2.0f * riverWeight);
        
        // Blend from current height to river depth
        height = Lerp(height, params.riverDepth, riverWeight);
    }

    // Coastline & Beach processing
    if (height < params.seaLevel) {
        // Underwater / Lake
        // Smooth lake bed
        float lakeBedDepth = params.riverDepth;
        float t = std::min(1.0f, std::abs(height - params.seaLevel) / 10.0f);
        height = Lerp(params.seaLevel, lakeBedDepth, t);
    } else if (height < params.beachThreshold) {
        // Flatten beach areas slightly for beach gameplay
        float t = (height - params.seaLevel) / (params.beachThreshold - params.seaLevel);
        height = Lerp(params.seaLevel + 0.2f, height, t);
    }

    return height;
}

int NoiseGenerator::GetBiome(float x, float z, float height) const {
    if (height < params.seaLevel) {
        return 0; // Ocean/Lake
    }
    if (height < params.beachThreshold) {
        return 1; // Beach
    }

    // River check
    float riverNoise = Fbm2D(x, z, 3, 0.0015f, 1.0f);
    if (std::abs(riverNoise) < 0.04f) {
        return 5; // River
    }

    float biomeNoise = Fbm2D(x, z, 3, 0.0003f, 1.0f);
    float biomeVal = biomeNoise * 0.5f + 0.5f;

    if (biomeVal < 0.3f) {
        // Valley selector
        float valleyNoise = Fbm2D(x, z, 3, 0.004f, 1.0f);
        if (valleyNoise < -0.1f) return 2; // Valley behaves like plains/lowland
        return 2; // Plains
    }
    if (biomeVal < 0.6f) {
        return 3; // Hills
    }
    return 4; // Mountains
}

} // namespace KumariEngine::Terrain
