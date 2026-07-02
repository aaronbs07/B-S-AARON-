#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Core {

struct VFSEntry {
    std::string guid;
    std::string relativePath; // Normalized forward-slash path
    std::string bundleName;
    uint64_t offset = 0;
    uint64_t compressedSize = 0;
    uint64_t uncompressedSize = 0;
};

class VFS {
public:
    static VFS& Get();

    void Initialize(bool packaged, const std::string& manifestPath = "", const std::string& decryptionKey = "KumariKandamKey!");
    void Shutdown();

    bool Exists(const std::string& path);
    std::vector<uint8_t> Read(const std::string& path);
    std::string ReadAsString(const std::string& path);

    bool IsPackaged() const { return m_packaged; }
    std::string GetDecryptionKey() const { return m_decryptionKey; }

private:
    VFS() = default;
    ~VFS() = default;

    std::string NormalizePath(const std::string& path) const;

    bool m_packaged = false;
    std::string m_manifestPath;
    std::string m_decryptionKey;
    std::string m_manifestDir;

    std::shared_mutex m_mutex;
    std::unordered_map<std::string, VFSEntry> m_guidToEntry;
    std::unordered_map<std::string, VFSEntry> m_pathToEntry;
};

} // namespace KumariEngine::Core
