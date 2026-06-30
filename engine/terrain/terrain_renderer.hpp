#pragma once
#include <volk.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace KumariEngine::Camera { class Camera; }

namespace KumariEngine::Terrain {

struct TerrainPushConstants {
    glm::mat4 viewProj;
    int debugMode; // 0: standard, 1: LOD, 2: normals, 3: boundaries
    int lod;
};

class TerrainRenderer {
public:
    TerrainRenderer() = default;
    ~TerrainRenderer();

    bool Initialize(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice);
    void Shutdown(VkDevice device);

    // Record draw commands for all visible terrain chunks
    void Draw(VkCommandBuffer commandBuffer, const glm::mat4& viewProj, const Camera::Camera* camera);

private:
    bool CreatePipelineLayout(VkDevice device);
    bool CreatePipelines(VkDevice device, VkRenderPass renderPass);

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_solidPipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;

    // Track if wireframe is supported (fillModeNonSolid)
    bool m_wireframeSupported = false;
};

} // namespace KumariEngine::Terrain
