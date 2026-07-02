#pragma once
#include <glm/glm.hpp>
#include <volk.h>
#include <string>

namespace KumariEngine::Environment {

struct AtmosphericSettings {
    bool enabled = true;
    float timeOfDay = 12.0f;           // Range [0.0, 24.0] hours
    float cycleSpeed = 0.1f;
    float turbidity = 3.0f;            // Haze/Aerosol density [1.0, 10.0]
    glm::vec3 groundAlbedo{0.1f, 0.1f, 0.1f};
    
    // Sun & Moon parameters
    glm::vec3 sunColor{1.0f, 0.9f, 0.75f};
    float sunIntensity = 1.0f;
    glm::vec3 moonColor{0.6f, 0.75f, 1.0f};
    float moonIntensity = 0.2f;

    // Cloud settings foundation
    float cloudCoverage = 0.4f;
    float cloudSpeed = 0.05f;
};

class AtmosphericRenderer {
public:
    static AtmosphericRenderer& Get() {
        static AtmosphericRenderer instance;
        return instance;
    }

    AtmosphericRenderer(const AtmosphericRenderer&) = delete;
    AtmosphericRenderer& operator=(const AtmosphericRenderer&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    const AtmosphericSettings& GetSettings() const { return m_settings; }
    AtmosphericSettings& GetSettings() { return m_settings; }
    void SetSettings(const AtmosphericSettings& settings) { m_settings = settings; }

    // Dynamic Updates
    void UpdateDayNightCycle(float deltaTime);

    // Light Position / Direction Getters
    glm::vec3 GetSunDirection() const;
    glm::vec3 GetMoonDirection() const;
    
    // Sky math: Preetham / Hosek-Wilkie procedural sky color at a given view direction
    glm::vec3 ComputeProceduralSkyColor(const glm::vec3& viewDir) const;

    // Dynamic cloud coverage simulation (FBM noise approximation)
    float CalculateCloudDensity(const glm::vec3& worldPos, float time) const;

private:
    AtmosphericRenderer() = default;
    ~AtmosphericRenderer() = default;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    AtmosphericSettings m_settings;
};

} // namespace KumariEngine::Environment
