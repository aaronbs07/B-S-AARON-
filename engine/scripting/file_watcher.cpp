#include "file_watcher.hpp"
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::Scripting {

void FileWatcher::AddDirectory(const std::string& directoryPath) {
    std::error_code ec;
    auto normDir = std::filesystem::absolute(directoryPath, ec);
    if (ec) {
        Core::Logger::Warning("FileWatcher", "Failed to resolve absolute path for: %s", directoryPath.c_str());
        return;
    }
    normDir = normDir.lexically_normal();
    std::string normDirStr = normDir.string();

    if (std::find(m_monitoredDirectories.begin(), m_monitoredDirectories.end(), normDirStr) != m_monitoredDirectories.end()) {
        return;
    }
    m_monitoredDirectories.push_back(normDirStr);
    Core::Logger::Info("FileWatcher", "Started watching directory: %s", normDirStr.c_str());

    // Initial scan to cache timestamps without triggering events
    try {
        if (std::filesystem::exists(normDir) && std::filesystem::is_directory(normDir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(normDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                    std::string filePath = std::filesystem::absolute(entry.path()).lexically_normal().string();
                    m_watchedFiles[filePath] = { std::filesystem::last_write_time(entry.path()), true };
                }
            }
        }
    } catch (const std::exception& e) {
        Core::Logger::Warning("FileWatcher", "Error doing initial scan in directory %s: %s", normDirStr.c_str(), e.what());
    }
}

void FileWatcher::RemoveDirectory(const std::string& directoryPath) {
    std::error_code ec;
    auto normDir = std::filesystem::absolute(directoryPath, ec);
    if (ec) return;
    normDir = normDir.lexically_normal();
    std::string normDirStr = normDir.string();

    auto it = std::find(m_monitoredDirectories.begin(), m_monitoredDirectories.end(), normDirStr);
    if (it != m_monitoredDirectories.end()) {
        m_monitoredDirectories.erase(it);
        Core::Logger::Info("FileWatcher", "Stopped watching directory: %s", normDirStr.c_str());

        // Remove cached files belonging to this directory
        for (auto fileIt = m_watchedFiles.begin(); fileIt != m_watchedFiles.end(); ) {
            if (fileIt->first.rfind(normDirStr, 0) == 0) {
                fileIt = m_watchedFiles.erase(fileIt);
            } else {
                ++fileIt;
            }
        }
    }
}

void FileWatcher::Update(const FileWatchCallback& callback) {
    // 1. Mark all watched files as not exists (temporary, to detect deletion)
    for (auto& [path, info] : m_watchedFiles) {
        info.exists = false;
    }

    // 2. Scan monitored directories
    for (const auto& dir : m_monitoredDirectories) {
        try {
            if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                    std::string filePath = std::filesystem::absolute(entry.path()).lexically_normal().string();
                    auto lastWrite = std::filesystem::last_write_time(entry.path());

                    auto it = m_watchedFiles.find(filePath);
                    if (it == m_watchedFiles.end()) {
                        // File Added
                        m_watchedFiles[filePath] = { lastWrite, true };
                        callback(filePath, FileEvent::Added);
                    } else {
                        it->second.exists = true;
                        if (lastWrite > it->second.lastWriteTime) {
                            // File Modified
                            it->second.lastWriteTime = lastWrite;
                            callback(filePath, FileEvent::Modified);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            Core::Logger::Warning("FileWatcher", "Error scanning directory %s: %s", dir.c_str(), e.what());
        }
    }

    // 3. Detect Deletions
    for (auto it = m_watchedFiles.begin(); it != m_watchedFiles.end(); ) {
        if (!it->second.exists) {
            // File Deleted
            std::string deletedPath = it->first;
            it = m_watchedFiles.erase(it);
            callback(deletedPath, FileEvent::Deleted);
        } else {
            ++it;
        }
    }
}

} // namespace KumariEngine::Scripting
