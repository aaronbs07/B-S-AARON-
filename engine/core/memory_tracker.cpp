#include "memory_tracker.hpp"
#include <algorithm>

namespace KumariEngine::Core {

void MemoryTracker::TrackAllocation(MemoryCategory category, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& stats = m_stats[category];
    stats.currentUsage += size;
    stats.allocationCount++;

    if (stats.currentUsage > stats.peakUsage) {
        stats.peakUsage = stats.currentUsage;
    }

    m_totalAllocated += size;
    if (m_totalAllocated > m_totalPeak) {
        m_totalPeak = m_totalAllocated;
    }

    // Estimate fragmentation
    if (stats.peakUsage > 0) {
        size_t activeCount = (stats.allocationCount > stats.deallocationCount) ? (stats.allocationCount - stats.deallocationCount) : 0;
        // Assume an average block size of 128 bytes
        stats.fragmentationRatio = static_cast<float>(activeCount * 128) / stats.peakUsage;
        stats.fragmentationRatio = std::min(stats.fragmentationRatio, 1.0f);
    }

    // Propagate to global Profiler allocations tracking for backward compatibility
    Profiler::Get().TrackAllocation(category, size);
}

void MemoryTracker::TrackDeallocation(MemoryCategory category, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& stats = m_stats[category];
    if (stats.currentUsage >= size) {
        stats.currentUsage -= size;
    } else {
        stats.currentUsage = 0;
    }
    stats.deallocationCount++;

    if (m_totalAllocated >= size) {
        m_totalAllocated -= size;
    } else {
        m_totalAllocated = 0;
    }

    // Estimate fragmentation
    if (stats.peakUsage > 0) {
        size_t activeCount = (stats.allocationCount > stats.deallocationCount) ? (stats.allocationCount - stats.deallocationCount) : 0;
        stats.fragmentationRatio = static_cast<float>(activeCount * 128) / stats.peakUsage;
        stats.fragmentationRatio = std::min(stats.fragmentationRatio, 1.0f);
    }

    // Propagate to global Profiler deallocations tracking for backward compatibility
    Profiler::Get().TrackDeallocation(category, size);
}

MemoryStats MemoryTracker::GetStats(MemoryCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stats.find(category);
    if (it != m_stats.end()) {
        return it->second;
    }
    return MemoryStats{};
}

size_t MemoryTracker::GetTotalAllocated() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalAllocated;
}

size_t MemoryTracker::GetTotalPeak() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalPeak;
}

} // namespace KumariEngine::Core
