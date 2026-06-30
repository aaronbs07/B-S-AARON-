#pragma once
#include <volk.h>
#include <glm/glm.hpp>
#include <vector>
#include <mutex>
#include "physics_types.hpp"

namespace KumariEngine::Physics {

struct DebugVertex {
    glm::vec3 pos;
    glm::vec3 color;
};

class PhysicsDebugRenderer {
public:
    static PhysicsDebugRenderer& Get() {
        static PhysicsDebugRenderer instance;
        return instance;
    }

    PhysicsDebugRenderer(const PhysicsDebugRenderer&) = delete;
    PhysicsDebugRenderer& operator=(const PhysicsDebugRenderer&) = delete;

    bool Initialize(VkDevice device, VkRenderPass renderPass, VkPhysicalDevice physicalDevice);
    void Shutdown(VkDevice device);

    // Submission functions (thread-safe)
    void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color = glm::vec3(0.0f, 1.0f, 0.0f));
    void DrawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color = glm::vec3(0.0f, 1.0f, 0.0f));
    void DrawSphere(const glm::vec3& center, float radius, const glm::vec3& color = glm::vec3(0.0f, 1.0f, 0.0f));
    void DrawCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color = glm::vec3(0.0f, 1.0f, 0.0f));
    void DrawContactPoint(const glm::vec3& pos, const glm::vec3& normal, float penetration, const glm::vec3& color = glm::vec3(1.0f, 0.0f, 0.0f));

    // Clear submitted lines
    void Clear();

    // Render pass call
    void Draw(VkCommandBuffer commandBuffer, const glm::mat4& viewProj);

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enable) { m_enabled = enable; }

private:
    PhysicsDebugRenderer() = default;
    ~PhysicsDebugRenderer() = default;

    bool CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, 
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    std::vector<DebugVertex> m_submittedVertices;
    std::mutex m_mutex;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    uint32_t m_maxVertices = 131072; // ~2.6 MB buffer size
    void* m_mappedData = nullptr;
    bool m_enabled = false;
    bool m_initialized = false;
};

} // namespace KumariEngine::Physics
