#include "core/engine.hpp"
#include "scene/transform_component.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "core/benchmark_framework.hpp"
#include "core/memory_tracker.hpp"
#include "core/thread_profiler.hpp"
#include "core/frame_graph.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

using namespace KumariEngine;

int main() {
    std::cout << "=== RUNNING ENGINE PERFORMANCE BENCHMARK SUITE ===" << std::endl;

    // 1. Initialize systems
    ECS::Registry registry;
    registry.RegisterComponent<Scene::TransformComponent>();

    auto& benchmark = Core::BenchmarkFramework::Get();
    auto& memory = Core::MemoryTracker::Get();
    auto& threadProf = Core::ThreadProfiler::Get();

    // Register test threads
    threadProf.RegisterThread(std::this_thread::get_id(), "MainThread");

    // Track mock memory allocations
    memory.TrackAllocation(Core::MemoryCategory::ECS, 1024 * 1024 * 5); // 5 MB
    memory.TrackAllocation(Core::MemoryCategory::Renderer, 1024 * 1024 * 20); // 20 MB
    memory.TrackAllocation(Core::MemoryCategory::Terrain, 1024 * 1024 * 10); // 10 MB

    // Start benchmark
    benchmark.StartBenchmark();
    benchmark.RecordStartupTime(45.2);
    benchmark.RecordLoadingTime("mesh_car", 12.4);
    benchmark.RecordLoadingTime("texture_grass", 3.8);

    // Simulate 150 frames of benchmark loop
    for (int frame = 0; frame < 150; ++frame) {
        threadProf.BeginWork(std::this_thread::get_id(), "FrameLoop");

        Core::Profiler::Get().BeginFrame();
        {
            PROFILE_SCOPE("PhysicsSystem");
            std::this_thread::sleep_for(std::chrono::microseconds(500)); // small delay
        }
        {
            PROFILE_SCOPE("RenderSystem");
            std::this_thread::sleep_for(std::chrono::microseconds(1000)); // small delay
        }
        Core::Profiler::Get().EndFrame(16.6);

        threadProf.EndWork(std::this_thread::get_id());

        // Update benchmark frame time
        benchmark.UpdateBenchmark(0.0166f); // 16.6 ms dt
    }

    // After 200 frames, BenchmarkFramework::UpdateBenchmark exits or ends benchmark.
    // In our manual test execution, we force EndBenchmark to ensure it completes under custom output names.
    const std::string jsonReportPath = "test_benchmark_report.json";
    const std::string mdReportPath = "docs/test_benchmark_report.md";

    benchmark.EndBenchmark(jsonReportPath, mdReportPath);

    // Validate that outputs exist and are populated
    assert(std::filesystem::exists(jsonReportPath));
    assert(std::filesystem::exists(mdReportPath));

    std::cout << "Benchmark reports generated successfully." << std::endl;

    // Verify report contents
    std::ifstream jsonIn(jsonReportPath);
    std::string line;
    bool foundAverage = false;
    while (std::getline(jsonIn, line)) {
        if (line.find("average_frametime_ms") != std::string::npos) {
            foundAverage = true;
            break;
        }
    }
    assert(foundAverage);

    // Cleanup output files
    std::remove(jsonReportPath.c_str());
    std::remove(mdReportPath.c_str());

    std::cout << "=== PERFORMANCE BENCHMARK SUITE PASSED! ===" << std::endl;
    return 0;
}
