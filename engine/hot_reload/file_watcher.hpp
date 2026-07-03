#pragma once
#include <string>
#include <functional>
#include <memory>
#include "reload_events.hpp"

namespace KumariEngine::HotReload {

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    // Start watching the specified directory path.
    // Callback is invoked on the background watcher thread when a file event occurs.
    bool Start(const std::string& directoryPath, std::function<void(const ReloadEvent&)> callback);
    
    // Stop watching and terminate the background thread.
    void Stop();
    
    // Check if currently watching.
    bool IsWatching() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace KumariEngine::HotReload
