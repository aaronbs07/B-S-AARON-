#pragma once
#include "renderer/renderer.hpp"
#include "vulkan_context.hpp"
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

private:
    bool CreateRenderPass();
    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    void RecreateSwapChain();

    Window::Window* m_window = nullptr;
    std::unique_ptr<VulkanContext> m_context;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_frameStarted = false;

    std::unique_ptr<Terrain::TerrainRenderer> m_terrainRenderer;
};

} // namespace KumariEngine::Renderer
