#include "frame_graph.hpp"
#include "profiler.hpp"
#include "thread_profiler.hpp"
#include <fstream>
#include <iomanip>

namespace KumariEngine::Core {

FrameGraph::FrameGraph() {
    m_history.resize(256);
}

void FrameGraph::RecordFrame(uint64_t frameIdx, double frameTimeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FrameGraphRecord record;
    record.frameIndex = frameIdx;
    record.totalFrameTimeMs = frameTimeMs;

    // Pull CPU system metrics from Profiler
    auto cpuSamples = Profiler::Get().GetCPUHistory();
    for (const auto& sample : cpuSamples) {
        record.systems.push_back({sample.name, sample.durationMs});
        for (const auto& child : sample.children) {
            record.systems.push_back({sample.name + "/" + child.name, child.durationMs});
        }
    }

    // Pull thread states from ThreadProfiler
    auto threadStates = ThreadProfiler::Get().GetThreadStates();
    for (const auto& [tid, state] : threadStates) {
        (void)tid;
        record.threads.push_back({state.threadName, state.lastUtilizationRatio});
    }

    // Pull GPU timings
    record.gpuStages = Profiler::Get().GetGPUSamples();

    m_history[m_historyIndex] = std::move(record);
    m_historyIndex = (m_historyIndex + 1) % 256;
}

bool FrameGraph::ExportJSON(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "{\n  \"frames\": [\n";
    bool firstFrame = true;

    for (size_t i = 0; i < 256; ++i) {
        size_t idx = (m_historyIndex + i) % 256;
        const auto& rec = m_history[idx];
        if (rec.totalFrameTimeMs <= 0.0) continue; // skip unrecorded frames

        if (!firstFrame) out << ",\n";
        firstFrame = false;

        out << "    {\n";
        out << "      \"frame_index\": " << rec.frameIndex << ",\n";
        out << "      \"frame_time_ms\": " << rec.totalFrameTimeMs << ",\n";
        
        // Systems
        out << "      \"cpu_systems\": {\n";
        for (size_t s = 0; s < rec.systems.size(); ++s) {
            out << "        \"" << rec.systems[s].systemName << "\": " << rec.systems[s].durationMs;
            if (s + 1 < rec.systems.size()) out << ",";
            out << "\n";
        }
        out << "      },\n";

        // Threads
        out << "      \"threads\": {\n";
        for (size_t t = 0; t < rec.threads.size(); ++t) {
            out << "        \"" << rec.threads[t].threadName << "\": " << std::fixed << std::setprecision(4) << rec.threads[t].utilization;
            if (t + 1 < rec.threads.size()) out << ",";
            out << "\n";
        }
        out << "      },\n";

        // GPU stages
        out << "      \"gpu_stages\": {\n";
        size_t g = 0;
        for (const auto& [name, time] : rec.gpuStages) {
            out << "        \"" << name << "\": " << time;
            if (g + 1 < rec.gpuStages.size()) out << ",";
            out << "\n";
            g++;
        }
        out << "      }\n";

        out << "    }";
    }

    out << "\n  ]\n}\n";
    return true;
}

} // namespace KumariEngine::Core
