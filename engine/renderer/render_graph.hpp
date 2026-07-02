#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <volk.h>
#include "render_pass.hpp"

namespace KumariEngine::Renderer {

struct GraphImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{0, 0};
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool isExternal = false;
};

class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    void RegisterPass(std::unique_ptr<RenderPass> pass);
    void RegisterExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent, VkImageLayout initialLayout);
    void RegisterPhysicalImage(const std::string& name, VkFormat format, VkExtent2D extent);

    bool Compile(VkDevice device, VkPhysicalDevice physicalDevice);
    void PruneUnusedPasses();
    void Execute(VkCommandBuffer cmd, VkDevice device);
    void Shutdown(VkDevice device);

    VkImage GetImage(const std::string& name) const;
    VkImageView GetImageView(const std::string& name) const;
    VkImageLayout GetImageLayout(const std::string& name) const;
    VkExtent2D GetImageExtent(const std::string& name) const;
    VkRenderPass GetPassRenderPass(const std::string& name) const;

    const std::vector<std::unique_ptr<RenderPass>>& GetSortedPasses() const { return m_sortedPasses; }

    static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

private:
    bool CreatePhysicalImages(VkDevice device, VkPhysicalDevice physicalDevice);
    bool CreateVkRenderPasses(VkDevice device);
    bool CreateFramebuffers(VkDevice device);

    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::vector<std::unique_ptr<RenderPass>> m_sortedPasses;
    std::unordered_map<std::string, GraphImage> m_images;

    // Vulkan runtime caches per sorted pass
    struct PassResources {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };
    std::vector<PassResources> m_passResources;
};

} // namespace KumariEngine::Renderer
