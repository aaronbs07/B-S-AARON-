#pragma once
#include "renderer/renderer.hpp"
#include "vulkan_context.hpp"
#include "renderer/render_graph.hpp"
#include "renderer/hdr_pipeline.hpp"
#include "renderer/post_process_pipeline.hpp"
#include "renderer/light_manager.hpp"
#include "shadow/shadow_system.hpp"
#include "renderer/optimization_manager.hpp"
#include "renderer/ibl_manager.hpp"
#include "renderer/ssr_manager.hpp"
#include "renderer/volumetric_lighting_manager.hpp"
#include "weather/atmospheric_renderer.hpp"
#include "renderer/rendering_profiler.hpp"
#include <volk.h>
#include <memory>
#include <vector>

namespace KumariEngine::Terrain { class TerrainRenderer; }

namespace KumariEngine::Renderer {

class VulkanRenderer : public Renderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    bool Initialize(Window::Window* window) override;
    void Shutdown() override;

    void BeginFrame() override;
    void DrawFrame() override;
    void EndFrame() override;

    VkDevice GetDevice() const { return m_context ? m_context->GetDevice() : VK_NULL_HANDLE; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_context ? m_context->GetPhysicalDevice() : VK_NULL_HANDLE; }
    VkCommandPool GetCommandPool() const { return m_commandPool; }
    VkQueue GetGraphicsQueue() const { return m_context ? m_context->GetGraphicsQueue() : VK_NULL_HANDLE; }

    // Modern Renderer Foundation API Accessors
    RenderGraph& GetRenderGraph() { return m_renderGraph; }
    HDRPipeline& GetHDRPipeline() { return m_hdrPipeline; }
    LightManager& GetLightManager() { return m_lightManager; }
    ShadowSystem& GetShadowSystem() { return m_shadowSystem; }
    PostProcessingPipeline& GetPostProcessingPipeline() { return m_postProcessPipeline; }
    OptimizationManager& GetOptimizationManager() { return m_optimizationManager; }

    IBLManager& GetIBLManager() { return IBLManager::Get(); }
    SSRManager& GetSSRManager() { return SSRManager::Get(); }
    VolumetricLightingManager& GetVolumetricLightingManager() { return VolumetricLightingManager::Get(); }
    Environment::AtmosphericRenderer& GetAtmosphericRenderer() { return Environment::AtmosphericRenderer::Get(); }
    RenderingProfiler& GetRenderingProfiler() { return RenderingProfiler::Get(); }

    VkPipeline GetPBRPipeline() const { return m_pbrPipeline; }
    VkPipelineLayout GetPBRPipelineLayout() const { return m_pbrPipelineLayout; }

private:
    bool CreateRenderPass();
    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    // Modern rendering setup helpers
    bool InitializeModernRendering();
    void CleanupModernRendering();
    bool CreatePBRResources();
    bool CreateHDRResources();
    bool CreatePostProcessingResources(); // Helper to compile other pipelines

    void RecreateSwapChain();

    Window::Window* m_window = nullptr;
    std::unique_ptr<VulkanContext> m_context;

    // Legacy render pass (maintained for compatibility)
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    // Modern Render Graph, HDR, and Lighting subsystems
    RenderGraph m_renderGraph;
    HDRPipeline m_hdrPipeline;
    LightManager m_lightManager;
    ShadowSystem m_shadowSystem;
    PostProcessingPipeline m_postProcessPipeline;

    // PBR Pass resources
    VkPipeline m_pbrPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pbrPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_pbrDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pbrDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_pbrDescriptorSet = VK_NULL_HANDLE;
    VkBuffer m_sceneUbo = VK_NULL_HANDLE;
    VkDeviceMemory m_sceneUboMemory = VK_NULL_HANDLE;
    VkBuffer m_materialUbo = VK_NULL_HANDLE;
    VkDeviceMemory m_materialUboMemory = VK_NULL_HANDLE;

    // HDR / Tone mapping Post-process resources
    VkPipeline m_hdrPipelineVk = VK_NULL_HANDLE;
    VkPipelineLayout m_hdrPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_hdrDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_hdrDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_hdrDescriptorSet = VK_NULL_HANDLE;
    VkSampler m_hdrSampler = VK_NULL_HANDLE;

    // SSAO Vulkan resources
    VkPipeline m_ssaoPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_ssaoPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ssaoDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_ssaoDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_ssaoDescriptorSet = VK_NULL_HANDLE;
    VkImage m_ssaoNoiseImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ssaoNoiseImageMemory = VK_NULL_HANDLE;
    VkImageView m_ssaoNoiseImageView = VK_NULL_HANDLE;
    VkSampler m_ssaoNoiseSampler = VK_NULL_HANDLE;

    // Bloom Vulkan resources
    VkPipeline m_bloomBrightPipeline = VK_NULL_HANDLE;
    VkPipeline m_bloomBlurPipeline = VK_NULL_HANDLE;
    VkPipeline m_bloomBlendPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_bloomPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bloomDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_bloomDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_bloomDescriptorSet = VK_NULL_HANDLE;

    // DoF Vulkan resources
    VkPipeline m_dofPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_dofPipelineLayout = VK_NULL_HANDLE;

    // Motion Blur Vulkan resources
    VkPipeline m_motionBlurPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_motionBlurPipelineLayout = VK_NULL_HANDLE;

    // Color Grading Vulkan resources
    VkPipeline m_colorGradingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_colorGradingPipelineLayout = VK_NULL_HANDLE;
    VkImage m_colorLutImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorLutImageMemory = VK_NULL_HANDLE;
    VkImageView m_colorLutImageView = VK_NULL_HANDLE;
    VkSampler m_colorLutSampler = VK_NULL_HANDLE;

    // Shadow Map resources
    VkSampler m_shadowSampler = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_frameStarted = false;

    OptimizationManager m_optimizationManager;
    std::unique_ptr<Terrain::TerrainRenderer> m_terrainRenderer;
};

} // namespace KumariEngine::Renderer
