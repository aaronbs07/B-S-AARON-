#include "thread_profiler.hpp"
#include "profiler.hpp"

namespace KumariEngine::Core {

void ThreadProfiler::RegisterThread(std::thread::id tid, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& info = m_threads[tid];
    info.threadName = name;
    info.tid = tid;
    info.isWorking = false;
    info.accumulatedWorkTimeMs = 0.0;
    info.accumulatedIdleTimeMs = 0.0;
    info.lastUtilizationRatio = 0.0;

    // Propagate to main Profiler naming for compatibility
    Profiler::Get().SetThreadName(tid, name);
}

void ThreadProfiler::BeginWork(std::thread::id tid, const std::string& taskName) {
    (void)taskName;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(tid);
    if (it != m_threads.end() && !it->second.isWorking) {
        it->second.isWorking = true;
        it->second.workStartTime = std::chrono::high_resolution_clock::now();
    }
}

void ThreadProfiler::EndWork(std::thread::id tid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(tid);
    if (it != m_threads.end() && it->second.isWorking) {
        it->second.isWorking = false;
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = endTime - it->second.workStartTime;
        it->second.accumulatedWorkTimeMs += elapsed.count();
    }
}

void ThreadProfiler::UpdateUtilization(float elapsedSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    double elapsedMs = static_cast<double>(elapsedSec * 1000.0f);
    if (elapsedMs <= 0.0) return;

    auto now = std::chrono::high_resolution_clock::now();

    for (auto& [tid, info] : m_threads) {
        if (info.isWorking) {
            // Add currently ongoing work time
            std::chrono::duration<double, std::milli> elapsed = now - info.workStartTime;
            info.accumulatedWorkTimeMs += elapsed.count();
            info.workStartTime = now; // reset start time to now for next interval
        }

        double totalMs = info.accumulatedWorkTimeMs + info.accumulatedIdleTimeMs;
        if (totalMs > 0.0) {
            info.lastUtilizationRatio = info.accumulatedWorkTimeMs / totalMs;
        } else {
            info.lastUtilizationRatio = info.isWorking ? 1.0 : 0.0;
        }

        // Clamp ratio
        if (info.lastUtilizationRatio > 1.0) info.lastUtilizationRatio = 1.0;
        if (info.lastUtilizationRatio < 0.0) info.lastUtilizationRatio = 0.0;

        // Reset accumulators
        info.accumulatedWorkTimeMs = 0.0;
        info.accumulatedIdleTimeMs = elapsedMs * (1.0 - info.lastUtilizationRatio);
    }
}

double ThreadProfiler::GetUtilizationRatio(std::thread::id tid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(tid);
    if (it != m_threads.end()) {
        return it->second.lastUtilizationRatio;
    }
    return 0.0;
}

std::unordered_map<std::thread::id, ThreadStateInfo> ThreadProfiler::GetThreadStates() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_threads;
}

} // namespace KumariEngine::Core
