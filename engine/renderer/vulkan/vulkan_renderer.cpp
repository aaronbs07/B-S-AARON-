#include "vulkan_renderer.hpp"
#include "core/logger.hpp"
#include "window/window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "terrain/terrain_renderer.hpp"
#include "terrain/terrain_manager.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "physics/debug_renderer.hpp"

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

    m_context->RecreateSwapChain(m_window);
    CreateFramebuffers();
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

    m_frameStarted = true;
}

void VulkanRenderer::DrawFrame() {
    if (!m_frameStarted) return;

    VkCommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];
    VkExtent2D extent = m_context->GetSwapChainExtent();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[m_imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    // Dark steel-blue colored background
    VkClearValue clearColor = {{{0.11f, 0.13f, 0.19f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    // Apply dynamic viewport and scissor states
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Issue draw call (3 vertices for 1 triangle)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // Draw Terrain chunks
    auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
    glm::mat4 viewProj = glm::mat4(1.0f);
    const Camera::Camera* cameraPtr = nullptr;
    if (activeCam) {
        viewProj = activeCam->GetProjectionMatrix() * activeCam->GetViewMatrix();
        cameraPtr = activeCam.get();
    }
    if (m_terrainRenderer) {
        m_terrainRenderer->Draw(commandBuffer, viewProj, cameraPtr);
    }

    Physics::PhysicsDebugRenderer::Get().Draw(commandBuffer, viewProj);

    vkCmdEndRenderPass(commandBuffer);
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

} // namespace KumariEngine::Renderer
