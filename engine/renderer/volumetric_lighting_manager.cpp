#include "volumetric_lighting_manager.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Renderer {

void VolumetricLightingManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    Core::Logger::Info("VolumetricLightingManager", "Initialized Volumetric Lighting manager.");
}

void VolumetricLightingManager::Shutdown() {
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}

float VolumetricLightingManager::HenyeyGreensteinPhase(float cosTheta, float g) const {
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    if (denom <= 0.0f) return 1.0f / (4.0f * 3.14159265f);
    return (1.0f - g2) / (4.0f * 3.14159265f * denom * std::sqrt(denom));
}

float VolumetricLightingManager::CalculateHeightFog(float height) const {
    if (!m_fogSettings.enabled) return 0.0f;
    
    // Exponential height fog density formula
    float density = m_fogSettings.density * std::exp(-m_fogSettings.heightFalloff * (height - m_fogSettings.heightOffset));
    return std::clamp(density, 0.0f, 1.0f);
}

glm::vec3 VolumetricLightingManager::IntegrateAtmosphericScattering(const glm::vec3& rayDir, const glm::vec3& sunDir, 
                                                                   const glm::vec3& rayleighColor, const glm::vec3& mieColor) const {
    float cosTheta = glm::dot(rayDir, sunDir);
    
    // Rayleigh Phase: 3 / (16 * pi) * (1 + cosTheta^2)
    float phaseR = 3.0f / (16.0f * 3.14159265f) * (1.0f + cosTheta * cosTheta);
    
    // Mie Phase using Henyey-Greenstein scattering
    float phaseM = HenyeyGreensteinPhase(cosTheta, m_fogSettings.anisotropy);
    
    return rayleighColor * phaseR + mieColor * phaseM * m_fogSettings.scatteringColor;
}

std::vector<glm::vec2> VolumetricLightingManager::ComputeLightShafts(const glm::vec2& pixelUV, const glm::vec2& sunScreenPos, 
                                                                    float& intensityOut) const {
    std::vector<glm::vec2> samples;
    intensityOut = 0.0f;
    
    if (!m_shaftSettings.enabled) {
        return samples;
    }

    glm::vec2 deltaTexCoord = pixelUV - sunScreenPos;
    float dist = glm::length(deltaTexCoord);
    if (dist < 1e-5f) return samples;

    int numSamples = m_shaftSettings.samples;
    deltaTexCoord *= 1.0f / static_cast<float>(numSamples) * m_shaftSettings.density;

    glm::vec2 currentUV = pixelUV;
    float illuminationDecay = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
        currentUV -= deltaTexCoord;
        samples.push_back(currentUV);
        
        // Accumulate illumination intensity
        intensityOut += illuminationDecay * m_shaftSettings.weight;
        illuminationDecay *= m_shaftSettings.decay;
    }

    intensityOut *= m_shaftSettings.exposure;
    return samples;
}

} // namespace KumariEngine::Renderer
