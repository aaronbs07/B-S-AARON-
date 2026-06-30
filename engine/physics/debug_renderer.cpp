#include "debug_renderer.hpp"
#include "debug_shader_bytecode.hpp"
#include "core/logger.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace KumariEngine::Physics {

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

bool PhysicsDebugRenderer::Initialize(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice) {
    if (m_initialized) return true;
    if (device == VK_NULL_HANDLE) {
        m_initialized = true;
        return true;
    }

    Core::Logger::Info("PhysicsDebugRenderer", "Initializing Vulkan Physics Debug Renderer...");

    // 1. Create dynamic vertex buffer (Host-Visible, Coherent for fast map-once updates)
    VkDeviceSize bufferSize = m_maxVertices * sizeof(DebugVertex);
    if (!CreateBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       m_vertexBuffer, m_vertexBufferMemory)) {
        Core::Logger::Error("PhysicsDebugRenderer", "Failed to create vertex buffer.");
        return false;
    }

    // Map the buffer permanently
    vkMapMemory(device, m_vertexBufferMemory, 0, bufferSize, 0, &m_mappedData);

    // 2. Build graphics pipeline for lines drawing
    VkShaderModule vertShaderModule = CreateShaderModule(device, debugVertShaderCode, sizeof(debugVertShaderCode));
    VkShaderModule fragShaderModule = CreateShaderModule(device, debugFragShaderCode, sizeof(debugFragShaderCode));

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        Core::Logger::Error("PhysicsDebugRenderer", "Failed to create Vulkan debug shader modules.");
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

    // Vertex input details
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(DebugVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2]{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(DebugVertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(DebugVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; // Line rendering
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport
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

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE; // Force outline line rendering
    rasterizer.lineWidth = 2.0f;                  // Sleek thicker lines
    rasterizer.cullMode = VK_CULL_MODE_NONE;       // Do not cull double-sided lines
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

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

    // Push constant range for ViewProj matrix
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        Core::Logger::Error("PhysicsDebugRenderer", "Failed to create pipeline layout.");
        return false;
    }

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

    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    if (res != VK_SUCCESS) {
        Core::Logger::Error("PhysicsDebugRenderer", "Failed to create line graphics pipeline. Result: %d", res);
        return false;
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    m_initialized = true;
    m_enabled = true; // Enabled by default
    Core::Logger::Info("PhysicsDebugRenderer", "Physics Debug Renderer initialized successfully.");
    return true;
}

void PhysicsDebugRenderer::Shutdown(VkDevice device) {
    if (!m_initialized) return;
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        if (m_mappedData) {
            vkUnmapMemory(device, m_vertexBufferMemory);
            m_mappedData = nullptr;
        }

        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }

        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }

        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
    }
    m_initialized = false;
}

void PhysicsDebugRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) {
    if (!m_enabled) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_submittedVertices.size() + 2 > m_maxVertices) return;

    m_submittedVertices.push_back(DebugVertex{start, color});
    m_submittedVertices.push_back(DebugVertex{end, color});
}

void PhysicsDebugRenderer::DrawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color) {
    glm::vec3 c0(min.x, min.y, min.z);
    glm::vec3 c1(max.x, min.y, min.z);
    glm::vec3 c2(max.x, max.y, min.z);
    glm::vec3 c3(min.x, max.y, min.z);

    glm::vec3 c4(min.x, min.y, max.z);
    glm::vec3 c5(max.x, min.y, max.z);
    glm::vec3 c6(max.x, max.y, max.z);
    glm::vec3 c7(min.x, max.y, max.z);

    // Bottom face
    DrawLine(c0, c1, color); DrawLine(c1, c5, color); DrawLine(c5, c4, color); DrawLine(c4, c0, color);
    // Top face
    DrawLine(c3, c2, color); DrawLine(c2, c6, color); DrawLine(c6, c7, color); DrawLine(c7, c3, color);
    // Vertical edges
    DrawLine(c0, c3, color); DrawLine(c1, c2, color); DrawLine(c5, c6, color); DrawLine(c4, c7, color);
}

void PhysicsDebugRenderer::DrawSphere(const glm::vec3& center, float radius, const glm::vec3& color) {
    constexpr int segments = 16;
    constexpr float step = 2.0f * 3.14159265f / static_cast<float>(segments);

    // Draw 3 orthogonal rings
    for (int i = 0; i < segments; ++i) {
        float theta1 = static_cast<float>(i) * step;
        float theta2 = static_cast<float>(i + 1) * step;

        float cos1 = std::cos(theta1); float sin1 = std::sin(theta1);
        float cos2 = std::cos(theta2); float sin2 = std::sin(theta2);

        // XY Ring
        DrawLine(center + glm::vec3(cos1 * radius, sin1 * radius, 0.0f),
                 center + glm::vec3(cos2 * radius, sin2 * radius, 0.0f), color);

        // XZ Ring
        DrawLine(center + glm::vec3(cos1 * radius, 0.0f, sin1 * radius),
                 center + glm::vec3(cos2 * radius, 0.0f, sin2 * radius), color);

        // YZ Ring
        DrawLine(center + glm::vec3(0.0f, cos1 * radius, sin1 * radius),
                 center + glm::vec3(0.0f, cos2 * radius, sin2 * radius), color);
    }
}

void PhysicsDebugRenderer::DrawCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color) {
    glm::vec3 topCenter = center + glm::vec3(0.0f, halfHeight, 0.0f);
    glm::vec3 botCenter = center - glm::vec3(0.0f, halfHeight, 0.0f);

    // Draw end caps (spheres)
    DrawSphere(topCenter, radius, color);
    DrawSphere(botCenter, radius, color);

    // Vertical connecting lines
    DrawLine(topCenter + glm::vec3(radius, 0.0f, 0.0f), botCenter + glm::vec3(radius, 0.0f, 0.0f), color);
    DrawLine(topCenter + glm::vec3(-radius, 0.0f, 0.0f), botCenter + glm::vec3(-radius, 0.0f, 0.0f), color);
    DrawLine(topCenter + glm::vec3(0.0f, 0.0f, radius), botCenter + glm::vec3(0.0f, 0.0f, radius), color);
    DrawLine(topCenter + glm::vec3(0.0f, 0.0f, -radius), botCenter + glm::vec3(0.0f, 0.0f, -radius), color);
}

void PhysicsDebugRenderer::DrawContactPoint(const glm::vec3& pos, const glm::vec3& normal, float penetration, const glm::vec3& color) {
    // Normal line
    DrawLine(pos, pos + normal * (penetration + 0.5f), color);

    // Small cross
    float delta = 0.05f;
    DrawLine(pos - glm::vec3(delta, 0.0f, 0.0f), pos + glm::vec3(delta, 0.0f, 0.0f), color);
    DrawLine(pos - glm::vec3(0.0f, delta, 0.0f), pos + glm::vec3(0.0f, delta, 0.0f), color);
    DrawLine(pos - glm::vec3(0.0f, 0.0f, delta), pos + glm::vec3(0.0f, 0.0f, delta), color);
}

void PhysicsDebugRenderer::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_submittedVertices.clear();
}

void PhysicsDebugRenderer::Draw(VkCommandBuffer commandBuffer, const glm::mat4& viewProj) {
    if (!m_enabled || !m_initialized) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = static_cast<uint32_t>(m_submittedVertices.size());
    if (count == 0) return;

    count = std::min(count, m_maxVertices);

    // Upload vertices to permanently mapped coherent memory
    if (m_mappedData) {
        std::memcpy(m_mappedData, m_submittedVertices.data(), count * sizeof(DebugVertex));
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        // ViewProj push constants
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &viewProj);

        VkBuffer vertexBuffers[] = { m_vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDraw(commandBuffer, count, 1, 0, 0);
    }
}

bool PhysicsDebugRenderer::CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, 
                                        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                                        VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
    return true;
}

uint32_t PhysicsDebugRenderer::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

} // namespace KumariEngine::Physics
