#include "reload_queue.hpp"
#include <algorithm>

namespace KumariEngine::HotReload {

void ReloadQueue::Push(const ReloadEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pendingEvents.find(event.path);
    if (it != m_pendingEvents.end()) {
        // If it's already pending, update the event details (e.g. latest timestamp and type)
        // This is the core of duplicate prevention during change bursts
        it->second = event;
    } else {
        m_pendingEvents[event.path] = event;
        m_pendingPaths.push_back(event.path);
    }
}

bool ReloadQueue::Pop(ReloadEvent& outEvent) {
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_pendingPaths.empty()) {
        std::string path = m_pendingPaths.front();
        m_pendingPaths.erase(m_pendingPaths.begin());

        auto it = m_pendingEvents.find(path);
        if (it != m_pendingEvents.end()) {
            outEvent = it->second;
            m_pendingEvents.erase(it);
            return true;
        }
    }
    return false;
}

size_t ReloadQueue::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingEvents.size();
}

void ReloadQueue::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingPaths.clear();
    m_pendingEvents.clear();
}

bool ReloadQueue::IsReloadPending(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingEvents.find(path) != m_pendingEvents.end();
}

} // namespace KumariEngine::HotReload
