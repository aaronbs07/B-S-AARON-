#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <volk.h>

namespace KumariEngine::Renderer {

struct VolumetricFogSettings {
    bool enabled = false;
    float density = 0.02f;
    float anisotropy = 0.6f;          // Henyey-Greenstein g parameter [-1, 1]
    glm::vec3 scatteringColor{0.8f, 0.85f, 0.9f};
    float heightFalloff = 0.1f;
    float heightOffset = 0.0f;
};

struct LightShaftSettings {
    bool enabled = false;
    float density = 1.0f;
    float weight = 0.015f;
    float decay = 0.95f;
    float exposure = 0.5f;
    int samples = 32;
};

class VolumetricLightingManager {
public:
    static VolumetricLightingManager& Get() {
        static VolumetricLightingManager instance;
        return instance;
    }

    VolumetricLightingManager(const VolumetricLightingManager&) = delete;
    VolumetricLightingManager& operator=(const VolumetricLightingManager&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    const VolumetricFogSettings& GetFogSettings() const { return m_fogSettings; }
    VolumetricFogSettings& GetFogSettings() { return m_fogSettings; }
    void SetFogSettings(const VolumetricFogSettings& settings) { m_fogSettings = settings; }

    const LightShaftSettings& GetLightShaftSettings() const { return m_shaftSettings; }
    LightShaftSettings& GetLightShaftSettings() { return m_shaftSettings; }
    void SetLightShaftSettings(const LightShaftSettings& settings) { m_shaftSettings = settings; }

    // Phase Functions & Math
    float HenyeyGreensteinPhase(float cosTheta, float g) const;
    float CalculateHeightFog(float height) const;
    
    // Atmospheric scattering model integration
    glm::vec3 IntegrateAtmosphericScattering(const glm::vec3& rayDir, const glm::vec3& sunDir, 
                                             const glm::vec3& rayleighColor, const glm::vec3& mieColor) const;

    // Light shaft screen space sample generator
    std::vector<glm::vec2> ComputeLightShafts(const glm::vec2& pixelUV, const glm::vec2& sunScreenPos, 
                                              float& intensityOut) const;

private:
    VolumetricLightingManager() = default;
    ~VolumetricLightingManager() = default;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    VolumetricFogSettings m_fogSettings;
    LightShaftSettings m_shaftSettings;
};

} // namespace KumariEngine::Renderer
