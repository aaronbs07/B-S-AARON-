#include "vfs.hpp"
#include "compression.hpp"
#include "encryption.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace KumariEngine::Core {

VFS& VFS::Get() {
    static VFS instance;
    return instance;
}

std::string VFS::NormalizePath(const std::string& path) const {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    
    // Remove leading ./ if present
    if (norm.rfind("./", 0) == 0) {
        norm = norm.substr(2);
    }
    return norm;
}

void VFS::Initialize(bool packaged, const std::string& manifestPath, const std::string& decryptionKey) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_packaged = packaged;
    m_manifestPath = manifestPath;
    m_decryptionKey = decryptionKey;
    
    m_guidToEntry.clear();
    m_pathToEntry.clear();

    if (!m_packaged) {
        Logger::Info("VFS", "VFS initialized in Editor/Development mode. Reading directly from disk.");
        return;
    }

    Logger::Info("VFS", "VFS initializing in Packaged mode using manifest: %s", manifestPath.c_str());

    std::filesystem::path p(manifestPath);
    m_manifestDir = p.parent_path().string();
    if (m_manifestDir.empty()) {
        m_manifestDir = ".";
    }

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        Logger::Error("VFS", "Failed to open version manifest at: %s", manifestPath.c_str());
        return;
    }

    std::string line;
    std::string currentBundle;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "VERSION") {
            std::string ver;
            ss >> ver;
            Logger::Info("VFS", "Manifest version: %s", ver.c_str());
        } else if (command == "BUNDLE") {
            ss >> currentBundle;
        } else if (command == "ASSET") {
            VFSEntry entry;
            entry.bundleName = currentBundle;
            ss >> entry.guid >> entry.relativePath >> entry.offset >> entry.compressedSize >> entry.uncompressedSize;
            
            entry.relativePath = NormalizePath(entry.relativePath);

            m_guidToEntry[entry.guid] = entry;
            m_pathToEntry[entry.relativePath] = entry;
        }
    }
    
    Logger::Info("VFS", "VFS manifest loaded successfully. Registered %zu assets.", m_pathToEntry.size());
}

void VFS::Shutdown() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_guidToEntry.clear();
    m_pathToEntry.clear();
    m_packaged = false;
    Logger::Info("VFS", "VFS shut down.");
}

bool VFS::Exists(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::string norm = NormalizePath(path);

    if (!m_packaged) {
        return std::filesystem::exists(norm);
    }

    if (m_pathToEntry.find(norm) != m_pathToEntry.end() || m_guidToEntry.find(norm) != m_guidToEntry.end()) {
        return true;
    }
    
    return std::filesystem::exists(norm);
}

std::vector<uint8_t> VFS::Read(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::string norm = NormalizePath(path);

    if (!m_packaged) {
        std::ifstream file(norm, std::ios::binary);
        if (!file.is_open()) return {};
        
        file.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    VFSEntry entry;
    bool found = false;
    
    auto itPath = m_pathToEntry.find(norm);
    if (itPath != m_pathToEntry.end()) {
        entry = itPath->second;
        found = true;
    } else {
        auto itGuid = m_guidToEntry.find(norm);
        if (itGuid != m_guidToEntry.end()) {
            entry = itGuid->second;
            found = true;
        }
    }

    if (!found) {
        std::ifstream file(norm, std::ios::binary);
        if (!file.is_open()) return {};
        
        file.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    std::filesystem::path bundlePath = std::filesystem::path(m_manifestDir) / entry.bundleName;
    std::ifstream file(bundlePath, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("VFS", "Failed to open asset bundle: %s", bundlePath.string().c_str());
        return {};
    }

    file.seekg(entry.offset, std::ios::beg);
    std::vector<uint8_t> compressedData(entry.compressedSize);
    file.read(reinterpret_cast<char*>(compressedData.data()), entry.compressedSize);

    Encryption::Decrypt(compressedData, m_decryptionKey);

    std::vector<uint8_t> uncompressedData = Compression::Decompress(compressedData, entry.uncompressedSize);
    return uncompressedData;
}

std::string VFS::ReadAsString(const std::string& path) {
    std::vector<uint8_t> buffer = Read(path);
    if (buffer.empty()) return "";
    return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

} // namespace KumariEngine::Core
