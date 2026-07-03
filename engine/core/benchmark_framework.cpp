#include "benchmark_framework.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"
#include <fstream>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Core {

void BenchmarkFramework::StartBenchmark() {
    Logger::Info("Benchmark", "Performance benchmark started. Capturing 200 frames...");
    m_active = true;
    m_frameTimesMs.clear();
    m_loadingTimes.clear();
    m_benchmarkStartTime = std::chrono::high_resolution_clock::now();
}

void BenchmarkFramework::UpdateBenchmark(float dt) {
    if (!m_active) return;

    m_frameTimesMs.push_back(static_cast<double>(dt * 1000.0));

    // End benchmark automatically after 200 frames
    if (m_frameTimesMs.size() >= 200) {
        EndBenchmark();
        // Terminate process cleanly in benchmark mode
        Logger::Info("Benchmark", "Benchmark completed. Exiting program.");
        std::exit(0);
    }
}

void BenchmarkFramework::RecordStartupTime(double timeMs) {
    m_startupTimeMs = timeMs;
    Logger::Info("Benchmark", "Recorded startup time: %.3f ms", timeMs);
}

void BenchmarkFramework::RecordLoadingTime(const std::string& name, double timeMs) {
    m_loadingTimes[name].push_back(timeMs);
    Logger::Info("Benchmark", "Recorded loading time for '%s': %.3f ms", name.c_str(), timeMs);
}

void BenchmarkFramework::EndBenchmark(const std::string& outputPath, const std::string& mdOutputPath) {
    if (!m_active) return;
    m_active = false;

    if (m_frameTimesMs.empty()) return;

    double totalFrametime = std::accumulate(m_frameTimesMs.begin(), m_frameTimesMs.end(), 0.0);
    double avgFrametime = totalFrametime / m_frameTimesMs.size();
    double minFrametime = *std::min_element(m_frameTimesMs.begin(), m_frameTimesMs.end());
    double maxFrametime = *std::max_element(m_frameTimesMs.begin(), m_frameTimesMs.end());
    double avgFps = 1000.0 / avgFrametime;

    // Build directories if they have parent paths
    std::filesystem::path outParent = std::filesystem::path(outputPath).parent_path();
    if (!outParent.empty()) {
        std::filesystem::create_directories(outParent);
    }
    std::filesystem::path mdParent = std::filesystem::path(mdOutputPath).parent_path();
    if (!mdParent.empty()) {
        std::filesystem::create_directories(mdParent);
    }

    // JSON export
    std::ofstream jsonFile(outputPath);
    if (jsonFile.is_open()) {
        jsonFile << "{\n";
        jsonFile << "  \"startup_time_ms\": " << m_startupTimeMs << ",\n";
        jsonFile << "  \"average_frametime_ms\": " << avgFrametime << ",\n";
        jsonFile << "  \"min_frametime_ms\": " << minFrametime << ",\n";
        jsonFile << "  \"max_frametime_ms\": " << maxFrametime << ",\n";
        jsonFile << "  \"average_fps\": " << avgFps << ",\n";
        
        jsonFile << "  \"memory_usage\": {\n";
        jsonFile << "    \"ECS\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::ECS) << ",\n";
        jsonFile << "    \"Renderer\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Renderer) << ",\n";
        jsonFile << "    \"Audio\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Audio) << ",\n";
        jsonFile << "    \"Physics\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Physics) << ",\n";
        jsonFile << "    \"Script\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Script) << ",\n";
        jsonFile << "    \"Network\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Network) << ",\n";
        jsonFile << "    \"General\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::General) << ",\n";
        jsonFile << "    \"Terrain\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Terrain) << ",\n";
        jsonFile << "    \"Streaming\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Streaming) << ",\n";
        jsonFile << "    \"HotReload\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::HotReload) << ",\n";
        jsonFile << "    \"Reflection\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Reflection) << "\n";
        jsonFile << "  },\n";

        jsonFile << "  \"loading_latencies\": {\n";
        size_t idx = 0;
        for (const auto& pair : m_loadingTimes) {
            double totalLoad = std::accumulate(pair.second.begin(), pair.second.end(), 0.0);
            double avgLoad = totalLoad / pair.second.size();
            jsonFile << "    \"" << pair.first << "\": " << avgLoad;
            jsonFile << (idx == m_loadingTimes.size() - 1 ? "\n" : ",\n");
            idx++;
        }
        jsonFile << "  }\n";
        jsonFile << "}\n";
        jsonFile.close();
    }

    // Markdown export
    std::ofstream mdFile(mdOutputPath);
    if (mdFile.is_open()) {
        mdFile << "# Performance Benchmark Report\n\n";
        mdFile << "## Engine Performance Statistics\n\n";
        mdFile << "| Metric | Value |\n";
        mdFile << "| --- | --- |\n";
        mdFile << "| **Startup Time** | " << m_startupTimeMs << " ms |\n";
        mdFile << "| **Average Frame Time** | " << avgFrametime << " ms |\n";
        mdFile << "| **Min Frame Time** | " << minFrametime << " ms |\n";
        mdFile << "| **Max Frame Time** | " << maxFrametime << " ms |\n";
        mdFile << "| **Average Framerate** | " << avgFps << " FPS |\n\n";

        mdFile << "## Memory Allocation Breakdown\n\n";
        mdFile << "| Category | Usage (Bytes) |\n";
        mdFile << "| --- | --- |\n";
        mdFile << "| **ECS** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::ECS) << " |\n";
        mdFile << "| **Renderer** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Renderer) << " |\n";
        mdFile << "| **Audio** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Audio) << " |\n";
        mdFile << "| **Physics** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Physics) << " |\n";
        mdFile << "| **Script** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Script) << " |\n";
        mdFile << "| **Network** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Network) << " |\n";
        mdFile << "| **General** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::General) << " |\n";
        mdFile << "| **Terrain** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Terrain) << " |\n";
        mdFile << "| **Streaming** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Streaming) << " |\n";
        mdFile << "| **HotReload** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::HotReload) << " |\n";
        mdFile << "| **Reflection** | " << Profiler::Get().GetMemoryUsage(MemoryCategory::Reflection) << " |\n";
        mdFile << "| **Total** | " << Profiler::Get().GetTotalMemoryUsage() << " |\n\n";

        if (!m_loadingTimes.empty()) {
            mdFile << "## Asset Loading Latencies\n\n";
            mdFile << "| Asset | Average Load Time (ms) |\n";
            mdFile << "| --- | --- |\n";
            for (const auto& pair : m_loadingTimes) {
                double totalLoad = std::accumulate(pair.second.begin(), pair.second.end(), 0.0);
                double avgLoad = totalLoad / pair.second.size();
                mdFile << "| " << pair.first << " | " << avgLoad << " ms |\n";
            }
        }
        mdFile.close();
    }

    Logger::Info("Benchmark", "Benchmark report files exported successfully to %s and %s.", outputPath.c_str(), mdOutputPath.c_str());
}

} // namespace KumariEngine::Core
