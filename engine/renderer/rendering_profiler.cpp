#include "rendering_profiler.hpp"
#include "core/logger.hpp"
#include <sstream>
#include <iomanip>

namespace KumariEngine::Renderer {

void RenderingProfiler::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    Core::Logger::Info("RenderingProfiler", "Initialized Rendering Profiler.");
}

void RenderingProfiler::Shutdown() {
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_passTimings.clear();
}

void RenderingProfiler::RecordPassTiming(const std::string& name, float cpuTimeMs, float gpuTimeMs) {
    for (auto& timing : m_passTimings) {
        if (timing.name == name) {
            timing.cpuTimeMs = cpuTimeMs;
            timing.gpuTimeMs = gpuTimeMs;
            return;
        }
    }
    m_passTimings.push_back({name, cpuTimeMs, gpuTimeMs});
}

std::string RenderingProfiler::GetFrameGraphVisualization() const {
    std::stringstream ss;
    ss << "=== RENDER GRAPH FRAME VISUALIZATION ===\n";
    ss << "  [ShadowPass]\n";
    ss << "       │ (shadow_atlas)\n";
    ss << "       ▼\n";
    ss << "  [HZBPass] ──► [SSAOGenPass] ──► [SSAOBlurPass]\n";
    ss << "       │             │\n";
    ss << "       ▼             ▼ (ssao_blurred)\n";
    ss << "  [PBRPass] ◄────────┘\n";
    ss << "       │ (hdr_target)\n";
    ss << "       ▼\n";
    ss << "  [SSRPass]\n";
    ss << "       │ (ssr_target)\n";
    ss << "       ├────────────────────────┐\n";
    ss << "       ▼                        ▼\n";
    ss << "  [MotionBlurPass]         [DOFPass]\n";
    ss << "       │ (velocity_buffer)      │ (dof_target)\n";
    ss << "       │                        ▼\n";
    ss << "       │                   [BloomBrightPass]\n";
    ss << "       │                        │\n";
    ss << "       │                        ▼\n";
    ss << "       │                   [BloomBlurPass]\n";
    ss << "       │                        │ (bloom_blurred)\n";
    ss << "       ▼                        ▼\n";
    ss << "  [ColorGradingPass] ◄──────────┘\n";
    ss << "       │ (color_graded)\n";
    ss << "       ▼\n";
    ss << "  [HDRPass] ──► [swapchain]\n";
    ss << "========================================";
    return ss.str();
}

std::vector<std::string> RenderingProfiler::GeneratePerformanceWarnings() const {
    std::vector<std::string> warnings;

    // Check individual pass timing limit (threshold: 10ms)
    for (const auto& timing : m_passTimings) {
        if (timing.gpuTimeMs > 10.0f) {
            std::stringstream ss;
            ss << "Warning: Pass '" << timing.name << "' has high GPU frame cost: " 
               << std::fixed << std::setprecision(2) << timing.gpuTimeMs << " ms (Threshold: 10.0 ms)";
            warnings.push_back(ss.str());
        }
    }

    // Check excessive draw calls (threshold: 1000)
    if (m_drawCallStats.drawCallCount > 1000) {
        std::stringstream ss;
        ss << "Warning: Excessive draw call count of " << m_drawCallStats.drawCallCount 
           << " detected. Consider optimization or batching adjustments.";
        warnings.push_back(ss.str());
    }

    // Check high shader pipeline switches (threshold: 100)
    if (m_drawCallStats.pipelineSwitches > 100) {
        std::stringstream ss;
        ss << "Warning: High pipeline state switches count: " << m_drawCallStats.pipelineSwitches 
           << ". Optimize shader bucket sorting.";
        warnings.push_back(ss.str());
    }

    // Check GPU Memory pressure (threshold: 2.5 GB / 2684354560 bytes)
    const size_t vramThreshold = 2684354560ULL; // 2.5 GB
    if (m_resourceUsage.vramAllocatedBytes > vramThreshold) {
        std::stringstream ss;
        ss << "Warning: VRAM allocated usage is high: " 
           << (m_resourceUsage.vramAllocatedBytes / (1024 * 1024)) << " MB. Potential risk of out of memory.";
        warnings.push_back(ss.str());
    }

    return warnings;
}

} // namespace KumariEngine::Renderer
