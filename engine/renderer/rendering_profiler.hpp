#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <volk.h>

namespace KumariEngine::Renderer {

struct RenderPassTiming {
    std::string name;
    float cpuTimeMs = 0.0f;
    float gpuTimeMs = 0.0f;
};

struct DrawCallStats {
    uint32_t drawCallCount = 0;
    uint32_t instanceCount = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t pipelineSwitches = 0;
    uint32_t descriptorBinds = 0;
};

struct RenderingResourceUsage {
    size_t vramAllocatedBytes = 0;
    uint32_t textureCount = 0;
    uint32_t bufferCount = 0;
    uint32_t pipelineCount = 0;
    uint32_t descriptorSetCount = 0;
};

class RenderingProfiler {
public:
    static RenderingProfiler& Get() {
        static RenderingProfiler instance;
        return instance;
    }

    RenderingProfiler(const RenderingProfiler&) = delete;
    RenderingProfiler& operator=(const RenderingProfiler&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    // Stats updates
    void RecordPassTiming(const std::string& name, float cpuTimeMs, float gpuTimeMs);
    void SetDrawCallStats(const DrawCallStats& stats) { m_drawCallStats = stats; }
    void SetResourceUsage(const RenderingResourceUsage& usage) { m_resourceUsage = usage; }

    const std::vector<RenderPassTiming>& GetPassTimings() const { return m_passTimings; }
    const DrawCallStats& GetDrawCallStats() const { return m_drawCallStats; }
    const RenderingResourceUsage& GetResourceUsage() const { return m_resourceUsage; }

    // Visualizations
    std::string GetFrameGraphVisualization() const;

    // Diagnostics & Warnings
    std::vector<std::string> GeneratePerformanceWarnings() const;

private:
    RenderingProfiler() = default;
    ~RenderingProfiler() = default;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    std::vector<RenderPassTiming> m_passTimings;
    DrawCallStats m_drawCallStats;
    RenderingResourceUsage m_resourceUsage;
};

} // namespace KumariEngine::Renderer
