#include "profiler.hpp"
#include <volk.h>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <cassert>

namespace KumariEngine::Core {

// Thread-local stack to collect samples per thread before merging at root or frame end
thread_local std::vector<Profiler::ActiveSample> t_activeStack;

Profiler::Profiler() {
    m_frameHistory.resize(256);
}

void Profiler::BeginSample(const std::string& name) {
    ActiveSample active;
    active.sample.name = name;
    active.startTime = std::chrono::high_resolution_clock::now();
    t_activeStack.push_back(std::move(active));
}

void Profiler::EndSample() {
    if (t_activeStack.empty()) return;

    auto active = std::move(t_activeStack.back());
    t_activeStack.pop_back();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - active.startTime;
    active.sample.durationMs = elapsed.count();

    if (t_activeStack.empty()) {
        // Root sample completed on this thread, merge into main list under lock
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cpuSamples.push_back(std::move(active.sample));
    } else {
        // Append as child of current top of stack
        t_activeStack.back().sample.children.push_back(std::move(active.sample));
    }
}

void Profiler::ClearCPUHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cpuSamples.clear();
}

void Profiler::BeginFrame() {
    ClearCPUHistory();
}

void Profiler::EndFrame(double frameTimeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Save to ring buffer
    FrameRecord record;
    record.cpuTree = m_cpuSamples;
    record.frameTimeMs = frameTimeMs;

    m_frameHistory[m_historyIndex] = std::move(record);
    m_historyIndex = (m_historyIndex + 1) % 256;
}

void Profiler::SetThreadName(std::thread::id tid, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threadNames[tid] = name;
}

std::string Profiler::GetThreadName(std::thread::id tid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threadNames.find(tid);
    if (it != m_threadNames.end()) {
        return it->second;
    }
    return "WorkerThread";
}

// --- GPU Profiler ---

void Profiler::BeginGPUSample(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeGPUStack.push_back({name, std::chrono::high_resolution_clock::now()});
}

void Profiler::EndGPUSample() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeGPUStack.empty()) return;

    auto active = m_activeGPUStack.back();
    m_activeGPUStack.pop_back();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - active.startTime;
    m_gpuSamples[active.name] = elapsed.count();
}

struct GPUMap {
    std::string name;
    uint32_t startIdx = 0;
    uint32_t endIdx = 0;
    bool resolved = false;
};
static std::vector<GPUMap> s_activeMaps;
static uint32_t s_queryIndex = 0;

void Profiler::InitGPUTimestamps(void* device, void* physicalDevice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!device || !physicalDevice) {
        m_gpuTimestampsEnabled = false;
        return;
    }

    VkDevice vkDevice = static_cast<VkDevice>(device);
    VkPhysicalDevice vkPhysDevice = static_cast<VkPhysicalDevice>(physicalDevice);

    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = 128;

    VkQueryPool pool;
    if (vkCreateQueryPool(vkDevice, &createInfo, nullptr, &pool) == VK_SUCCESS) {
        m_queryPool = pool;
        m_gpuTimestampsEnabled = true;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(vkPhysDevice, &properties);
        m_timestampPeriod = properties.limits.timestampPeriod;
    } else {
        m_gpuTimestampsEnabled = false;
    }
}

void Profiler::WriteGPUTimestamp(void* cmdBuffer, const std::string& name, bool start) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_gpuTimestampsEnabled || !m_queryPool || !cmdBuffer) {
        // Fallback to CPU-based mock GPU timers
        if (start) {
            m_activeGPUStack.push_back({name, std::chrono::high_resolution_clock::now()});
        } else {
            if (!m_activeGPUStack.empty()) {
                auto active = m_activeGPUStack.back();
                m_activeGPUStack.pop_back();
                auto endTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = endTime - active.startTime;
                m_gpuSamples[active.name] = elapsed.count();
            }
        }
        return;
    }

    VkCommandBuffer vkCmd = static_cast<VkCommandBuffer>(cmdBuffer);
    VkQueryPool vkPool = static_cast<VkQueryPool>(m_queryPool);

    uint32_t currentIdx = s_queryIndex % 128;
    s_queryIndex++;

    vkCmdWriteTimestamp(vkCmd, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vkPool, currentIdx);

    if (start) {
        s_activeMaps.push_back({name, currentIdx, 0, false});
    } else {
        for (auto& m : s_activeMaps) {
            if (m.name == name && m.endIdx == 0 && !m.resolved) {
                m.endIdx = currentIdx;
                break;
            }
        }
    }
}

void Profiler::ResolveGPUTimestamps(void* device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_gpuTimestampsEnabled || !m_queryPool || !device) return;

    VkDevice vkDevice = static_cast<VkDevice>(device);
    VkQueryPool vkPool = static_cast<VkQueryPool>(m_queryPool);

    uint64_t queryResults[128];
    if (vkGetQueryPoolResults(vkDevice, vkPool, 0, 128, sizeof(queryResults), queryResults, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
        for (auto& m : s_activeMaps) {
            if (m.endIdx > 0 && !m.resolved) {
                uint64_t startVal = queryResults[m.startIdx];
                uint64_t endVal = queryResults[m.endIdx];
                if (endVal > startVal) {
                    double duration = static_cast<double>(endVal - startVal) * m_timestampPeriod * 1e-6; // convert nanoseconds to milliseconds
                    m_gpuSamples[m.name] = duration;
                }
                m.resolved = true;
            }
        }
        // Prune resolved maps
        s_activeMaps.erase(std::remove_if(s_activeMaps.begin(), s_activeMaps.end(), [](const GPUMap& m) {
            return m.resolved;
        }), s_activeMaps.end());
    }
}

// --- Memory Profiler ---

void Profiler::TrackAllocation(MemoryCategory category, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memoryUsage[category] += size;
}

void Profiler::TrackDeallocation(MemoryCategory category, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_memoryUsage.find(category);
    if (it != m_memoryUsage.end()) {
        if (it->second >= size) {
            it->second -= size;
        } else {
            it->second = 0;
        }
    }
}

size_t Profiler::GetMemoryUsage(MemoryCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_memoryUsage.find(category);
    if (it != m_memoryUsage.end()) {
        return it->second;
    }
    return 0;
}

size_t Profiler::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& [cat, size] : m_memoryUsage) {
        (void)cat;
        total += size;
    }
    return total;
}

static void ExportSampleRecursively(std::ofstream& out, const ProfilerSample& sample, int indent) {
    std::string spaces(indent * 2, ' ');
    out << spaces << "  [Scope] " << sample.name << ": " << sample.durationMs << " ms\n";
    for (const auto& child : sample.children) {
        ExportSampleRecursively(out, child, indent + 1);
    }
}

bool Profiler::CaptureFrame(const std::string& filename, ECS::Registry* registry) {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    out << "=== KUMARI KANDAM FRAME DIAGNOSTIC CAPTURE ===\n";
    
    // 1. Entities details
    if (registry) {
        auto entities = registry->GetAliveEntities();
        out << "Active ECS Entities: " << entities.size() << "\n";
        for (auto e : entities) {
            out << "  [Entity] ID: " << e << " | GUID: " << registry->GetGUID(e).high << "\n";
        }
    }

    // 2. CPU performance timers (hierarchical tree export)
    out << "\nCPU Performance Timers:\n";
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_cpuSamples) {
        ExportSampleRecursively(out, s, 0);
    }

    // 3. GPU performance timers
    out << "\nGPU Performance Timers:\n";
    for (const auto& [name, duration] : m_gpuSamples) {
        out << "  [Stage] " << name << ": " << duration << " ms\n";
    }

    // 4. Memory Profiler details
    out << "\nMemory Profiler Allocations:\n";
    const char* categoryNames[] = { 
        "ECS", "Renderer", "Audio", "Physics", "Script", "Network", "General",
        "Terrain", "Streaming", "HotReload", "Reflection"
    };
    for (int i = 0; i <= static_cast<int>(MemoryCategory::Reflection); ++i) {
        size_t usage = m_memoryUsage[static_cast<MemoryCategory>(i)];
        out << "  [Category] " << categoryNames[i] << ": " << (static_cast<double>(usage) / 1024.0) << " KB\n";
    }
    size_t totalMem = 0;
    for (const auto& [cat, size] : m_memoryUsage) {
        (void)cat;
        totalMem += size;
    }
    out << "  Total Memory Tracked: " << (static_cast<double>(totalMem) / 1024.0) << " KB\n";
    
    out << "=== CAPTURE END ===\n";
    return true;
}

} // namespace KumariEngine::Core
