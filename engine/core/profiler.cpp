#include "profiler.hpp"
#include "scene/scene_node.hpp"
#include "scene/scene_manager.hpp"
#include <fstream>
#include <numeric>

namespace KumariEngine::Core {

// --- CPU Profiler ---

void Profiler::BeginSample(const std::string& name) {
    m_activeCPUStack.push_back({name, std::chrono::high_resolution_clock::now()});
}

void Profiler::EndSample() {
    if (m_activeCPUStack.empty()) return;

    auto active = m_activeCPUStack.back();
    m_activeCPUStack.pop_back();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - active.startTime;
    double duration = elapsed.count();

    ProfilerSample sample{active.name, duration, {}};

    if (m_activeCPUStack.empty()) {
        m_cpuSamples.push_back(sample);
    } else {
        // Find or create child entries (simplified flat hierarchy for tracing)
        m_cpuSamples.push_back(sample);
    }
}


// --- GPU Profiler ---

void Profiler::BeginGPUSample(const std::string& name) {
    m_activeGPUStack.push_back({name, std::chrono::high_resolution_clock::now()});
}

void Profiler::EndGPUSample() {
    if (m_activeGPUStack.empty()) return;

    auto active = m_activeGPUStack.back();
    m_activeGPUStack.pop_back();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - active.startTime;
    m_gpuSamples[active.name] = elapsed.count();
}


// --- Memory Profiler ---

void Profiler::TrackAllocation(MemoryCategory category, size_t size) {
    m_memoryUsage[category] += size;
}

void Profiler::TrackDeallocation(MemoryCategory category, size_t size) {
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
    auto it = m_memoryUsage.find(category);
    if (it != m_memoryUsage.end()) {
        return it->second;
    }
    return 0;
}

size_t Profiler::GetTotalMemoryUsage() const {
    size_t total = 0;
    for (const auto& [cat, size] : m_memoryUsage) {
        (void)cat;
        total += size;
    }
    return total;
}


// --- Frame Capture ---

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

    // 2. CPU performance timers
    out << "\nCPU Performance Timers:\n";
    for (const auto& s : m_cpuSamples) {
        out << "  [Scope] " << s.name << ": " << s.durationMs << " ms\n";
    }

    // 3. GPU performance timers
    out << "\nGPU Performance Timers:\n";
    for (const auto& [name, duration] : m_gpuSamples) {
        out << "  [Stage] " << name << ": " << duration << " ms\n";
    }

    // 4. Memory Profiler details
    out << "\nMemory Profiler Allocations:\n";
    const char* categoryNames[] = { "ECS", "Renderer", "Audio", "Physics", "Script", "Network", "General" };
    for (int i = 0; i <= static_cast<int>(MemoryCategory::General); ++i) {
        size_t usage = GetMemoryUsage(static_cast<MemoryCategory>(i));
        out << "  [Category] " << categoryNames[i] << ": " << (static_cast<double>(usage) / 1024.0) << " KB\n";
    }
    out << "  Total Memory Tracked: " << (static_cast<double>(GetTotalMemoryUsage()) / 1024.0) << " KB\n";
    
    out << "=== CAPTURE END ===\n";
    return true;
}

} // namespace KumariEngine::Core
