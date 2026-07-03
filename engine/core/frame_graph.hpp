#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace KumariEngine::Core {

struct SystemTiming {
    std::string systemName;
    double durationMs = 0.0;
};

struct ThreadTiming {
    std::string threadName;
    double utilization = 0.0;
};

struct FrameGraphRecord {
    uint64_t frameIndex = 0;
    double totalFrameTimeMs = 0.0;
    std::vector<SystemTiming> systems;
    std::vector<ThreadTiming> threads;
    std::unordered_map<std::string, double> gpuStages;
};

class FrameGraph {
public:
    static FrameGraph& Get() {
        static FrameGraph instance;
        return instance;
    }

    FrameGraph(const FrameGraph&) = delete;
    FrameGraph& operator=(const FrameGraph&) = delete;

    void RecordFrame(uint64_t frameIdx, double frameTimeMs);
    bool ExportJSON(const std::string& filepath) const;

    const std::vector<FrameGraphRecord>& GetHistory() const { return m_history; }

private:
    FrameGraph();
    ~FrameGraph() = default;

    mutable std::mutex m_mutex;
    std::vector<FrameGraphRecord> m_history;
    size_t m_historyIndex = 0;
};

} // namespace KumariEngine::Core
