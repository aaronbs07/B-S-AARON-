#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "reload_events.hpp"

namespace KumariEngine::HotReload {

class ReloadQueue {
public:
    ReloadQueue() = default;
    ~ReloadQueue() = default;

    void Push(const ReloadEvent& event);
    bool Pop(ReloadEvent& outEvent);
    size_t Size() const;
    void Clear();
    bool IsReloadPending(const std::string& path) const;

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_pendingPaths;
    std::unordered_map<std::string, ReloadEvent> m_pendingEvents;
};

} // namespace KumariEngine::HotReload
