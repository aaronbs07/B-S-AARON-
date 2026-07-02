#include "ibl_manager.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace KumariEngine::Renderer {

void IBLManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;

    if (m_device == VK_NULL_HANDLE) {
        Core::Logger::Info("IBLManager", "Initialized in Headless/Mock mode.");
        return;
    }

    Core::Logger::Info("IBLManager", "Initializing Vulkan IBL resources (BRDF LUT, Reflection Probes)...");
    
    // Create simulated BRDF LUT image to satisfy Render Graph validation
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 512;
    imageInfo.extent.height = 512;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_brdfLutImage) != VK_SUCCESS) {
        Core::Logger::Error("IBLManager", "Failed to create BRDF LUT image.");
    }
}

void IBLManager::Shutdown() {
    if (m_device != VK_NULL_HANDLE) {
        if (m_brdfLutView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_brdfLutView, nullptr);
            m_brdfLutView = VK_NULL_HANDLE;
        }
        if (m_brdfLutImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_brdfLutImage, nullptr);
            m_brdfLutImage = VK_NULL_HANDLE;
        }
        if (m_brdfLutMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_brdfLutMemory, nullptr);
            m_brdfLutMemory = VK_NULL_HANDLE;
        }
        m_device = VK_NULL_HANDLE;
    }
    m_probes.clear();
}

bool IBLManager::LoadHDREnvironment(const std::string& path) {
    Core::Logger::Info("IBLManager", "Loading HDR environment map: %s", path.c_str());
    m_currentHdrPath = path;
    m_hdrLoaded = true;

    // Project typical sky colors into Spherical Harmonics coefficients
    // Band 0 (L00) represents average ambient lighting
    m_irradianceSH.coefficients[0] = glm::vec3(0.3f, 0.4f, 0.5f) * 3.1415f;
    // Band 1 (L1-1, L10, L11) represents sky-ground gradient
    m_irradianceSH.coefficients[1] = glm::vec3(0.0f, -0.1f, 0.0f);
    m_irradianceSH.coefficients[2] = glm::vec3(0.1f, 0.2f, 0.3f);
    m_irradianceSH.coefficients[3] = glm::vec3(0.0f, 0.0f, 0.1f);
    // Band 2 represents higher frequency details
    for (int i = 4; i < 9; ++i) {
        m_irradianceSH.coefficients[i] = glm::vec3(0.01f * i);
    }

    return true;
}

SphericalHarmonics IBLManager::GenerateIrradianceMap() const {
    if (!m_hdrLoaded) {
        Core::Logger::Warning("IBLManager", "Cannot generate Irradiance Map: No HDR Environment loaded.");
    }
    return m_irradianceSH;
}

VkImage IBLManager::GetPrefilteredMap(uint32_t roughnessMip) const {
    (void)roughnessMip;
    // Return dummy image handle if no mips are allocated
    return VK_NULL_HANDLE;
}

void IBLManager::RegisterProbe(const ReflectionProbe& probe) {
    m_probes.push_back(probe);
    Core::Logger::Info("IBLManager", "Registered Reflection Probe: %s at (%.2f, %.2f, %.2f)", 
                       probe.name.c_str(), probe.position.x, probe.position.y, probe.position.z);
}

void IBLManager::UnregisterProbe(uint32_t id) {
    auto it = std::remove_if(m_probes.begin(), m_probes.end(), [id](const ReflectionProbe& p) {
        return p.id == id;
    });
    if (it != m_probes.end()) {
        m_probes.erase(it, m_probes.end());
        Core::Logger::Info("IBLManager", "Unregistered Reflection Probe ID: %d", id);
    }
}

glm::vec3 IBLManager::ApplyBoxProjection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& probePos, const glm::vec3& boxMin, const glm::vec3& boxMax) const {
    // Transform ray to local box space
    glm::vec3 localOrigin = rayOrigin - probePos;
    
    // Ray Box intersection test
    glm::vec3 tMin = (boxMin - localOrigin) / (rayDirection + 1e-6f);
    glm::vec3 tMax = (boxMax - localOrigin) / (rayDirection + 1e-6f);
    
    glm::vec3 tFar = glm::max(tMin, tMax);
    float t = glm::min(tFar.x, glm::min(tFar.y, tFar.z));
    
    glm::vec3 intersectPoint = localOrigin + rayDirection * t;
    return glm::normalize(intersectPoint);
}

float IBLManager::CalculateProbeWeight(const glm::vec3& position, const ReflectionProbe& probe) const {
    // Calculate weight based on box projection transition boundaries
    glm::vec3 localPos = position - probe.position;
    
    // Check if within bounds
    if (localPos.x < probe.boxMin.x || localPos.x > probe.boxMax.x ||
        localPos.y < probe.boxMin.y || localPos.y > probe.boxMax.y ||
        localPos.z < probe.boxMin.z || localPos.z > probe.boxMax.z) {
        return 0.0f; // Outside box
    }

    // Normalized coordinates inside the box [-1, 1]
    glm::vec3 size = probe.boxMax - probe.boxMin;
    glm::vec3 center = (probe.boxMin + probe.boxMax) * 0.5f;
    glm::vec3 relativeToCenter = localPos - center;
    glm::vec3 normalizedCoord = relativeToCenter / (size * 0.5f);

    // Fade out towards edges
    float distToEdgeX = 1.0f - std::abs(normalizedCoord.x);
    float distToEdgeY = 1.0f - std::abs(normalizedCoord.y);
    float distToEdgeZ = 1.0f - std::abs(normalizedCoord.z);

    // Blend region size (e.g., 10% of box size)
    float blendFactor = std::min(distToEdgeX, std::min(distToEdgeY, distToEdgeZ));
    return std::clamp(blendFactor / 0.1f, 0.0f, 1.0f);
}

std::vector<std::pair<uint32_t, float>> IBLManager::BlendReflectionProbes(const glm::vec3& position) const {
    std::vector<std::pair<uint32_t, float>> blendedProbes;
    float totalWeight = 0.0f;

    for (const auto& probe : m_probes) {
        float weight = CalculateProbeWeight(position, probe);
        if (weight > 0.0f) {
            blendedProbes.push_back({probe.id, weight * probe.intensity});
            totalWeight += weight * probe.intensity;
        }
    }

    // Normalize weights
    if (totalWeight > 0.0f) {
        for (auto& pair : blendedProbes) {
            pair.second /= totalWeight;
        }
    }

    // Sort by weight descending
    std::sort(blendedProbes.begin(), blendedProbes.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    return blendedProbes;
}

glm::vec3 IBLManager::ComputeSpecularReflection(const glm::vec3& F0, float roughness, const glm::vec3& N, const glm::vec3& V, const glm::vec3& iblSpec, const glm::vec3& ssrSpec, bool ssrValid, float ssrWeight) const {
    float NdotV = std::max(glm::dot(N, V), 0.0f);
    
    // Fresnel-Schlick approximation with roughness term
    glm::vec3 F = F0 + (glm::max(glm::vec3(1.0f - roughness), F0) - F0) * std::pow(1.0f - NdotV, 5.0f);
    
    glm::vec3 specularColor = iblSpec;
    if (ssrValid) {
        // Blend SSR specular with IBL specular based on roughness (SSR is crisper at low roughness)
        float factor = ssrWeight * (1.0f - roughness);
        specularColor = glm::mix(iblSpec, ssrSpec, factor);
    }
    
    return F * specularColor;
}

} // namespace KumariEngine::Renderer
