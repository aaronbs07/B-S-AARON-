#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Core {

class BenchmarkFramework {
public:
    static BenchmarkFramework& Get() {
        static BenchmarkFramework instance;
        return instance;
    }

    void StartBenchmark();
    void UpdateBenchmark(float dt);
    void EndBenchmark(const std::string& outputPath = "benchmark_report.json", const std::string& mdOutputPath = "docs/benchmark_report.md");

    void RecordStartupTime(double timeMs);
    void RecordLoadingTime(const std::string& name, double timeMs);

    bool IsBenchmarking() const { return m_active; }

private:
    BenchmarkFramework() = default;
    ~BenchmarkFramework() = default;

    bool m_active = false;
    double m_startupTimeMs = 0.0;
    std::unordered_map<std::string, std::vector<double>> m_loadingTimes;

    std::vector<double> m_frameTimesMs;
    
    std::chrono::high_resolution_clock::time_point m_benchmarkStartTime;
};

} // namespace KumariEngine::Core
