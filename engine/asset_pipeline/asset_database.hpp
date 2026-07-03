#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include "resource/resource.hpp"
#include "scripting/file_watcher.hpp"

namespace KumariEngine::Asset {

struct AssetMetadata {
    std::string guid;
    std::string type;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::string> importerSettings;
};

class AssetDatabase {
public:
    static AssetDatabase& Get() {
        static AssetDatabase instance;
        return instance;
    }

    void Initialize(const std::string& assetsPath);
    void Shutdown();

    void Scan();

    // Asset Management APIs
    std::string GetAssetPath(const std::string& guid) const;
    std::string GetAssetGuid(const std::string& relativePath) const;
    std::string GetAssetType(const std::string& guid) const;
    
    bool RegisterAsset(const std::string& relativePath, const std::string& guid, const std::string& type);
    bool UnregisterAsset(const std::string& relativePath);
    
    // Dependencies
    void RegisterDependency(const std::string& assetGuid, const std::string& dependencyGuid);
    void UnregisterDependency(const std::string& assetGuid, const std::string& dependencyGuid);
    std::vector<std::string> GetDependencies(const std::string& guid) const;
    std::vector<std::string> GetReferencers(const std::string& guid) const;
    
    // Missing Reference Detection & Recovery
    std::vector<std::pair<std::string, std::string>> GetMissingReferences() const;
    void ResolveMissingReference(const std::string& missingGuid, const std::string& replacementGuid);

    // Operations (affect both asset file and .meta file)
    bool CreateFolder(const std::string& folderPath);
    bool RenameAsset(const std::string& oldPath, const std::string& newPath);
    bool DeleteAsset(const std::string& path);
    bool DuplicateAsset(const std::string& sourcePath, const std::string& destPath);

    // Live update
    void Update(float dt);

    // Metadata read/write helper
    bool LoadMetadata(const std::string& assetPath, AssetMetadata& outMeta);
    bool SaveMetadata(const std::string& assetPath, const AssetMetadata& meta);

    const std::unordered_map<std::string, std::string>& GetGuidToPathMap() const { return m_guidToPath; }
    const std::string& GetAssetsRoot() const { return m_assetsRoot; }
    void ReimportAsset(const std::string& path);

private:
    AssetDatabase() = default;
    ~AssetDatabase() = default;

    std::string m_assetsRoot;
    std::unordered_map<std::string, std::string> m_guidToPath; // guid -> relative path (normalized, forward slashes)
    std::unordered_map<std::string, std::string> m_pathToGuid; // relative path -> guid
    std::unordered_map<std::string, std::string> m_guidToType; // guid -> type
    std::unordered_map<std::string, std::shared_ptr<Resource::Resource>> m_loadedAssets; // relativePath -> loaded resource
    
    // Dependency Maps
    std::unordered_map<std::string, std::vector<std::string>> m_dependencies; // guid -> dependent guids
    std::unordered_map<std::string, std::vector<std::string>> m_referencers;  // guid -> guids that depend on this
    
    // File Watcher for Live Reimport
    std::unique_ptr<Scripting::FileWatcher> m_fileWatcher;

    void ScanDirectory(const std::filesystem::path& dir);
    void DetectRenamedMovedAssets();
    std::string GetTypeFromExtension(const std::string& ext) const;
    std::string NormalizePath(const std::string& path) const;
};

} // namespace KumariEngine::Asset
