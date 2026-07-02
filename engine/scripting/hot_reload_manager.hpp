#pragma once
#include <string>
#include <unordered_set>
#include <memory>
#include "ecs/ecs.hpp"
#include "file_watcher.hpp"

namespace KumariEngine::Scripting {

class HotReloadManager {
public:
    static HotReloadManager& Get() {
        static HotReloadManager instance;
        return instance;
    }

    HotReloadManager(const HotReloadManager&) = delete;
    HotReloadManager& operator=(const HotReloadManager&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Scan for script file modifications and reload them
    void Update(float dt);

    // Register script path folder to be watched
    void WatchScript(const std::string& scriptPath);

private:
    HotReloadManager() = default;
    ~HotReloadManager() = default;

    ECS::Registry* m_registry = nullptr;
    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::unordered_set<std::string> m_watchedDirectories;
};

} // namespace KumariEngine::Scripting
