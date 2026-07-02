#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include "ecs/ecs.hpp"

namespace KumariEngine::Core {

struct ProfilerSample {
    std::string name;
    double durationMs = 0.0;
    std::vector<ProfilerSample> children;
};

enum class MemoryCategory {
    ECS,
    Renderer,
    Audio,
    Physics,
    Script,
    Network,
    General
};

class Profiler {
public:
    static Profiler& Get() {
        static Profiler instance;
        return instance;
    }
    
    // CPU Profiler
    void BeginSample(const std::string& name);
    void EndSample();
    const std::vector<ProfilerSample>& GetCPUHistory() const { return m_cpuSamples; }
    void ClearCPUHistory() { m_cpuSamples.clear(); }
    
    // GPU Profiler
    void BeginGPUSample(const std::string& name);
    void EndGPUSample();
    const std::unordered_map<std::string, double>& GetGPUSamples() const { return m_gpuSamples; }
    
    // Memory Profiler
    void TrackAllocation(MemoryCategory category, size_t size);
    void TrackDeallocation(MemoryCategory category, size_t size);
    size_t GetMemoryUsage(MemoryCategory category) const;
    size_t GetTotalMemoryUsage() const;
    
    // Frame Capture Diagnostic Export
    bool CaptureFrame(const std::string& filename, ECS::Registry* registry);

private:
    Profiler() = default;
    ~Profiler() = default;
    
    // CPU state
    struct ActiveSampleInfo {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    std::vector<ActiveSampleInfo> m_activeCPUStack;
    std::vector<ProfilerSample> m_cpuSamples;
    
    // GPU state
    struct ActiveGPUSampleInfo {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    std::vector<ActiveGPUSampleInfo> m_activeGPUStack;
    std::unordered_map<std::string, double> m_gpuSamples;
    
    // Memory state
    std::unordered_map<MemoryCategory, size_t> m_memoryUsage;
};

// RAII Profiler Scope Helper
class CPUProfileScope {
public:
    CPUProfileScope(const std::string& name) {
        Profiler::Get().BeginSample(name);
    }
    ~CPUProfileScope() {
        Profiler::Get().EndSample();
    }
};

#define PROFILE_SCOPE(name) KumariEngine::Core::CPUProfileScope profileScope##__LINE__(name)

} // namespace KumariEngine::Core
