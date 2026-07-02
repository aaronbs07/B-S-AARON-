#include "render_graph.hpp"
#include "core/logger.hpp"
#include <queue>
#include <algorithm>
#include <stdexcept>

namespace KumariEngine::Renderer {

uint32_t FindMemoryTypeHelper(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

RenderGraph::RenderGraph() = default;

RenderGraph::~RenderGraph() = default;

void RenderGraph::RegisterPass(std::unique_ptr<RenderPass> pass) {
    m_passes.push_back(std::move(pass));
}

void RenderGraph::RegisterExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent, VkImageLayout initialLayout) {
    GraphImage img{};
    img.image = image;
    img.view = view;
    img.format = format;
    img.extent = extent;
    img.currentLayout = initialLayout;
    img.isExternal = true;
    m_images[name] = img;
}

void RenderGraph::RegisterPhysicalImage(const std::string& name, VkFormat format, VkExtent2D extent) {
    GraphImage img{};
    img.format = format;
    img.extent = extent;
    img.isExternal = false;
    m_images[name] = img;
}

void RenderGraph::PruneUnusedPasses() {
    if (m_passes.empty()) return;

    std::vector<bool> needed(m_passes.size(), false);
    bool changed = true;

    // Phase 1: Mark passes that write directly to presentation/external final targets (e.g. "swapchain") as needed
    for (size_t i = 0; i < m_passes.size(); ++i) {
        for (const auto& att : m_passes[i]->GetAttachments()) {
            if (att.isOutput && att.name == "swapchain") {
                needed[i] = true;
                break;
            }
        }
    }

    // Phase 2: Propagate dependency backwards: if pass A is needed, and it reads a resource written by pass B, then pass B is needed
    while (changed) {
        changed = false;
        for (size_t i = 0; i < m_passes.size(); ++i) {
            if (needed[i]) continue;

            bool writesToNeeded = false;
            for (const auto& attThis : m_passes[i]->GetAttachments()) {
                if (!attThis.isOutput) continue;

                for (size_t j = 0; j < m_passes.size(); ++j) {
                    if (!needed[j]) continue;

                    for (const auto& attOther : m_passes[j]->GetAttachments()) {
                        if (attOther.isInput && attOther.name == attThis.name) {
                            writesToNeeded = true;
                            break;
                        }
                    }
                    if (writesToNeeded) break;
                }
                if (writesToNeeded) break;
            }

            if (writesToNeeded) {
                needed[i] = true;
                changed = true;
            }
        }
    }

    // Prune the unneeded passes
    std::vector<std::unique_ptr<RenderPass>> keptPasses;
    for (size_t i = 0; i < m_passes.size(); ++i) {
        if (needed[i]) {
            keptPasses.push_back(std::move(m_passes[i]));
        } else {
            Core::Logger::Info("RenderGraph", "Pruned unused render pass: %s", m_passes[i]->GetName().c_str());
        }
    }
    m_passes = std::move(keptPasses);
}

bool RenderGraph::Compile(VkDevice device, VkPhysicalDevice physicalDevice) {
    PruneUnusedPasses();
    Core::Logger::Info("RenderGraph", "Compiling Render Graph with %d passes...", m_passes.size());

    // 1. Topological Sort of Render Passes
    size_t passCount = m_passes.size();
    std::vector<int> inDegree(passCount, 0);
    std::vector<std::vector<int>> adj(passCount);

    for (size_t i = 0; i < passCount; ++i) {
        for (size_t j = 0; j < passCount; ++j) {
            if (i == j) continue;
            // Check if pass j depends on pass i
            bool depends = false;
            for (const auto& attJ : m_passes[j]->GetAttachments()) {
                if (attJ.isInput) {
                    for (const auto& attI : m_passes[i]->GetAttachments()) {
                        if (attI.isOutput && attI.name == attJ.name) {
                            depends = true;
                            break;
                        }
                    }
                }
                if (depends) break;
            }
            if (depends) {
                adj[i].push_back(static_cast<int>(j));
                inDegree[j]++;
            }
        }
    }

    std::queue<int> q;
    for (size_t i = 0; i < passCount; ++i) {
        if (inDegree[i] == 0) {
            q.push(static_cast<int>(i));
        }
    }

    m_sortedPasses.clear();

    // Since we want to keep passes intact, let's clone the passes or just sort them in-place
    // Fallback: If sorting cycle is detected or to make it simpler, sort them based on dependencies
    // Let's implement a simple topological sort index list:
    std::vector<int> sortedIndices;
    std::queue<int> qSort;
    for (size_t i = 0; i < passCount; ++i) {
        if (inDegree[i] == 0) {
            qSort.push(static_cast<int>(i));
        }
    }
    while (!qSort.empty()) {
        int curr = qSort.front();
        qSort.pop();
        sortedIndices.push_back(curr);
        for (int neighbor : adj[curr]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                qSort.push(neighbor);
            }
        }
    }

    // Fallback if sorting failed (e.g. cycle) to insertion order
    if (sortedIndices.size() < passCount) {
        Core::Logger::Warning("RenderGraph", "Cyclic dependency detected in Render Graph! Falling back to insertion order.");
        sortedIndices.clear();
        for (size_t i = 0; i < passCount; ++i) {
            sortedIndices.push_back(static_cast<int>(i));
        }
    }

    m_sortedPasses.clear();
    m_sortedPasses.reserve(passCount);
    for (int idx : sortedIndices) {
        // Transfer ownership of passes to sortedPasses
        m_sortedPasses.push_back(std::move(m_passes[idx]));
    }
    m_passes.clear();

    if (device == VK_NULL_HANDLE) {
        Core::Logger::Info("RenderGraph", "Render Graph compiled in Headless/Mock mode successfully.");
        return true;
    }

    // 2. Allocate Physical Images
    if (!CreatePhysicalImages(device, physicalDevice)) {
        return false;
    }

    // 3. Create Vulkan Render Passes & Framebuffers
    if (!CreateVkRenderPasses(device)) {
        return false;
    }

    if (!CreateFramebuffers(device)) {
        return false;
    }

    Core::Logger::Info("RenderGraph", "Render Graph compiled successfully.");
    return true;
}

bool RenderGraph::CreatePhysicalImages(VkDevice device, VkPhysicalDevice physicalDevice) {
    for (auto& [name, img] : m_images) {
        if (img.isExternal) continue;

        Core::Logger::Info("RenderGraph", "Allocating physical image for target: %s", name.c_str());

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = img.extent.width;
        imageInfo.extent.height = img.extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = img.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Assign standard usages
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (img.format == VK_FORMAT_D32_SFLOAT || img.format == VK_FORMAT_D24_UNORM_S8_UINT) {
            usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        imageInfo.usage = usage;

        if (vkCreateImage(device, &imageInfo, nullptr, &img.image) != VK_SUCCESS) {
            Core::Logger::Error("RenderGraph", "Failed to create image: %s", name.c_str());
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, img.image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryTypeHelper(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &img.memory) != VK_SUCCESS) {
            Core::Logger::Error("RenderGraph", "Failed to allocate memory for image: %s", name.c_str());
            return false;
        }

        vkBindImageMemory(device, img.image, img.memory, 0);

        // Create View
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = img.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = img.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &img.view) != VK_SUCCESS) {
            Core::Logger::Error("RenderGraph", "Failed to create image view for: %s", name.c_str());
            return false;
        }

        img.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    return true;
}

bool RenderGraph::CreateVkRenderPasses(VkDevice device) {
    m_passResources.resize(m_sortedPasses.size());

    for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
        const auto& pass = m_sortedPasses[i];
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;
        VkAttachmentReference depthRef{};
        bool hasDepth = false;

        uint32_t attachIndex = 0;
        for (const auto& att : pass->GetAttachments()) {
            if (!att.isOutput) continue; // Only output attachments are bound to target framebuffers

            VkAttachmentDescription desc{};
            desc.format = att.format;
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = att.loadOp;
            desc.storeOp = att.storeOp;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            
            if (att.type == AttachmentType::Depth) {
                desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                attachments.push_back(desc);

                depthRef.attachment = attachIndex;
                depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                hasDepth = true;
            } else {
                desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (att.name == "swapchain") {
                    desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
                attachments.push_back(desc);

                VkAttachmentReference ref{};
                ref.attachment = attachIndex;
                ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorRefs.push_back(ref);
            }
            attachIndex++;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();
        if (hasDepth) {
            subpass.pDepthStencilAttachment = &depthRef;
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &createInfo, nullptr, &m_passResources[i].renderPass) != VK_SUCCESS) {
            Core::Logger::Error("RenderGraph", "Failed to create VkRenderPass for: %s", pass->GetName().c_str());
            return false;
        }
    }
    return true;
}

bool RenderGraph::CreateFramebuffers(VkDevice device) {
    for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
        const auto& pass = m_sortedPasses[i];
        std::vector<VkImageView> attachments;
        VkExtent2D extent{0, 0};

        for (const auto& att : pass->GetAttachments()) {
            if (!att.isOutput) continue;
            auto it = m_images.find(att.name);
            if (it != m_images.end()) {
                attachments.push_back(it->second.view);
                extent = it->second.extent;
            }
        }

        if (attachments.empty()) {
            // Virtual pass without physical attachments
            continue;
        }

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_passResources[i].renderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = extent.width;
        createInfo.height = extent.height;
        createInfo.layers = 1;

        if (vkCreateFramebuffer(device, &createInfo, nullptr, &m_passResources[i].framebuffer) != VK_SUCCESS) {
            Core::Logger::Error("RenderGraph", "Failed to create VkFramebuffer for: %s", pass->GetName().c_str());
            return false;
        }
    }
    return true;
}

void RenderGraph::Execute(VkCommandBuffer cmd, VkDevice device) {
    (void)device;

    for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
        const auto& pass = m_sortedPasses[i];
        const auto& resources = m_passResources[i];

        // 1. Transition layouts for inputs and outputs before executing pass
        for (const auto& att : pass->GetAttachments()) {
            auto it = m_images.find(att.name);
            if (it == m_images.end()) continue;

            VkImageLayout targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            if (att.type == AttachmentType::Depth) {
                targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            } else if (att.isInput) {
                targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            if (it->second.currentLayout != targetLayout) {
                TransitionImageLayout(cmd, it->second.image, it->second.format, it->second.currentLayout, targetLayout);
                it->second.currentLayout = targetLayout;
            }
        }

        // 2. Begin VkRenderPass if attachments are present
        if (resources.framebuffer != VK_NULL_HANDLE) {
            VkRenderPassBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = resources.renderPass;
            beginInfo.framebuffer = resources.framebuffer;

            // Resolve extent from output image
            VkExtent2D extent{800, 600};
            std::vector<VkClearValue> clearValues;
            for (const auto& att : pass->GetAttachments()) {
                if (att.isOutput) {
                    clearValues.push_back(att.clearValue);
                    auto it = m_images.find(att.name);
                    if (it != m_images.end()) {
                        extent = it->second.extent;
                    }
                }
            }

            beginInfo.renderArea.offset = {0, 0};
            beginInfo.renderArea.extent = extent;
            beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            beginInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        // 3. Execute pass callback
        pass->Execute(cmd, *this);

        // 4. End VkRenderPass if attachments are present
        if (resources.framebuffer != VK_NULL_HANDLE) {
            vkCmdEndRenderPass(cmd);
        }
    }
}

void RenderGraph::Shutdown(VkDevice device) {
    for (auto& res : m_passResources) {
        if (res.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, res.framebuffer, nullptr);
            res.framebuffer = VK_NULL_HANDLE;
        }
        if (res.renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, res.renderPass, nullptr);
            res.renderPass = VK_NULL_HANDLE;
        }
    }
    m_passResources.clear();

    for (auto& [name, img] : m_images) {
        if (img.isExternal) continue;
        if (img.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, img.view, nullptr);
            img.view = VK_NULL_HANDLE;
        }
        if (img.image != VK_NULL_HANDLE) {
            vkDestroyImage(device, img.image, nullptr);
            img.image = VK_NULL_HANDLE;
        }
        if (img.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, img.memory, nullptr);
            img.memory = VK_NULL_HANDLE;
        }
    }
    m_images.clear();
    m_sortedPasses.clear();
    m_passes.clear();
}

VkImage RenderGraph::GetImage(const std::string& name) const {
    auto it = m_images.find(name);
    return it != m_images.end() ? it->second.image : VK_NULL_HANDLE;
}

VkImageView RenderGraph::GetImageView(const std::string& name) const {
    auto it = m_images.find(name);
    return it != m_images.end() ? it->second.view : VK_NULL_HANDLE;
}

VkImageLayout RenderGraph::GetImageLayout(const std::string& name) const {
    auto it = m_images.find(name);
    return it != m_images.end() ? it->second.currentLayout : VK_IMAGE_LAYOUT_UNDEFINED;
}

VkExtent2D RenderGraph::GetImageExtent(const std::string& name) const {
    auto it = m_images.find(name);
    return it != m_images.end() ? it->second.extent : VkExtent2D{0, 0};
}

void RenderGraph::TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || 
        format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(
        cmd,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

VkRenderPass RenderGraph::GetPassRenderPass(const std::string& name) const {
    for (size_t i = 0; i < m_sortedPasses.size(); ++i) {
        if (m_sortedPasses[i]->GetName() == name) {
            return m_passResources[i].renderPass;
        }
    }
    return VK_NULL_HANDLE;
}

} // namespace KumariEngine::Renderer
