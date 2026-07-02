#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <volk.h>

namespace KumariEngine::Renderer {

// Spherical Harmonics (SH) coefficients for Irradiance Maps (L0, L1, L2 bands)
struct SphericalHarmonics {
    glm::vec3 coefficients[9]{
        glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
        glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
        glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f)
    };
};

struct ReflectionProbe {
    uint32_t id = 0;
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 boxMin{-10.0f};
    glm::vec3 boxMax{10.0f};
    float intensity = 1.0f;
    bool isDynamic = false;
    VkImage cubemapImage = VK_NULL_HANDLE;
    VkImageView cubemapView = VK_NULL_HANDLE;
};

class IBLManager {
public:
    static IBLManager& Get() {
        static IBLManager instance;
        return instance;
    }

    IBLManager(const IBLManager&) = delete;
    IBLManager& operator=(const IBLManager&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    // Environment Map & IBL Texture Generation
    bool LoadHDREnvironment(const std::string& path);
    SphericalHarmonics GenerateIrradianceMap() const;
    VkImage GetPrefilteredMap(uint32_t roughnessMip) const;
    VkImage GetBRDFLookUpTexture() const { return m_brdfLutImage; }

    // Reflection Probe Management
    void RegisterProbe(const ReflectionProbe& probe);
    void UnregisterProbe(uint32_t id);
    const std::vector<ReflectionProbe>& GetProbes() const { return m_probes; }
    void ClearProbes() { m_probes.clear(); }

    // Math: Box-Projected Reflection Vector
    glm::vec3 ApplyBoxProjection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& probePos, const glm::vec3& boxMin, const glm::vec3& boxMax) const;
    
    // Blending calculation for multiple probes
    float CalculateProbeWeight(const glm::vec3& position, const ReflectionProbe& probe) const;
    std::vector<std::pair<uint32_t, float>> BlendReflectionProbes(const glm::vec3& position) const;

    // Specular Reflection Blend: IBL Specular + SSR based on Roughness and Fresnel
    glm::vec3 ComputeSpecularReflection(const glm::vec3& F0, float roughness, const glm::vec3& N, const glm::vec3& V, const glm::vec3& iblSpec, const glm::vec3& ssrSpec, bool ssrValid, float ssrWeight) const;

private:
    IBLManager() = default;
    ~IBLManager() = default;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    std::string m_currentHdrPath;
    bool m_hdrLoaded = false;
    SphericalHarmonics m_irradianceSH;

    VkImage m_brdfLutImage = VK_NULL_HANDLE;
    VkDeviceMemory m_brdfLutMemory = VK_NULL_HANDLE;
    VkImageView m_brdfLutView = VK_NULL_HANDLE;

    std::vector<ReflectionProbe> m_probes;
    std::vector<VkImage> m_prefilteredMips;
};

} // namespace KumariEngine::Renderer
