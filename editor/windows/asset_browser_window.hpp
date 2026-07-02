#pragma once
#include "editor/window_system.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace KumariEngine::Editor {

struct AssetFileEntry {
    std::string name;
    std::string relativePath;
    std::string absolutePath;
    bool isDirectory = false;
    std::string icon = "📄";
};

class AssetBrowserWindow : public EditorWindow {
public:
    AssetBrowserWindow() : EditorWindow("Asset Browser") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    void SetCurrentPath(const std::string& path);
    std::string GetCurrentPath() const { return m_currentPath.string(); }
    void NavigateUp();
    void NavigateInto(const std::string& directoryName);

    void SetSearchQuery(const std::string& query);
    void DoubleClickAsset(const std::string& assetName);

    std::vector<AssetFileEntry> GetFiles() const;
    std::string ResolveIconForExtension(const std::string& ext, bool isDir) const;

    void SetTypeFilter(const std::string& type, bool enabled);
    bool IsTypeFilterEnabled(const std::string& type) const;
    void CreateFolder(const std::string& name);
    void RenameAsset(const std::string& oldName, const std::string& newName);
    void DeleteAsset(const std::string& assetName);
    void DuplicateAsset(const std::string& assetName);
    std::string GetAssetPreview(const std::string& assetName) const;
    std::string GetAssetThumbnail(const std::string& assetName) const;
    void DragDropAsset(const std::string& dragSrcRelativePath, const std::string& dropDstRelativePath);

private:
    std::filesystem::path m_currentPath;
    std::string m_searchQuery = "";
    std::vector<std::string> m_typeFilters;
};

} // namespace KumariEngine::Editor
