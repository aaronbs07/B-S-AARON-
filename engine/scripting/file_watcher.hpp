#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>

namespace KumariEngine::Scripting {

enum class FileEvent {
    Modified,
    Added,
    Deleted
};

using FileWatchCallback = std::function<void(const std::string& filePath, FileEvent event)>;

class FileWatcher {
public:
    FileWatcher() = default;
    ~FileWatcher() = default;

    void AddDirectory(const std::string& directoryPath);
    void RemoveDirectory(const std::string& directoryPath);

    // Scan directories and trigger callbacks for changes
    void Update(const FileWatchCallback& callback);

private:
    struct FileInfo {
        std::filesystem::file_time_type lastWriteTime;
        bool exists = true;
    };

    std::vector<std::string> m_monitoredDirectories;
    std::unordered_map<std::string, FileInfo> m_watchedFiles;
};

} // namespace KumariEngine::Scripting
