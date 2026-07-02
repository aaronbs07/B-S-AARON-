#include "ssr_manager.hpp"
#include "core/logger.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Renderer {

void SSRManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    Core::Logger::Info("SSRManager", "Initialized Screen Space Reflections manager.");
}

void SSRManager::Shutdown() {
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}

glm::vec2 SSRManager::ProjectToUV(const glm::vec3& viewSpacePoint, const glm::mat4& projectionMatrix) const {
    glm::vec4 ndc = projectionMatrix * glm::vec4(viewSpacePoint, 1.0f);
    if (std::abs(ndc.w) < 1e-6f) {
        return glm::vec2(0.0f);
    }
    glm::vec3 projected = glm::vec3(ndc) / ndc.w;
    
    // Convert NDC [-1, 1] to UV [0, 1]
    // Vulkan NDC Y is downward, so we might need mapping:
    return glm::vec2(projected.x, projected.y) * 0.5f + 0.5f;
}

bool SSRManager::TraceRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& projMatrix, 
                          const std::vector<float>& depthBuffer, uint32_t width, uint32_t height,
                          glm::vec2& hitUV, float& hitDepth) const {
    if (!m_settings.enabled || depthBuffer.empty() || width == 0 || height == 0) {
        return false;
    }

    // Step configuration depending on quality
    int steps = m_settings.maxSteps;
    if (m_settings.quality == SSRQuality::Low) {
        steps = std::min(steps, 16);
    } else if (m_settings.quality == SSRQuality::High) {
        steps = std::max(steps, 64);
    }

    float stepSize = m_settings.stride;
    glm::vec3 currentPoint = rayOrigin;

    for (int i = 0; i < steps; ++i) {
        // Step along view-space ray
        currentPoint += rayDir * stepSize;

        // Clip constraints
        float rayDepth = -currentPoint.z; // positive depth distance
        if (rayDepth <= 0.0f || rayDepth > m_settings.maxDistance) {
            return false;
        }

        // Project current ray point back to screen UV
        glm::vec2 uv = ProjectToUV(currentPoint, projMatrix);
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
            return false;
        }

        // Read linear depth from depth buffer
        uint32_t x = std::clamp(static_cast<uint32_t>(uv.x * width), 0u, width - 1);
        uint32_t y = std::clamp(static_cast<uint32_t>(uv.y * height), 0u, height - 1);
        float bufferDepth = depthBuffer[y * width + x];

        // Difference between ray depth and surface depth
        float depthDiff = rayDepth - bufferDepth;

        // If ray is behind the surface but within thickness, hit detected!
        if (depthDiff > 0.0f && depthDiff < m_settings.thickness) {
            hitUV = uv;
            hitDepth = rayDepth;
            return true;
        }
    }

    return false;
}

glm::vec2 SSRManager::ApplyTemporalReprojection(const glm::vec2& uv, const glm::vec2& velocity) const {
    // Current frame UV reprojected using motion vector to previous frame
    return uv - velocity;
}

} // namespace KumariEngine::Renderer
