#pragma once
#include <string>
#include <memory>
#include <shared_mutex>
#include "file_watcher.hpp"
#include "reload_queue.hpp"
#include "reflection/type_registry.hpp"

namespace KumariEngine::Renderer { class VulkanRenderer; }

namespace KumariEngine::HotReload {

class HotReloadManager {
public:
    static HotReloadManager& Get() {
        static HotReloadManager instance;
        return instance;
    }

    HotReloadManager(const HotReloadManager&) = delete;
    HotReloadManager& operator=(const HotReloadManager&) = delete;

    void Initialize(const std::string& assetsPath, Renderer::VulkanRenderer* renderer);
    void Shutdown();
    void StopWatching();
    void Update(float deltaTime);

    void QueueReload(const std::string& path);
    bool IsReloadPending(const std::string& path) const;

    // Telemetry & Stats Accessors
    int GetSuccessCount() const { return m_successCount; }
    int GetFailureCount() const { return m_failureCount; }
    int GetQueuedCount() const;
    std::string GetLastReloadTime() const { return m_lastReloadTime; }
    bool IsWatching() const;
    std::string GetWatchedDirectory() const { return m_assetsPath; }

    // Manual/Mock modifiers for testing
    void IncrementSuccess() { m_successCount++; UpdateLastReloadTime(); }
    void IncrementFailure() { m_failureCount++; }
    ReloadQueue* GetQueue() const { return m_reloadQueue.get(); }

    // Reflection type-reload hook.
    // Call when a script/asset reload may invalidate reflected type data.
    void NotifyTypeReloaded(uint64_t typeId) {
        Reflection::TypeRegistry::Get().OnTypeReloaded(typeId);
    }

private:
    HotReloadManager() = default;
    ~HotReloadManager() = default;

    void ProcessReloadEvent(const ReloadEvent& event);
    void UpdateLastReloadTime();

    std::string m_assetsPath;
    Renderer::VulkanRenderer* m_renderer = nullptr;
    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::unique_ptr<ReloadQueue> m_reloadQueue;

    // Telemetry stats
    int m_successCount = 0;
    int m_failureCount = 0;
    std::string m_lastReloadTime = "Never";
    
    bool m_initialized = false;
};

} // namespace KumariEngine::HotReload
