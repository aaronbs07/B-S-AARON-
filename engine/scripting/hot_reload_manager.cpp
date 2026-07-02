#include "hot_reload_manager.hpp"
#include "file_watcher.hpp"
#include "script_engine.hpp"
#include "core/logger.hpp"
#include <filesystem>

namespace KumariEngine::Scripting {

void HotReloadManager::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    m_fileWatcher = std::make_unique<FileWatcher>();
    m_watchedDirectories.clear();
    Core::Logger::Info("HotReload", "Hot Reload Manager initialized.");
}

void HotReloadManager::Shutdown() {
    m_fileWatcher.reset();
    m_registry = nullptr;
    m_watchedDirectories.clear();
    Core::Logger::Info("HotReload", "Hot Reload Manager shut down.");
}

void HotReloadManager::Update(float dt) {
    (void)dt;
    if (!m_fileWatcher) return;

    m_fileWatcher->Update([](const std::string& filePath, FileEvent event) {
        if (event == FileEvent::Modified) {
            Core::Logger::Info("HotReload", "File modification detected: %s. Reloading...", filePath.c_str());
            ScriptEngine::Get().ReloadScript(filePath);
        } else if (event == FileEvent::Added) {
            Core::Logger::Info("HotReload", "File addition detected: %s.", filePath.c_str());
        } else if (event == FileEvent::Deleted) {
            Core::Logger::Info("HotReload", "File deletion detected: %s.", filePath.c_str());
        }
    });
}

void HotReloadManager::WatchScript(const std::string& scriptPath) {
    if (!m_fileWatcher) return;

    std::error_code ec;
    std::filesystem::path p(scriptPath);
    std::filesystem::path dir = p.parent_path();
    if (dir.empty()) {
        dir = ".";
    }

    auto absDir = std::filesystem::absolute(dir, ec);
    if (ec) {
        Core::Logger::Warning("HotReload", "Failed to resolve directory for script %s", scriptPath.c_str());
        return;
    }
    std::string dirStr = absDir.lexically_normal().string();

    if (m_watchedDirectories.find(dirStr) == m_watchedDirectories.end()) {
        m_watchedDirectories.insert(dirStr);
        m_fileWatcher->AddDirectory(dirStr);
    }
}

} // namespace KumariEngine::Scripting
