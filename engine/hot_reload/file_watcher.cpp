#include "file_watcher.hpp"
#include "core/logger.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace KumariEngine::HotReload {

#ifdef _WIN32
struct FileWatcher::Impl {
    std::string watchDir;
    std::function<void(const ReloadEvent&)> callback;
    HANDLE hDir = INVALID_HANDLE_VALUE;
    std::thread watchThread;
    std::atomic<bool> isWatching{false};

    bool IsTemporaryFile(const std::string& path) {
        if (path.empty()) return true;

        size_t filenamePos = path.find_last_of('/');
        std::string filename = (filenamePos == std::string::npos) ? path : path.substr(filenamePos + 1);

        if (filename.empty()) return true;
        if (filename[0] == '.') return true;
        if (filename.find('~') != std::string::npos) return true;

        std::string ext;
        size_t extPos = filename.find_last_of('.');
        if (extPos != std::string::npos) {
            ext = filename.substr(extPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        if (ext == ".tmp" || ext == ".swp" || ext == ".lock" || ext == ".lk" || ext == ".pdb" || ext == ".ilk") {
            return true;
        }
        return false;
    }

    void ThreadLoop() {
        alignas(DWORD) uint8_t buffer[65536];
        DWORD bytesReturned = 0;
        std::string lastOldName = "";

        while (isWatching) {
            BOOL success = ReadDirectoryChangesW(
                hDir,
                buffer,
                sizeof(buffer),
                TRUE, // recursive watch subtree
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned,
                NULL,
                NULL
            );

            if (!success || !isWatching) {
                break;
            }

            auto* notifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            while (notifyInfo != nullptr) {
                std::wstring wname(notifyInfo->FileName, notifyInfo->FileNameLength / sizeof(WCHAR));
                
                // Convert WCHAR to std::string UTF-8
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.length(), NULL, 0, NULL, NULL);
                std::string relativePath(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.length(), &relativePath[0], size_needed, NULL, NULL);

                // Normalize slashes
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                if (!IsTemporaryFile(relativePath)) {
                    std::string fullPath = (std::filesystem::path(watchDir) / relativePath).lexically_normal().string();
                    std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

                    if (notifyInfo->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                        lastOldName = fullPath;
                    } 
                    else if (notifyInfo->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                        ReloadEvent ev{};
                        ev.path = fullPath;
                        ev.oldPath = lastOldName;
                        ev.type = ReloadEventType::Renamed;
                        ev.timestamp = std::chrono::system_clock::now();
                        callback(ev);
                        lastOldName = "";
                    } 
                    else {
                        ReloadEvent ev{};
                        ev.path = fullPath;
                        if (notifyInfo->Action == FILE_ACTION_ADDED) {
                            ev.type = ReloadEventType::Created;
                        } else if (notifyInfo->Action == FILE_ACTION_REMOVED) {
                            ev.type = ReloadEventType::Deleted;
                        } else {
                            ev.type = ReloadEventType::Modified;
                        }
                        ev.timestamp = std::chrono::system_clock::now();
                        callback(ev);
                    }
                }

                if (notifyInfo->NextEntryOffset == 0) {
                    break;
                }
                notifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<uint8_t*>(notifyInfo) + notifyInfo->NextEntryOffset
                );
            }
        }
    }
};
#else
struct FileWatcher::Impl {
    std::string watchDir;
    std::atomic<bool> isWatching{false};
};
#endif

FileWatcher::FileWatcher() : m_impl(std::make_unique<Impl>()) {}
FileWatcher::~FileWatcher() {
    Stop();
}

bool FileWatcher::Start(const std::string& directoryPath, std::function<void(const ReloadEvent&)> callback) {
    Stop();

#ifdef _WIN32
    std::error_code ec;
    auto normDir = std::filesystem::absolute(directoryPath, ec);
    if (ec) {
        Core::Logger::Error("FileWatcher", "Failed to resolve absolute path for: %s", directoryPath.c_str());
        return false;
    }
    m_impl->watchDir = normDir.lexically_normal().string();
    std::replace(m_impl->watchDir.begin(), m_impl->watchDir.end(), '\\', '/');
    m_impl->callback = callback;
    m_impl->isWatching = true;

    std::wstring wpath;
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, m_impl->watchDir.c_str(), (int)m_impl->watchDir.length(), NULL, 0);
    wpath.resize(size_needed);
    MultiByteToWideChar(CP_UTF8, 0, m_impl->watchDir.c_str(), (int)m_impl->watchDir.length(), &wpath[0], size_needed);

    m_impl->hDir = CreateFileW(
        wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (m_impl->hDir == INVALID_HANDLE_VALUE) {
        m_impl->isWatching = false;
        Core::Logger::Error("FileWatcher", "Failed to open directory handle for watching: %s", m_impl->watchDir.c_str());
        return false;
    }

    m_impl->watchThread = std::thread(&Impl::ThreadLoop, m_impl.get());
    Core::Logger::Info("HotReload", "[HotReload] Watching Assets/ at: %s", m_impl->watchDir.c_str());
    return true;
#else
    Core::Logger::Warning("HotReload", "[HotReload] File watching not supported on this platform.");
    return false;
#endif
}

void FileWatcher::Stop() {
#ifdef _WIN32
    if (m_impl->isWatching) {
        m_impl->isWatching = false;
        if (m_impl->hDir != INVALID_HANDLE_VALUE) {
            CloseHandle(m_impl->hDir);
            m_impl->hDir = INVALID_HANDLE_VALUE;
        }
        if (m_impl->watchThread.joinable()) {
            m_impl->watchThread.join();
        }
    }
#endif
}

bool FileWatcher::IsWatching() const {
    return m_impl->isWatching;
}

} // namespace KumariEngine::HotReload
