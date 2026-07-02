#pragma once
#include <string>
#include <vector>
#include <functional>
#include <volk.h>

namespace KumariEngine::Renderer {

class RenderGraph;

enum class AttachmentType {
    Color,
    Depth
};

struct PassAttachment {
    std::string name;
    AttachmentType type = AttachmentType::Color;
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    bool isInput = false;
    bool isOutput = false;
};

class RenderPass {
public:
    RenderPass(const std::string& name);
    ~RenderPass() = default;

    void AddInput(const std::string& name, AttachmentType type = AttachmentType::Color);
    void AddOutput(const std::string& name, AttachmentType type = AttachmentType::Color, VkFormat format = VK_FORMAT_B8G8R8A8_SRGB, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE, VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}});

    const std::string& GetName() const { return m_name; }
    const std::vector<PassAttachment>& GetAttachments() const { return m_attachments; }

    void SetExecuteCallback(std::function<void(VkCommandBuffer, const RenderGraph&)> callback) { m_executeCallback = callback; }
    void Execute(VkCommandBuffer cmd, const RenderGraph& graph) const;

private:
    std::string m_name;
    std::vector<PassAttachment> m_attachments;
    std::function<void(VkCommandBuffer, const RenderGraph&)> m_executeCallback;
};

} // namespace KumariEngine::Renderer
