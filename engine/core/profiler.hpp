#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include "ecs/ecs.hpp"

namespace KumariEngine::Core {

struct ProfilerSample {
    std::string name;
    double durationMs = 0.0;
    std::vector<ProfilerSample> children;
};

struct FrameRecord {
    std::vector<ProfilerSample> cpuTree;
    double frameTimeMs = 0.0;
};

enum class MemoryCategory {
    ECS,
    Renderer,
    Audio,
    Physics,
    Script,
    Network,
    General,
    Terrain,
    Streaming,
    HotReload,
    Reflection
};

class Profiler {
public:
    struct ActiveSample {
        ProfilerSample sample;
        std::chrono::high_resolution_clock::time_point startTime;
    };

    static Profiler& Get() {
        static Profiler instance;
        return instance;
    }

    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    // CPU Profiler
    void BeginSample(const std::string& name);
    void EndSample();
    const std::vector<ProfilerSample>& GetCPUHistory() const { return m_cpuSamples; }
    void ClearCPUHistory();

    // Frame Boundary (updates ring buffer)
    void BeginFrame();
    void EndFrame(double frameTimeMs);
    const std::vector<FrameRecord>& GetFrameHistory() const { return m_frameHistory; }

    // Thread labeling
    void SetThreadName(std::thread::id tid, const std::string& name);
    std::string GetThreadName(std::thread::id tid) const;

    // GPU Profiler
    void BeginGPUSample(const std::string& name);
    void EndGPUSample();
    const std::unordered_map<std::string, double>& GetGPUSamples() const { return m_gpuSamples; }

    void InitGPUTimestamps(void* device, void* physicalDevice);
    void WriteGPUTimestamp(void* cmdBuffer, const std::string& name, bool start);
    void ResolveGPUTimestamps(void* device);

    // Memory Profiler
    void TrackAllocation(MemoryCategory category, size_t size);
    void TrackDeallocation(MemoryCategory category, size_t size);
    size_t GetMemoryUsage(MemoryCategory category) const;
    size_t GetTotalMemoryUsage() const;

    // Frame Capture Diagnostic Export
    bool CaptureFrame(const std::string& filename, ECS::Registry* registry);

private:
    Profiler();
    ~Profiler() = default;



    mutable std::mutex m_mutex;
    std::vector<ProfilerSample> m_cpuSamples;

    // Last 256 frames ring buffer
    std::vector<FrameRecord> m_frameHistory;
    size_t m_historyIndex = 0;

    // Thread names map
    std::unordered_map<std::thread::id, std::string> m_threadNames;

    // GPU state
    struct ActiveGPUSampleInfo {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    std::vector<ActiveGPUSampleInfo> m_activeGPUStack;
    std::unordered_map<std::string, double> m_gpuSamples;
    void* m_queryPool = nullptr;
    float m_timestampPeriod = 1.0f;
    bool m_gpuTimestampsEnabled = false;

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
