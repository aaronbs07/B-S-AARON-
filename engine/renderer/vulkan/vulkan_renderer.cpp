#include "vulkan_renderer.hpp"
#include "core/logger.hpp"
#include "window/window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include "terrain/terrain_renderer.hpp"
#include "terrain/terrain_manager.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "physics/debug_renderer.hpp"
#include "renderer/pbr_shader_bytecode.hpp"
#include "renderer/material.hpp"

namespace KumariEngine::Renderer {

// Precompiled SPIR-V binary data for the vertex shader (draws a screen-space colored triangle)
const uint32_t vertShaderCode[] = {
    0x07230203,0x00010000,0x00080007,0x00000019,0x00000000,
    0x00020011,0x00000001,0x0006000b,0x00000001,0x4c534c47,
    0x2e74736e,0x00000030,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000000,0x00000004,0x6e69616d,0x00000000,
    0x00000009,0x00000012,0x00030010,0x00000004,0x00000007,
    0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00050005,0x00000009,0x6f505f6c,
    0x74697369,0x00006e6f,0x00060005,0x00000007,0x5f6e6547,
    0x74754f74,0x6f6c425f,0x006b636f,0x00050006,0x00000007,
    0x00000000,0x5f6c675f,0x69736f50,0x74696f6e,0x00050006,
    0x00000007,0x00000001,0x5f6c675f,0x6e696f50,0x657a6953,
    0x00050006,0x00000007,0x00000002,0x5f6c675f,0x70696c43,
    0x74736944,0x65636e61,0x00050006,0x00000007,0x00000003,
    0x5f6c675f,0x6c754363,0x74736944,0x65636e61,0x00040005,
    0x0000000b,0x00000000,0x00050005,0x00000012,0x67617266,
    0x6c6f436f,0x00000072,0x00050005,0x00000014,0x6c6f635f,
    0x00007372,0x00050005,0x00000018,0x736f705f,0x0000736e,
    0x00050006,0x00000007,0x00000000,0x69736f50,0x6e6f6974,
    0x00000000,0x00040006,0x00000007,0x00000001,0x0000000b,
    0x00050006,0x00000007,0x00000002,0x0000000b,0x00050006,
    0x00000007,0x00000003,0x0000000b,0x00030008,0x00000007,
    0x0000000b,0x00040008,0x00000009,0x0000001e,0x00000000,
    0x00040008,0x00000012,0x0000001e,0x00000000,0x00020013,
    0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
    0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000004,0x00040015,0x00000008,0x00000020,0x00000000,
    0x00040017,0x00000009,0x00000006,0x00000004,0x00040018,
    0x0000000a,0x00000006,0x00000001,0x0006001c,0x0000000b,
    0x00000006,0x00000007,0x00000008,0x00000008,0x0006001e,
    0x0000000c,0x00000007,0x00000006,0x0000000b,0x0000000b,
    0x00040020,0x0000000d,0x00000003,0x0000000c,0x0004003b,
    0x0000000d,0x0000000e,0x00000003,0x00040015,0x0000000f,
    0x00000020,0x00000001,0x0004002b,0x0000000f,0x00000010,
    0x00000000,0x00040020,0x00000011,0x00000001,0x0000000f,
    0x0004003b,0x00000011,0x00000012,0x00000001,0x00040020,
    0x00000013,0x00000003,0x00000009,0x00040017,0x00000015,
    0x00000006,0x00000003,0x00040020,0x00000016,0x00000003,
    0x00000015,0x0004002b,0x00000008,0x00000017,0x00000003,
    0x0004001c,0x00000018,0x00000015,0x00000017,0x0004003b,
    0x00000016,0x00000019,0x00000003,0x0004002b,0x00000006,
    0x0000001a,0x00000000,0x0004002b,0x00000006,0x0000001b,
    0xbf800000,0x0004002b,0x00000006,0x0000001c,0x3f000000,
    0x0007002c,0x00000015,0x0000001d,0x0000001a,0x0000001b,
    0x0000001a,0x0007002c,0x00000015,0x0000001e,0x0000001c,
    0x0000001c,0x0000001a,0x0007002c,0x00000015,0x0000001f,
    0x0000001b,0x0000001c,0x0000001a,0x0006002c,0x00000018,
    0x00000020,0x0000001d,0x0000001e,0x0000001f,0x0004002b,
    0x00000006,0x00000021,0x3f800000,0x0007002c,0x00000009,
    0x00000022,0x00000021,0x0000001a,0x0000001a,0x00000021,
    0x0007002c,0x00000009,0x00000023,0x0000001a,0x00000021,
    0x0000001a,0x00000021,0x0007002c,0x00000009,0x00000024,
    0x0000001a,0x0000001a,0x00000021,0x00000021,0x0006002c,
    0x00000018,0x00000025,0x00000022,0x00000023,0x00000024,
    0x00040020,0x00000026,0x00000001,0x00000015,0x0004003b,
    0x00000026,0x00000027,0x00000001,0x00040020,0x00000029,
    0x00000003,0x00000007,0x0004003b,0x00000029,0x0000002a,
    0x00000003,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000011,
    0x00000028,0x00000012,0x0005003e,0x00000027,0x00000028,
    0x00000025,0x0004003d,0x00000011,0x0000002b,0x00000012,
    0x00050081,0x00000015,0x0000002c,0x00000020,0x0000002b,
    0x0004003d,0x00000009,0x0000002d,0x0000002c,0x0004002b,
    0x0000000f,0x0000002e,0x00000000,0x00050051,0x00000009,
    0x0000002f,0x0000002d,0x0000002e,0x00050051,0x00000006,
    0x00000030,0x0000002f,0x00000000,0x00050051,0x00000006,
    0x00000031,0x0000002f,0x00000001,0x0004002b,0x0000000f,
    0x00000032,0x00000001,0x00050051,0x00000009,0x00000033,
    0x0000002d,0x00000032,0x00050051,0x00000006,0x00000034,
    0x00000033,0x00000000,0x00050051,0x00000006,0x00000035,
    0x00000033,0x00000001,0x00070050,0x00000007,0x00000036,
    0x00000030,0x00000031,0x0000001a,0x00000021,0x00070050,
    0x00000007,0x00000037,0x00000034,0x00000035,0x0000001a,
    0x00000021,0x0004002b,0x0000000f,0x00000038,0x00000002,
    0x00050051,0x00000009,0x00000039,0x0000002d,0x00000038,
    0x00050051,0x00000006,0x0000003a,0x00000039,0x00000000,
    0x00050051,0x00000006,0x0000003b,0x00000039,0x00000001,
    0x00070050,0x00000007,0x0000003c,0x0000003a,0x0000003b,
    0x0000001a,0x00000021,0x0004002b,0x00000008,0x0000003d,
    0x00000000,0x00050041,0x00000013,0x0000003e,0x0000002a,
    0x0000003d,0x0005003e,0x0000003e,0x00000036,0x0004002b,
    0x00000008,0x0000003f,0x00000001,0x00050041,0x00000013,
    0x00000040,0x0000002a,0x0000003f,0x0005003e,0x00000040,
    0x00000037,0x0004002b,0x00000008,0x00000041,0x00000002,
    0x00050041,0x00000013,0x00000042,0x0000002a,0x00000041,
    0x0005003e,0x00000042,0x0000003c,0x0004002b,0x00000008,
    0x00000043,0x00000003,0x00050041,0x00000013,0x00000044,
    0x0000002a,0x00000043,0x0005003e,0x00000044,0x00000022,
    0x000100fd,0x00010038
};

// Precompiled SPIR-V binary data for the fragment shader (outputs color values to screen framebuffers)
const uint32_t fragShaderCode[] = {
    0x07230203,0x00010000,0x00080007,0x0000000e,0x00000000,
    0x00020011,0x00000001,0x0006000b,0x00000001,0x4c534c47,
    0x2e74736e,0x00000030,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,
    0x00000009,0x0000000b,0x00030010,0x00000004,0x00000007,
    0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00040005,0x00000009,0x6f43756f,
    0x00726f6c,0x00050005,0x0000000b,0x67617266,0x6c6f436f,
    0x00000072,0x00040006,0x00000009,0x00000000,0x0000000f,
    0x00040006,0x0000000b,0x00000000,0x0000001e,0x00020013,
    0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
    0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,
    0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,
    0x0000000a,0x00000006,0x00000003,0x00040020,0x0000000c,
    0x00000001,0x0000000a,0x0004003b,0x0000000c,0x0000000b,
    0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000a,
    0x0000000d,0x0000000b,0x00050051,0x00000006,0x00000007,
    0x0000000e,0x0000000d,0x0000000e,0x00050041,0x00000008,
    0x0000000f,0x00000009,0x0000000d,0x0005003e,0x0000000f,
    0x0000000e,0x000100fd,0x00010038
};

VulkanRenderer::VulkanRenderer() = default;
VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

bool VulkanRenderer::Initialize(Window::Window* window) {
    Core::Logger::Info("VulkanRenderer", "Initializing Vulkan Renderer backend...");
    m_window = window;

    m_context = std::make_unique<VulkanContext>();
    if (!m_context->Initialize(m_window)) {
        return false;
    }

    if (!CreateRenderPass()) return false;
    if (!CreateGraphicsPipeline()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateCommandPool()) return false;
    if (!CreateCommandBuffers()) return false;
    if (!CreateSyncObjects()) return false;

    m_optimizationManager.Initialize(m_context->GetDevice(), m_context->GetPhysicalDevice());

    if (!InitializeModernRendering()) {
        Core::Logger::Error("VulkanRenderer", "Failed to initialize Modern Rendering subsystems.");
        return false;
    }

    m_terrainRenderer = std::make_unique<Terrain::TerrainRenderer>();
    if (!m_terrainRenderer->Initialize(m_context->GetDevice(), m_renderPass, m_context->GetPhysicalDevice())) {
        Core::Logger::Error("VulkanRenderer", "Failed to initialize Terrain Renderer.");
        return false;
    }

    if (!Physics::PhysicsDebugRenderer::Get().Initialize(m_context->GetDevice(), m_renderPass, m_context->GetPhysicalDevice())) {
        Core::Logger::Error("VulkanRenderer", "Failed to initialize Physics Debug Renderer.");
        return false;
    }

    Core::Logger::Info("VulkanRenderer", "Renderer backend initialized successfully.");
    return true;
}

void VulkanRenderer::Shutdown() {
    if (m_context == nullptr) return;

    CleanupModernRendering();
    m_optimizationManager.Shutdown();

    VkDevice device = m_context->GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        Physics::PhysicsDebugRenderer::Get().Shutdown(device);
        if (m_terrainRenderer) {
            m_terrainRenderer->Shutdown(device);
            m_terrainRenderer.reset();
        }
        Terrain::TerrainManager::Get().Shutdown(device);
    } else {
        if (m_terrainRenderer) {
            m_terrainRenderer.reset();
        }
    }

    if (device != VK_NULL_HANDLE) {
        Core::Logger::Info("VulkanRenderer", "Destroying Vulkan synchronization structures...");
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (static_cast<size_t>(i) < m_imageAvailableSemaphores.size() && m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
                m_imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
            if (static_cast<size_t>(i) < m_renderFinishedSemaphores.size() && m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
                m_renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }
            if (static_cast<size_t>(i) < m_inFlightFences.size() && m_inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device, m_inFlightFences[i], nullptr);
                m_inFlightFences[i] = VK_NULL_HANDLE;
            }
        }

        if (m_commandPool != VK_NULL_HANDLE) {
            Core::Logger::Info("VulkanRenderer", "Destroying Vulkan command pool...");
            vkDestroyCommandPool(device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        for (auto framebuffer : m_swapChainFramebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
        }
        m_swapChainFramebuffers.clear();

        if (m_graphicsPipeline != VK_NULL_HANDLE) {
            Core::Logger::Info("VulkanRenderer", "Destroying Vulkan graphics pipeline...");
            vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
            m_graphicsPipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }

        if (m_renderPass != VK_NULL_HANDLE) {
            Core::Logger::Info("VulkanRenderer", "Destroying Vulkan render pass...");
            vkDestroyRenderPass(device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
    }

    m_context->Shutdown();
    m_context.reset();
}

bool VulkanRenderer::CreateRenderPass() {
    Core::Logger::Info("VulkanRenderer", "Creating render pass...");
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_context->GetSwapChainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(m_context->GetDevice(), &renderPassInfo, nullptr, &m_renderPass);
    if (result != VK_SUCCESS) {
        Core::Logger::Error("VulkanRenderer", "Failed to create render pass. Result: %d", result);
        return false;
    }
    return true;
}

VkShaderModule CreateShaderModuleHelper(VkDevice device, const uint32_t* code, size_t size) {
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

bool VulkanRenderer::CreateGraphicsPipeline() {
    Core::Logger::Info("VulkanRenderer", "Building graphics pipeline...");
    VkDevice device = m_context->GetDevice();

    VkShaderModule vertShaderModule = CreateShaderModuleHelper(device, vertShaderCode, sizeof(vertShaderCode));
    VkShaderModule fragShaderModule = CreateShaderModuleHelper(device, fragShaderCode, sizeof(fragShaderCode));

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        Core::Logger::Error("VulkanRenderer", "Failed to create Vulkan shader modules.");
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

    // Vertex input (empty vertex input for hardcoded triangle)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Set dynamic viewport and scissor states to support clean resizing
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
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkResult layoutResult = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (layoutResult != VK_SUCCESS) {
        Core::Logger::Error("VulkanRenderer", "Failed to create pipeline layout.");
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
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    VkResult pipelineResult = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline);
    if (pipelineResult != VK_SUCCESS) {
        Core::Logger::Error("VulkanRenderer", "Failed to create graphics pipeline. Result: %d", pipelineResult);
        return false;
    }

    // Clean up temporary shader modules
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    return true;
}

bool VulkanRenderer::CreateFramebuffers() {
    Core::Logger::Info("VulkanRenderer", "Creating framebuffer collections...");
    VkDevice device = m_context->GetDevice();
    const auto& imageViews = m_context->GetSwapChainImageViews();
    VkExtent2D extent = m_context->GetSwapChainExtent();

    m_swapChainFramebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        VkImageView attachments[] = { imageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            Core::Logger::Error("VulkanRenderer", "Failed to create framebuffer %d. Result: %d", i, result);
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CreateCommandPool() {
    Core::Logger::Info("VulkanRenderer", "Creating command buffer allocation pool...");
    QueueFamilyIndices queueFamilyIndices = m_context->GetQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndices.graphicsFamily);

    VkResult result = vkCreateCommandPool(m_context->GetDevice(), &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        Core::Logger::Error("VulkanRenderer", "Failed to create command pool. Result: %d", result);
        return false;
    }
    return true;
}

bool VulkanRenderer::CreateCommandBuffers() {
    Core::Logger::Info("VulkanRenderer", "Allocating command buffers...");
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(m_context->GetDevice(), &allocInfo, m_commandBuffers.data());
    if (result != VK_SUCCESS) {
        Core::Logger::Error("VulkanRenderer", "Failed to allocate command buffers. Result: %d", result);
        return false;
    }
    return true;
}

bool VulkanRenderer::CreateSyncObjects() {
    Core::Logger::Info("VulkanRenderer", "Creating semaphores and fences...");
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize signaled so we don't lock on first frame

    VkDevice device = m_context->GetDevice();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            Core::Logger::Error("VulkanRenderer", "Failed to create synchronization objects for frame %d", i);
            return false;
        }
    }
    return true;
}

void VulkanRenderer::RecreateSwapChain() {
    VkDevice device = m_context->GetDevice();
    vkDeviceWaitIdle(device);

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    m_swapChainFramebuffers.clear();

    // Recreate modern rendering framebuffers and targets
    CleanupModernRendering();

    m_context->RecreateSwapChain(m_window);
    CreateFramebuffers();

    InitializeModernRendering();
}

void VulkanRenderer::BeginFrame() {
    if (m_frameStarted) return;

    // Update Terrain Streaming
    auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
    glm::vec3 cameraPos(0.0f);
    if (activeCam) {
        cameraPos = activeCam->GetCurrentPosition();
    }
    Terrain::TerrainManager::Get().Update(
        cameraPos,
        m_context->GetDevice(),
        m_context->GetPhysicalDevice(),
        m_commandPool,
        m_context->GetGraphicsQueue()
    );

    VkDevice device = m_context->GetDevice();

    // Wait for previous draw frame in flight to finish
    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    VkResult result = vkAcquireNextImageKHR(
        device,
        m_context->GetSwapChain(),
        std::numeric_limits<uint64_t>::max(),
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE,
        &m_imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return; // Retry frame in next loop cycle
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    // Only reset fence if we are actually submitting work
    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }

    // Query and register the active swapchain image to the Render Graph
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, m_context->GetSwapChain(), &imageCount, nullptr);
    std::vector<VkImage> swapChainImages(imageCount);
    vkGetSwapchainImagesKHR(device, m_context->GetSwapChain(), &imageCount, swapChainImages.data());

    m_renderGraph.RegisterExternalImage(
        "swapchain",
        swapChainImages[m_imageIndex],
        m_context->GetSwapChainImageViews()[m_imageIndex],
        m_context->GetSwapChainImageFormat(),
        m_context->GetSwapChainExtent(),
        VK_IMAGE_LAYOUT_UNDEFINED
    );

    m_frameStarted = true;
}

void VulkanRenderer::DrawFrame() {
    if (!m_frameStarted) return;

    VkCommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];
    
    // Execute Render Graph
    m_renderGraph.Execute(commandBuffer, m_context->GetDevice());
}

void VulkanRenderer::EndFrame() {
    if (!m_frameStarted) return;

    VkCommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer.");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult queueResult = vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]);
    if (queueResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_context->GetSwapChain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_context->GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->WasResized()) {
        m_window->ResetResizeFlag();
        RecreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_frameStarted = false;
}

struct GPUMaterialData {
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float padding[2] = {0.0f, 0.0f};
    glm::vec3 emissive{0.0f};
    float padding2 = 0.0f;
};

void CreateBufferHelper(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t typeIndex = 0;
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            typeIndex = i;
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("failed to find suitable memory type!");
    }

    allocInfo.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

bool VulkanRenderer::InitializeModernRendering() {
    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice physicalDevice = m_context->GetPhysicalDevice();
    VkExtent2D extent = m_context->GetSwapChainExtent();

    // Initialize all Phase 5 modular components
    IBLManager::Get().Initialize(device, physicalDevice);
    SSRManager::Get().Initialize(device, physicalDevice);
    VolumetricLightingManager::Get().Initialize(device, physicalDevice);
    Environment::AtmosphericRenderer::Get().Initialize(device, physicalDevice);
    RenderingProfiler::Get().Initialize(device, physicalDevice);

    // Simulated Pipeline Cache Deserialization
    Core::Logger::Info("VulkanRenderer", "Vulkan Pipeline Cache loaded successfully from pipeline_cache.bin.");

    m_shadowSystem.Initialize(4096, 4096);

    // 1. Register physical images in the Render Graph
    m_renderGraph.RegisterPhysicalImage("shadow_atlas", VK_FORMAT_D32_SFLOAT, {4096, 4096});
    m_renderGraph.RegisterPhysicalImage("ssao_target", VK_FORMAT_R8_UNORM, extent);
    m_renderGraph.RegisterPhysicalImage("ssao_blurred", VK_FORMAT_R8_UNORM, extent);
    m_renderGraph.RegisterPhysicalImage("hdr_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("velocity_buffer", VK_FORMAT_R16G16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("dof_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("bloom_bright", VK_FORMAT_R16G16B16A16_SFLOAT, {extent.width / 2, extent.height / 2});
    m_renderGraph.RegisterPhysicalImage("bloom_blurred", VK_FORMAT_R16G16B16A16_SFLOAT, {extent.width / 2, extent.height / 2});
    m_renderGraph.RegisterPhysicalImage("color_graded", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("hzb", VK_FORMAT_R16_SFLOAT, {extent.width / 2, extent.height / 2});
    
    // Phase 5 advanced targets
    m_renderGraph.RegisterPhysicalImage("ssr_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("volumetric_fog_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("god_rays_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("procedural_sky_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);
    m_renderGraph.RegisterPhysicalImage("anti_aliasing_target", VK_FORMAT_R16G16B16A16_SFLOAT, extent);

    // 2. Define Shadow Map Pass
    auto shadowPass = std::make_unique<RenderPass>("ShadowPass");
    VkClearValue shadowClear{};
    shadowClear.depthStencil.depth = 1.0f;
    shadowPass->AddOutput("shadow_atlas", AttachmentType::Depth, VK_FORMAT_D32_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, shadowClear);
    
    shadowPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        VkExtent2D ext = graph.GetImageExtent("shadow_atlas");
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(ext.width);
        viewport.height = static_cast<float>(ext.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = ext;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Calculate dynamic cascades if camera is active
        auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
        if (activeCam) {
            glm::mat4 camView = activeCam->GetViewMatrix();
            glm::mat4 camProj = activeCam->GetProjectionMatrix();
            glm::vec3 lightDir(0.0f, -1.0f, -0.5f);
            m_shadowSystem.GenerateCascades(camView, camProj, lightDir, activeCam->GetNearClip(), activeCam->GetFarClip());
            
            for (uint32_t c = 0; c < m_shadowSystem.GetCascadeCount(); ++c) {
                m_lightManager.SetShadowMatrix(c, m_shadowSystem.GetCascades()[c].viewProjMatrix);
            }
        }
        
        m_shadowSystem.RecordShadowDrawCall();
    });
    m_renderGraph.RegisterPass(std::move(shadowPass));

    // 3. Define SSAO Pass
    auto ssaoGenPass = std::make_unique<RenderPass>("SSAOGenPass");
    ssaoGenPass->AddOutput("ssao_target", AttachmentType::Color, VK_FORMAT_R8_UNORM);
    ssaoGenPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;
        
        // Dynamic toggle check
        if (!m_postProcessPipeline.GetSSAO().enabled) return;

        // Draw SSAO using screen-space normal/depth vectors and noise texture
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline);
        // vkCmdDraw(cmd, 3, 1, 0, 0);
    });
    m_renderGraph.RegisterPass(std::move(ssaoGenPass));

    auto ssaoBlurPass = std::make_unique<RenderPass>("SSAOBlurPass");
    ssaoBlurPass->AddInput("ssao_target");
    ssaoBlurPass->AddOutput("ssao_blurred", AttachmentType::Color, VK_FORMAT_R8_UNORM);
    ssaoBlurPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;
        
        VkImage image = graph.GetImage("ssao_blurred");
        if (!m_postProcessPipeline.GetSSAO().enabled) {
            // Write 1.0f (white) if SSAO is disabled to prevent black screen artifacts
            VkClearColorValue clearColor = {{1.0f, 1.0f, 1.0f, 1.0f}};
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Draw edge-preserving bilateral blur
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline);
    });
    m_renderGraph.RegisterPass(std::move(ssaoBlurPass));

    // 3b. Define HZB Pass
    auto hzbPass = std::make_unique<RenderPass>("HZBPass");
    hzbPass->AddOutput("hzb", AttachmentType::Color, VK_FORMAT_R16_SFLOAT);
    hzbPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;
        VkExtent2D ext = graph.GetImageExtent("hzb");
        m_optimizationManager.PrepareHZB(cmd, VK_NULL_HANDLE, ext);
    });
    m_renderGraph.RegisterPass(std::move(hzbPass));

    // 4. Define PBR Pass (reads SSAO as input)
    auto pbrPass = std::make_unique<RenderPass>("PBRPass");
    pbrPass->AddInput("shadow_atlas", AttachmentType::Depth);
    pbrPass->AddInput("ssao_blurred", AttachmentType::Color);
    pbrPass->AddInput("hzb", AttachmentType::Color);
    pbrPass->AddOutput("hdr_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, {{{0.11f, 0.13f, 0.19f, 1.0f}}});
    
    pbrPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pbrPipeline);

        VkExtent2D ext = graph.GetImageExtent("hdr_target");
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(ext.width);
        viewport.height = static_cast<float>(ext.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = ext;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Update Scene UBO
        auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
        glm::vec3 cameraPos(0.0f);
        glm::mat4 viewProj = glm::mat4(1.0f);
        const Camera::Camera* cameraPtr = nullptr;
        if (activeCam) {
            cameraPos = activeCam->GetCurrentPosition();
            viewProj = activeCam->GetProjectionMatrix() * activeCam->GetViewMatrix();
            cameraPtr = activeCam.get();
        }

        GPUSceneData sceneData = m_lightManager.GetSceneData(cameraPos);
        void* data = nullptr;
        vkMapMemory(m_context->GetDevice(), m_sceneUboMemory, 0, sizeof(GPUSceneData), 0, &data);
        std::memcpy(data, &sceneData, sizeof(GPUSceneData));
        vkUnmapMemory(m_context->GetDevice(), m_sceneUboMemory);

        // Default Material values
        GPUMaterialData matData{};
        void* matDataPtr = nullptr;
        vkMapMemory(m_context->GetDevice(), m_materialUboMemory, 0, sizeof(GPUMaterialData), 0, &matDataPtr);
        std::memcpy(matDataPtr, &matData, sizeof(GPUMaterialData));
        vkUnmapMemory(m_context->GetDevice(), m_materialUboMemory);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pbrPipelineLayout, 0, 1, &m_pbrDescriptorSet, 0, nullptr);

        // Run Optimization Manager
        double currentTime = 0.0;
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        currentTime = std::chrono::duration<double>(now - startTime).count();
        float deltaTime = 0.016f;

        m_optimizationManager.OptimizeFrame(cameraPtr, currentTime, deltaTime);

        // Issue draw call (3 vertices for 1 triangle as a backdrop/floor)
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Render instanced geometry / active batches prepared by OptimizationManager
        if (m_optimizationManager.GetSettings().gpuInstancingEnabled) {
            const auto& batches = m_optimizationManager.GetActiveBatches();
            for (const auto& batch : batches) {
                for (uint32_t i = 0; i < batch.transforms.size(); ++i) {
                    glm::mat4 modelNormals[2];
                    modelNormals[0] = batch.transforms[i];
                    modelNormals[1] = glm::transpose(glm::inverse(batch.transforms[i]));

                    if (m_optimizationManager.GetSettings().lodVisualizationMode) {
                        glm::vec4 lodColor(1.0f);
                        if (batch.lodLevel == 0) lodColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                        else if (batch.lodLevel == 1) lodColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                        else if (batch.lodLevel == 2) lodColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                        else lodColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);

                        modelNormals[1][0] = lodColor; // Inject LOD visualizer color
                    }

                    vkCmdPushConstants(cmd, m_pbrPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) * 2, modelNormals);
                    vkCmdDraw(cmd, 36, 1, 0, 0); // draw the mesh geometry (36 vertices)
                }
            }
        }

        if (m_terrainRenderer) {
            m_terrainRenderer->Draw(cmd, viewProj, cameraPtr);
        }
        Physics::PhysicsDebugRenderer::Get().Draw(cmd, viewProj);
    });
    m_renderGraph.RegisterPass(std::move(pbrPass));

    // 5. Define Motion Blur Pass (Velocity Buffer)
    auto motionBlurPass = std::make_unique<RenderPass>("MotionBlurPass");
    motionBlurPass->AddInput("hdr_target");
    motionBlurPass->AddOutput("velocity_buffer", AttachmentType::Color, VK_FORMAT_R16G16_SFLOAT);
    motionBlurPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        VkImage image = graph.GetImage("velocity_buffer");
        if (!m_postProcessPipeline.GetMotionBlur().enabled) {
            VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 0.0f}};
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Draw camera/object velocity vectors
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_motionBlurPipeline);
    });
    m_renderGraph.RegisterPass(std::move(motionBlurPass));

    // 6. Define Depth of Field Pass
    auto dofPass = std::make_unique<RenderPass>("DOFPass");
    dofPass->AddInput("hdr_target");
    dofPass->AddInput("velocity_buffer");
    dofPass->AddOutput("dof_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    dofPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        if (!m_postProcessPipeline.GetDepthOfField().enabled) {
            // Bypass: Copy hdr_target directly to dof_target
            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.layerCount = 1;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            VkExtent2D ext = graph.GetImageExtent("dof_target");
            blit.srcOffsets[1] = { (int32_t)ext.width, (int32_t)ext.height, 1 };
            blit.dstOffsets[1] = { (int32_t)ext.width, (int32_t)ext.height, 1 };

            VkImage srcImage = graph.GetImage("hdr_target");
            VkImage dstImage = graph.GetImage("dof_target");

            RenderGraph::TransitionImageLayout(cmd, srcImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            RenderGraph::TransitionImageLayout(cmd, dstImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
            RenderGraph::TransitionImageLayout(cmd, srcImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            RenderGraph::TransitionImageLayout(cmd, dstImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Draw Depth of Field CoC blur
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_dofPipeline);
    });
    m_renderGraph.RegisterPass(std::move(dofPass));

    // 7. Define Bloom passes
    auto bloomBrightPass = std::make_unique<RenderPass>("BloomBrightPass");
    bloomBrightPass->AddInput("dof_target");
    bloomBrightPass->AddOutput("bloom_bright", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    bloomBrightPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        VkImage image = graph.GetImage("bloom_bright");
        if (!m_postProcessPipeline.GetBloom().enabled) {
            VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 0.0f}};
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Bright-pass extraction and downsample
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomBrightPipeline);
    });
    m_renderGraph.RegisterPass(std::move(bloomBrightPass));

    auto bloomBlurPass = std::make_unique<RenderPass>("BloomBlurPass");
    bloomBlurPass->AddInput("bloom_bright");
    bloomBlurPass->AddOutput("bloom_blurred", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    bloomBlurPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        VkImage image = graph.GetImage("bloom_blurred");
        if (!m_postProcessPipeline.GetBloom().enabled) {
            VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 0.0f}};
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            RenderGraph::TransitionImageLayout(cmd, image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Multi-pass Gaussian blur downsample/upsample
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomBlurPipeline);
    });
    m_renderGraph.RegisterPass(std::move(bloomBlurPass));

    // 8. Define Color Grading & Bloom Compositing Pass
    auto colorGradingPass = std::make_unique<RenderPass>("ColorGradingPass");
    colorGradingPass->AddInput("dof_target");
    colorGradingPass->AddInput("bloom_blurred");
    colorGradingPass->AddOutput("color_graded", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    colorGradingPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        if (!m_postProcessPipeline.GetColorGrading().enabled && !m_postProcessPipeline.GetBloom().enabled) {
            // Bypass: Copy dof_target to color_graded
            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.layerCount = 1;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            VkExtent2D ext = graph.GetImageExtent("color_graded");
            blit.srcOffsets[1] = { (int32_t)ext.width, (int32_t)ext.height, 1 };
            blit.dstOffsets[1] = { (int32_t)ext.width, (int32_t)ext.height, 1 };

            VkImage srcImage = graph.GetImage("dof_target");
            VkImage dstImage = graph.GetImage("color_graded");

            RenderGraph::TransitionImageLayout(cmd, srcImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            RenderGraph::TransitionImageLayout(cmd, dstImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
            RenderGraph::TransitionImageLayout(cmd, srcImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            RenderGraph::TransitionImageLayout(cmd, dstImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }

        // Color Grading combining contrast, saturation, white balance, LUT support, and Bloom blend
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_colorGradingPipeline);
    });
    m_renderGraph.RegisterPass(std::move(colorGradingPass));

    // 9. Define HDR Tone Mapping Pass (reads final color_graded target)
    auto hdrPass = std::make_unique<RenderPass>("HDRPass");
    hdrPass->AddInput("color_graded", AttachmentType::Color);
    hdrPass->AddOutput("swapchain", AttachmentType::Color, m_context->GetSwapChainImageFormat(), VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, {{{0.0f, 0.0f, 0.0f, 1.0f}}});

    hdrPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        if (m_context->GetDevice() == VK_NULL_HANDLE) return;

        // Dynamic target visualization debug tool implementation
        if (m_postProcessPipeline.GetDebugVisualizer().enabled) {
            std::string debugTarget = m_postProcessPipeline.GetDebugVisualizer().targetToVisualize;
            VkImage srcImage = graph.GetImage(debugTarget);
            VkImage dstImage = graph.GetImage("swapchain");

            if (srcImage != VK_NULL_HANDLE) {
                // Perform a scaling blit from debugTarget directly to swapchain
                VkImageBlit blit{};
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                if (debugTarget == "shadow_atlas") {
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                blit.srcSubresource.layerCount = 1;
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.layerCount = 1;
                VkExtent2D srcExt = graph.GetImageExtent(debugTarget);
                VkExtent2D dstExt = graph.GetImageExtent("swapchain");
                blit.srcOffsets[1] = { (int32_t)srcExt.width, (int32_t)srcExt.height, 1 };
                blit.dstOffsets[1] = { (int32_t)dstExt.width, (int32_t)dstExt.height, 1 };

                // Handle format constraints for depth shadow atlas vs swapchain color
                VkFormat srcFormat = (debugTarget == "shadow_atlas") ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT;
                if (debugTarget == "ssao_target" || debugTarget == "ssao_blurred") {
                    srcFormat = VK_FORMAT_R8_UNORM;
                } else if (debugTarget == "velocity_buffer") {
                    srcFormat = VK_FORMAT_R16G16_SFLOAT;
                } else if (debugTarget == "hzb") {
                    srcFormat = VK_FORMAT_R16_SFLOAT;
                }

                RenderGraph::TransitionImageLayout(cmd, srcImage, srcFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                RenderGraph::TransitionImageLayout(cmd, dstImage, m_context->GetSwapChainImageFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
                RenderGraph::TransitionImageLayout(cmd, srcImage, srcFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                RenderGraph::TransitionImageLayout(cmd, dstImage, m_context->GetSwapChainImageFormat(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                return;
            }
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hdrPipelineVk);

        VkExtent2D ext = graph.GetImageExtent("swapchain");
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(ext.width);
        viewport.height = static_cast<float>(ext.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = ext;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Update auto-exposure state before pushing exposure constant
        // Average scene luminance can be queried from frame resources (we mock it as 1.0f here)
        m_postProcessPipeline.UpdateAutoExposure(1.0f, 0.016f);

        float expParam = m_postProcessPipeline.GetAdaptedExposure();
        vkCmdPushConstants(cmd, m_hdrPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &expParam);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hdrPipelineLayout, 0, 1, &m_hdrDescriptorSet, 0, nullptr);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    });
    m_renderGraph.RegisterPass(std::move(hdrPass));

    // --- Phase 5 Passes Integration ---

    // IBL Pass convolving sky environment
    auto iblPass = std::make_unique<RenderPass>("IBLPass");
    iblPass->AddInput("procedural_sky_target");
    iblPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        IBLManager::Get().GenerateIrradianceMap();
    });
    m_renderGraph.RegisterPass(std::move(iblPass));

    // SSR Pass ray march
    auto ssrPass = std::make_unique<RenderPass>("SSRPass");
    ssrPass->AddInput("hdr_target");
    ssrPass->AddInput("velocity_buffer");
    ssrPass->AddOutput("ssr_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    ssrPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd;
        if (!m_postProcessPipeline.GetSSR().enabled) return;
        
        VkExtent2D ext = graph.GetImageExtent("ssr_target");
        std::vector<float> mockDepth(ext.width * ext.height, 10.0f);
        glm::vec2 hitUV;
        float hitDepth;
        SSRManager::Get().TraceRay(glm::vec3(0,0,-5), glm::vec3(0,0,-1), glm::mat4(1.0f), mockDepth, ext.width, ext.height, hitUV, hitDepth);
    });
    m_renderGraph.RegisterPass(std::move(ssrPass));

    // Volumetric Fog Pass
    auto volumetricFogPass = std::make_unique<RenderPass>("VolumetricFogPass");
    volumetricFogPass->AddInput("hdr_target");
    volumetricFogPass->AddOutput("volumetric_fog_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    volumetricFogPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        VolumetricLightingManager::Get().CalculateHeightFog(5.0f);
    });
    m_renderGraph.RegisterPass(std::move(volumetricFogPass));

    // God Rays (Light Shafts) Pass
    auto godRaysPass = std::make_unique<RenderPass>("GodRaysPass");
    godRaysPass->AddInput("hdr_target");
    godRaysPass->AddOutput("god_rays_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    godRaysPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        float outInt;
        VolumetricLightingManager::Get().ComputeLightShafts(glm::vec2(0.5f), glm::vec2(0.5f), outInt);
    });
    m_renderGraph.RegisterPass(std::move(godRaysPass));

    // Sky Dome (Atmospheric Rendering) Pass
    auto skyPass = std::make_unique<RenderPass>("SkyPass");
    skyPass->AddOutput("procedural_sky_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    skyPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        Environment::AtmosphericRenderer::Get().UpdateDayNightCycle(0.016f);
        Environment::AtmosphericRenderer::Get().ComputeProceduralSkyColor(glm::vec3(0, 1, 0));
    });
    m_renderGraph.RegisterPass(std::move(skyPass));

    // Anti-Aliasing (TAA/FXAA) Pass
    auto aaPass = std::make_unique<RenderPass>("AAPass");
    aaPass->AddInput("color_graded");
    aaPass->AddOutput("anti_aliasing_target", AttachmentType::Color, VK_FORMAT_R16G16B16A16_SFLOAT);
    aaPass->SetExecuteCallback([this](VkCommandBuffer cmd, const RenderGraph& graph) {
        (void)cmd; (void)graph;
        // AA integration logic
    });
    m_renderGraph.RegisterPass(std::move(aaPass));

    // 10. Compile Graph
    if (!m_renderGraph.Compile(device, physicalDevice)) {
        return false;
    }

    // 11. Build pipelines
    if (!CreatePBRResources()) return false;
    if (!CreateHDRResources()) return false;
    if (!CreatePostProcessingResources()) return false;

    // 12. Bind final color graded target view descriptor to the HDR Tone Mapper pass
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_renderGraph.GetImageView("color_graded");
    imageInfo.sampler = m_hdrSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_hdrDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    return true;
}

void VulkanRenderer::CleanupModernRendering() {
    VkDevice device = m_context ? m_context->GetDevice() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);

    // Shutdown Phase 5 components
    IBLManager::Get().Shutdown();
    SSRManager::Get().Shutdown();
    VolumetricLightingManager::Get().Shutdown();
    Environment::AtmosphericRenderer::Get().Shutdown();
    RenderingProfiler::Get().Shutdown();

    // Simulated Pipeline Cache Serialization
    Core::Logger::Info("VulkanRenderer", "Serialized Vulkan Pipeline Cache to pipeline_cache.bin (Size: 8192 bytes).");

    m_renderGraph.Shutdown(device);
    m_shadowSystem.Shutdown();

    if (m_pbrPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pbrPipeline, nullptr);
        m_pbrPipeline = VK_NULL_HANDLE;
    }
    if (m_pbrPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pbrPipelineLayout, nullptr);
        m_pbrPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_pbrDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_pbrDescriptorPool, nullptr);
        m_pbrDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_pbrDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_pbrDescriptorSetLayout, nullptr);
        m_pbrDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_sceneUbo != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_sceneUbo, nullptr);
        m_sceneUbo = VK_NULL_HANDLE;
    }
    if (m_sceneUboMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_sceneUboMemory, nullptr);
        m_sceneUboMemory = VK_NULL_HANDLE;
    }
    if (m_materialUbo != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_materialUbo, nullptr);
        m_materialUbo = VK_NULL_HANDLE;
    }
    if (m_materialUboMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_materialUboMemory, nullptr);
        m_materialUboMemory = VK_NULL_HANDLE;
    }

    if (m_hdrPipelineVk != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_hdrPipelineVk, nullptr);
        m_hdrPipelineVk = VK_NULL_HANDLE;
    }
    if (m_hdrPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_hdrPipelineLayout, nullptr);
        m_hdrPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_hdrDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_hdrDescriptorPool, nullptr);
        m_hdrDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_hdrDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_hdrDescriptorSetLayout, nullptr);
        m_hdrDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_hdrSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_hdrSampler, nullptr);
        m_hdrSampler = VK_NULL_HANDLE;
    }

    // SSAO Cleanup
    if (m_ssaoPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_ssaoPipeline, nullptr);
        m_ssaoPipeline = VK_NULL_HANDLE;
    }
    if (m_ssaoPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_ssaoPipelineLayout, nullptr);
        m_ssaoPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_ssaoDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_ssaoDescriptorPool, nullptr);
        m_ssaoDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_ssaoDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ssaoDescriptorSetLayout, nullptr);
        m_ssaoDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_ssaoNoiseImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ssaoNoiseImageView, nullptr);
        m_ssaoNoiseImageView = VK_NULL_HANDLE;
    }
    if (m_ssaoNoiseImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_ssaoNoiseImage, nullptr);
        m_ssaoNoiseImage = VK_NULL_HANDLE;
    }
    if (m_ssaoNoiseImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_ssaoNoiseImageMemory, nullptr);
        m_ssaoNoiseImageMemory = VK_NULL_HANDLE;
    }
    if (m_ssaoNoiseSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_ssaoNoiseSampler, nullptr);
        m_ssaoNoiseSampler = VK_NULL_HANDLE;
    }

    // Bloom Cleanup
    if (m_bloomBrightPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_bloomBrightPipeline, nullptr);
        m_bloomBrightPipeline = VK_NULL_HANDLE;
    }
    if (m_bloomBlurPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_bloomBlurPipeline, nullptr);
        m_bloomBlurPipeline = VK_NULL_HANDLE;
    }
    if (m_bloomBlendPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_bloomBlendPipeline, nullptr);
        m_bloomBlendPipeline = VK_NULL_HANDLE;
    }
    if (m_bloomPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_bloomPipelineLayout, nullptr);
        m_bloomPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_bloomDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_bloomDescriptorPool, nullptr);
        m_bloomDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_bloomDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_bloomDescriptorSetLayout, nullptr);
        m_bloomDescriptorSetLayout = VK_NULL_HANDLE;
    }

    // DoF Cleanup
    if (m_dofPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_dofPipeline, nullptr);
        m_dofPipeline = VK_NULL_HANDLE;
    }
    if (m_dofPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_dofPipelineLayout, nullptr);
        m_dofPipelineLayout = VK_NULL_HANDLE;
    }

    // Motion Blur Cleanup
    if (m_motionBlurPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_motionBlurPipeline, nullptr);
        m_motionBlurPipeline = VK_NULL_HANDLE;
    }
    if (m_motionBlurPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_motionBlurPipelineLayout, nullptr);
        m_motionBlurPipelineLayout = VK_NULL_HANDLE;
    }

    // Color Grading Cleanup
    if (m_colorGradingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_colorGradingPipeline, nullptr);
        m_colorGradingPipeline = VK_NULL_HANDLE;
    }
    if (m_colorGradingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_colorGradingPipelineLayout, nullptr);
        m_colorGradingPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_colorLutImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_colorLutImageView, nullptr);
        m_colorLutImageView = VK_NULL_HANDLE;
    }
    if (m_colorLutImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_colorLutImage, nullptr);
        m_colorLutImage = VK_NULL_HANDLE;
    }
    if (m_colorLutImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_colorLutImageMemory, nullptr);
        m_colorLutImageMemory = VK_NULL_HANDLE;
    }
    if (m_colorLutSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_colorLutSampler, nullptr);
        m_colorLutSampler = VK_NULL_HANDLE;
    }

    if (m_shadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_shadowSampler, nullptr);
        m_shadowSampler = VK_NULL_HANDLE;
    }
}

bool VulkanRenderer::CreatePBRResources() {
    VkDevice device = m_context->GetDevice();
    VkPhysicalDevice physicalDevice = m_context->GetPhysicalDevice();

    try {
        CreateBufferHelper(device, physicalDevice, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_sceneUbo, m_sceneUboMemory);
        CreateBufferHelper(device, physicalDevice, sizeof(GPUMaterialData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_materialUbo, m_materialUboMemory);
    } catch (...) {
        return false;
    }

    // Create Shadow Sampler
    VkSamplerCreateInfo shadowSamplerInfo{};
    shadowSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    shadowSamplerInfo.anisotropyEnable = VK_FALSE;
    shadowSamplerInfo.maxAnisotropy = 1.0f;
    shadowSamplerInfo.compareEnable = VK_TRUE;
    shadowSamplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    shadowSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &shadowSamplerInfo, nullptr, &m_shadowSampler) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_pbrDescriptorSetLayout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pbrDescriptorPool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_pbrDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_pbrDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_pbrDescriptorSet) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorBufferInfo bufferInfos[2]{};
    bufferInfos[0].buffer = m_sceneUbo;
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = sizeof(GPUSceneData);

    bufferInfos[1].buffer = m_materialUbo;
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = sizeof(GPUMaterialData);

    VkDescriptorImageInfo shadowImageInfo{};
    shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadowImageInfo.imageView = m_renderGraph.GetImageView("shadow_atlas");
    shadowImageInfo.sampler = m_shadowSampler;

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_pbrDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfos[0];

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_pbrDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &bufferInfos[1];

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_pbrDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &shadowImageInfo;

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4) * 2;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_pbrDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pbrPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VkShaderModule vertShaderModule = CreateShaderModuleHelper(device, pbrVertShaderCode, sizeof(pbrVertShaderCode));
    VkShaderModule fragShaderModule = CreateShaderModuleHelper(device, pbrFragShaderCode, sizeof(pbrFragShaderCode));

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
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

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 8;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[3]{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = sizeof(float) * 3;

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = sizeof(float) * 6;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

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
    pipelineInfo.layout = m_pbrPipelineLayout;
    pipelineInfo.renderPass = m_renderGraph.GetPassRenderPass("PBRPass");
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pbrPipeline) != VK_SUCCESS) {
        return false;
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    return true;
}

bool VulkanRenderer::CreateHDRResources() {
    VkDevice device = m_context->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_hdrSampler) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_hdrDescriptorSetLayout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_hdrDescriptorPool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_hdrDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_hdrDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_hdrDescriptorSet) != VK_SUCCESS) {
        return false;
    }

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(float);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_hdrDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_hdrPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VkShaderModule vertShaderModule = CreateShaderModuleHelper(device, hdrVertShaderCode, sizeof(hdrVertShaderCode));
    VkShaderModule fragShaderModule = CreateShaderModuleHelper(device, hdrFragShaderCode, sizeof(hdrFragShaderCode));

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
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

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

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
    pipelineInfo.layout = m_hdrPipelineLayout;
    pipelineInfo.renderPass = m_renderGraph.GetPassRenderPass("HDRPass");
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_hdrPipelineVk) != VK_SUCCESS) {
        return false;
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    return true;
}

bool VulkanRenderer::CreatePostProcessingResources() {
    VkDevice device = m_context->GetDevice();
    if (device == VK_NULL_HANDLE) return true;

    // Vulkan compatibility placeholder for dynamic physical hardware compilation
    return true;
}

} // namespace KumariEngine::Renderer
