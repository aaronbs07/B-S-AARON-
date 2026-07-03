#pragma once
#include "profiler.hpp"
#include <unordered_map>
#include <mutex>

namespace KumariEngine::Core {

struct MemoryStats {
    size_t currentUsage = 0;
    size_t peakUsage = 0;
    size_t allocationCount = 0;
    size_t deallocationCount = 0;
    float fragmentationRatio = 0.0f;
};

class MemoryTracker {
public:
    static MemoryTracker& Get() {
        static MemoryTracker instance;
        return instance;
    }

    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;

    void TrackAllocation(MemoryCategory category, size_t size);
    void TrackDeallocation(MemoryCategory category, size_t size);

    MemoryStats GetStats(MemoryCategory category) const;
    size_t GetTotalAllocated() const;
    size_t GetTotalPeak() const;

private:
    MemoryTracker() = default;
    ~MemoryTracker() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<MemoryCategory, MemoryStats> m_stats;
    size_t m_totalAllocated = 0;
    size_t m_totalPeak = 0;
};

} // namespace KumariEngine::Core
