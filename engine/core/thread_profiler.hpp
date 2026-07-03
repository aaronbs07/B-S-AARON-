#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>

namespace KumariEngine::Core {

struct ThreadStateInfo {
    std::string threadName;
    std::thread::id tid;
    bool isWorking = false;
    std::chrono::high_resolution_clock::time_point workStartTime;
    double accumulatedWorkTimeMs = 0.0;
    double accumulatedIdleTimeMs = 0.0;
    double lastUtilizationRatio = 0.0;
};

class ThreadProfiler {
public:
    static ThreadProfiler& Get() {
        static ThreadProfiler instance;
        return instance;
    }

    ThreadProfiler(const ThreadProfiler&) = delete;
    ThreadProfiler& operator=(const ThreadProfiler&) = delete;

    void RegisterThread(std::thread::id tid, const std::string& name);
    void BeginWork(std::thread::id tid, const std::string& taskName);
    void EndWork(std::thread::id tid);

    // Updates utilization ratios for all registered threads and resets accumulators
    void UpdateUtilization(float elapsedSec);

    double GetUtilizationRatio(std::thread::id tid) const;
    std::unordered_map<std::thread::id, ThreadStateInfo> GetThreadStates() const;

private:
    ThreadProfiler() = default;
    ~ThreadProfiler() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::thread::id, ThreadStateInfo> m_threads;
};

} // namespace KumariEngine::Core
