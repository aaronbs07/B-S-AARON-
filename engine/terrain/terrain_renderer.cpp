#include "terrain_renderer.hpp"
#include "terrain_manager.hpp"
#include "terrain_shader_bytecode.hpp"
#include "camera/camera.hpp"
#include "core/logger.hpp"
#include <array>
#include <stdexcept>

namespace KumariEngine::Terrain {

TerrainRenderer::~TerrainRenderer() {
    // Pipeline resources must be cleaned up via Shutdown
}

bool TerrainRenderer::Initialize(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice) {
    Core::Logger::Info("TerrainRenderer", "Initializing Vulkan Terrain Renderer...");

    // Check if wireframe mode is supported
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    m_wireframeSupported = deviceFeatures.fillModeNonSolid;
    if (!m_wireframeSupported) {
        Core::Logger::Warning("TerrainRenderer", "Wireframe mode (fillModeNonSolid) not supported by GPU. Falling back to solid rendering.");
    }

    if (!CreatePipelineLayout(device)) return false;
    if (!CreatePipelines(device, renderPass)) return false;

    Core::Logger::Info("TerrainRenderer", "Vulkan Terrain Renderer initialized successfully.");
    return true;
}

void TerrainRenderer::Shutdown(VkDevice device) {
    Core::Logger::Info("TerrainRenderer", "Shutting down Vulkan Terrain Renderer...");

    if (m_solidPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_solidPipeline, nullptr);
        m_solidPipeline = VK_NULL_HANDLE;
    }

    if (m_wireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_wireframePipeline, nullptr);
        m_wireframePipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

bool TerrainRenderer::CreatePipelineLayout(VkDevice device) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TerrainPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS) {
        Core::Logger::Error("TerrainRenderer", "Failed to create pipeline layout. Result: %d", result);
        return false;
    }
    return true;
}

static VkShaderModule CreateShaderModule(VkDevice device, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

bool TerrainRenderer::CreatePipelines(VkDevice device, VkRenderPass renderPass) {
    VkShaderModule vertShaderModule = CreateShaderModule(device, terrainVertShaderCode, sizeof(terrainVertShaderCode));
    VkShaderModule fragShaderModule = CreateShaderModule(device, terrainFragShaderCode, sizeof(terrainFragShaderCode));

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        Core::Logger::Error("TerrainRenderer", "Failed to create Vulkan shader modules.");
        return false;
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Vertex Input Descriptions
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(TerrainVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(TerrainVertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(TerrainVertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(TerrainVertex, uv);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(TerrainVertex, biomeWeights);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and dynamic states
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 1. Create Solid Pipeline
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_solidPipeline);
    if (res != VK_SUCCESS) {
        Core::Logger::Error("TerrainRenderer", "Failed to create solid pipeline. Result: %d", res);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        return false;
    }

    // 2. Create Wireframe Pipeline
    if (m_wireframeSupported) {
        rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
        res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_wireframePipeline);
        if (res != VK_SUCCESS) {
            Core::Logger::Warning("TerrainRenderer", "Failed to create wireframe pipeline. Falling back to solid. Result: %d", res);
            m_wireframePipeline = m_solidPipeline;
        }
    } else {
        m_wireframePipeline = m_solidPipeline;
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    return true;
}

void TerrainRenderer::Draw(VkCommandBuffer commandBuffer, const glm::mat4& viewProj, const Camera::Camera* camera) {
    auto& tm = TerrainManager::Get();
    const auto& chunks = tm.GetActiveChunks();

    if (chunks.empty()) return;

    // Bind correct pipeline
    VkPipeline activePipeline = tm.IsWireframe() ? m_wireframePipeline : m_solidPipeline;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

    int debugMode = 0;
    if (tm.IsDebugVisEnabled()) {
        debugMode = 1; // LOD debug mode
    } else if (tm.IsChunkBordersEnabled()) {
        debugMode = 3; // Boundaries debug mode
    }

    // Bind Push Constants
    TerrainPushConstants push{};
    push.viewProj = viewProj;
    push.debugMode = debugMode;

    size_t visibleCount = 0;

    for (const auto& [coord, chunk] : chunks) {
        // Frustum culling check
        glm::vec3 minB = chunk->GetMinBounds();
        glm::vec3 maxB = chunk->GetMaxBounds();
        glm::vec3 center = (minB + maxB) * 0.5f;
        float radius = glm::distance(maxB, center);

        if (camera && !camera->IsSphereVisible(center, radius)) {
            continue; // Culled!
        }
        visibleCount++;

        push.lod = chunk->GetLOD();

        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TerrainPushConstants), &push);

        VkBuffer vertexBuffers[] = { chunk->GetVertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, chunk->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(commandBuffer, chunk->GetIndexCount(), 1, 0, 0, 0);
    }

    // Telemetry update: we can log or store how many chunks were drawn
    // (Could be stored or printed in overlay)
}

} // namespace KumariEngine::Terrain
