#include "render_pass.hpp"

namespace KumariEngine::Renderer {

RenderPass::RenderPass(const std::string& name)
    : m_name(name) {}

void RenderPass::AddInput(const std::string& name, AttachmentType type) {
    PassAttachment att;
    att.name = name;
    att.type = type;
    att.isInput = true;
    att.isOutput = false;
    m_attachments.push_back(att);
}

void RenderPass::AddOutput(const std::string& name, AttachmentType type, VkFormat format, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkClearValue clearValue) {
    PassAttachment att;
    att.name = name;
    att.type = type;
    att.format = format;
    att.loadOp = loadOp;
    att.storeOp = storeOp;
    att.clearValue = clearValue;
    att.isInput = false;
    att.isOutput = true;
    m_attachments.push_back(att);
}

void RenderPass::Execute(VkCommandBuffer cmd, const RenderGraph& graph) const {
    if (m_executeCallback) {
        m_executeCallback(cmd, graph);
    }
}

} // namespace KumariEngine::Renderer
