#include "atmospheric_renderer.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Environment {

void AtmosphericRenderer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    Core::Logger::Info("AtmosphericRenderer", "Initialized Atmospheric Renderer.");
}

void AtmosphericRenderer::Shutdown() {
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}

void AtmosphericRenderer::UpdateDayNightCycle(float deltaTime) {
    if (!m_settings.enabled) return;

    m_settings.timeOfDay += m_settings.cycleSpeed * deltaTime;
    if (m_settings.timeOfDay >= 24.0f) {
        m_settings.timeOfDay -= 24.0f;
    } else if (m_settings.timeOfDay < 0.0f) {
        m_settings.timeOfDay += 24.0f;
    }

    // Dynamic color and intensity modulation based on sun height
    glm::vec3 sunDir = GetSunDirection();
    float sunHeight = sunDir.y;

    if (sunHeight > 0.1f) {
        // Day mode
        m_settings.sunIntensity = std::clamp(sunHeight / 0.5f, 0.0f, 1.0f);
        m_settings.moonIntensity = 0.0f;
    } else if (sunHeight < -0.1f) {
        // Night mode
        m_settings.sunIntensity = 0.0f;
        m_settings.moonIntensity = std::clamp(-sunHeight / 0.5f, 0.0f, 0.2f);
    } else {
        // Twilight transition
        float factor = (sunHeight + 0.1f) / 0.2f; // [0.0, 1.0]
        m_settings.sunIntensity = factor * 0.2f;
        m_settings.moonIntensity = (1.0f - factor) * 0.2f;
    }
}

glm::vec3 AtmosphericRenderer::GetSunDirection() const {
    // 0.0h = midnight, 12.0h = noon
    // Convert time [0, 24] to angle [0, 2pi]
    // Sun rises in East (+X), peaks at Zenith (+Y), sets in West (-X)
    float angle = ((m_settings.timeOfDay - 6.0f) / 24.0f) * 2.0f * 3.14159265f;
    
    // Rotate along the XY plane (or vertical sky dome hemisphere path)
    return glm::vec3(std::cos(angle), std::sin(angle), -0.2f); // Slight Z offset for slant
}

glm::vec3 AtmosphericRenderer::GetMoonDirection() const {
    // Moon is opposite to the sun
    return -GetSunDirection();
}

glm::vec3 AtmosphericRenderer::ComputeProceduralSkyColor(const glm::vec3& viewDir) const {
    glm::vec3 sunDir = GetSunDirection();
    float cosTheta = glm::dot(viewDir, sunDir);

    // Rayleigh Phase: 3 / (16 * pi) * (1 + cosTheta^2)
    float rayleighPhase = 3.0f / (16.0f * 3.14159265f) * (1.0f + cosTheta * cosTheta);

    // Mie Phase with turbidity scaling
    float g = 0.76f;
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    float miePhase = (1.0f - g2) / (4.0f * 3.14159265f * denom * std::sqrt(denom));

    float sunHeight = sunDir.y;

    // Zenith and horizon base colors
    glm::vec3 zenithColor = glm::vec3(0.12f, 0.28f, 0.65f);
    glm::vec3 horizonColor = glm::vec3(0.6f, 0.65f, 0.75f);

    // Color shifts for twilight and night
    if (sunHeight < 0.2f && sunHeight > -0.2f) {
        float factor = (sunHeight + 0.2f) / 0.4f; // [0, 1]
        // Twilight orange/pink horizon
        horizonColor = glm::mix(glm::vec3(0.85f, 0.35f, 0.15f), horizonColor, factor);
        zenithColor = glm::mix(glm::vec3(0.01f, 0.02f, 0.08f), zenithColor, factor);
    } else if (sunHeight <= -0.2f) {
        // Deep night sky
        zenithColor = glm::vec3(0.002f, 0.004f, 0.01f);
        horizonColor = glm::vec3(0.005f, 0.008f, 0.015f);
    }

    // Sky gradient from horizon to zenith affected by Rayleigh scattering
    float altitude = std::max(viewDir.y, 0.0f);
    glm::vec3 skyColor = glm::mix(horizonColor, zenithColor, altitude) * (1.0f + rayleighPhase * 0.02f);

    // Add Mie scatter around sun (sun halo)
    if (sunHeight > -0.2f) {
        float fade = std::clamp((sunHeight + 0.2f) / 0.2f, 0.0f, 1.0f);
        skyColor += m_settings.sunColor * (miePhase * 0.1f * m_settings.turbidity) * m_settings.sunIntensity * fade;
    }

    // Add Moon glow if moon is visible
    glm::vec3 moonDir = GetMoonDirection();
    float cosMoon = glm::dot(viewDir, moonDir);
    float moonPhase = (1.0f - g2) / (4.0f * 3.14159265f * (1.0f + g2 - 2.0f * g * cosMoon) * std::sqrt(1.0f + g2 - 2.0f * g * cosMoon));
    if (moonDir.y > -0.2f) {
        float fade = std::clamp((moonDir.y + 0.2f) / 0.2f, 0.0f, 1.0f);
        skyColor += m_settings.moonColor * (moonPhase * 0.05f) * m_settings.moonIntensity * fade;
    }

    return skyColor;
}

float AtmosphericRenderer::CalculateCloudDensity(const glm::vec3& worldPos, float time) const {
    // 2D cloud coverage FBM approximation using sine/cosine combinations
    float speed = m_settings.cloudSpeed * time;
    
    // Detail frequencies
    float f1 = std::sin(worldPos.x * 0.0005f + speed) * std::cos(worldPos.z * 0.0005f + speed * 0.7f);
    float f2 = std::sin(worldPos.x * 0.002f - speed * 1.2f) * std::cos(worldPos.z * 0.0015f + speed) * 0.5f;
    float f3 = std::sin(worldPos.x * 0.008f + speed * 2.0f) * std::cos(worldPos.z * 0.007f - speed * 1.5f) * 0.25f;
    
    float noise = (f1 + f2 + f3) / 1.75f; // normalized to [-1, 1]
    noise = noise * 0.5f + 0.5f; // [0, 1]

    // Density is evaluated against cloud coverage threshold
    float coverageThreshold = 1.0f - m_settings.cloudCoverage;
    if (noise < coverageThreshold) {
        return 0.0f;
    }

    return std::clamp((noise - coverageThreshold) / (1.0f - coverageThreshold), 0.0f, 1.0f);
}

} // namespace KumariEngine::Environment
