#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <volk.h>

namespace KumariEngine::Renderer {

enum class SSRQuality {
    Low,
    Medium,
    High
};

struct SSRSettings {
    bool enabled = false;
    SSRQuality quality = SSRQuality::Medium;
    float thickness = 0.2f;            // Depth thickness threshold to qualify as hit
    int maxSteps = 32;                // Max iterations for ray marching
    float maxDistance = 40.0f;        // Max view space ray length
    float stride = 1.0f;              // Ray step increment size
    float temporalWeight = 0.90f;     // Accumulation weight of history frame
};

class SSRManager {
public:
    static SSRManager& Get() {
        static SSRManager instance;
        return instance;
    }

    SSRManager(const SSRManager&) = delete;
    SSRManager& operator=(const SSRManager&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    const SSRSettings& GetSettings() const { return m_settings; }
    SSRSettings& GetSettings() { return m_settings; }
    void SetSettings(const SSRSettings& settings) { m_settings = settings; }

    // Core Ray Marcher Math
    // Projects view-space point to UV space using projection matrix
    glm::vec2 ProjectToUV(const glm::vec3& viewSpacePoint, const glm::mat4& projectionMatrix) const;

    // Simulates ray marching across the depth buffer
    // Returns true if hit is found, writing out hitUV and hitDepth
    bool TraceRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& projMatrix, 
                  const std::vector<float>& depthBuffer, uint32_t width, uint32_t height,
                  glm::vec2& hitUV, float& hitDepth) const;

    // Temporal Reprojection calculation
    glm::vec2 ApplyTemporalReprojection(const glm::vec2& uv, const glm::vec2& velocity) const;

private:
    SSRManager() = default;
    ~SSRManager() = default;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    SSRSettings m_settings;
};

} // namespace KumariEngine::Renderer
