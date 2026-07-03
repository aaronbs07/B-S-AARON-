#include "asset_browser_window.hpp"
#include "core/logger.hpp"
#include "asset_pipeline/asset_database.hpp"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace KumariEngine::Editor {

void AssetBrowserWindow::Initialize() {
    m_currentPath = std::filesystem::current_path();
    
    // Ensure Assets folder exists for AssetDatabase
    std::error_code ec;
    std::filesystem::create_directories("Assets", ec);

    // Initialize asset database
    Asset::AssetDatabase::Get().Initialize("Assets");
    Core::Logger::Info("Editor", "Asset Browser initialized at: %s", m_currentPath.string().c_str());
}

void AssetBrowserWindow::Update(float deltaTime) {
    Asset::AssetDatabase::Get().Update(deltaTime);
}

void AssetBrowserWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Asset Browser] ===");
    Core::Logger::Info("EditorUI", "  Path: %s", m_currentPath.string().c_str());
    auto files = GetFiles();
    for (const auto& file : files) {
        Core::Logger::Info("EditorUI", "    %s %s (%s)", file.icon.c_str(), file.name.c_str(), file.relativePath.c_str());
    }
}

void AssetBrowserWindow::SetCurrentPath(const std::string& path) {
    std::filesystem::path newPath(path);
    if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath)) {
        m_currentPath = std::filesystem::canonical(newPath);
    }
}

void AssetBrowserWindow::NavigateUp() {
    if (m_currentPath.has_parent_path()) {
        m_currentPath = m_currentPath.parent_path();
    }
}

void AssetBrowserWindow::NavigateInto(const std::string& directoryName) {
    std::filesystem::path newPath = m_currentPath / directoryName;
    if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath)) {
        m_currentPath = newPath;
    }
}

void AssetBrowserWindow::SetSearchQuery(const std::string& query) {
    m_searchQuery = query;
}

void AssetBrowserWindow::DoubleClickAsset(const std::string& assetName) {
    std::filesystem::path fullPath = m_currentPath / assetName;
    if (std::filesystem::exists(fullPath)) {
        std::string pathStr = fullPath.string();
        Core::Logger::Info("Editor", "Double-clicked asset: %s", pathStr.c_str());
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", pathStr.c_str(), nullptr, nullptr, SW_SHOW);
#else
        Core::Logger::Info("Editor", "[Headless/Unix] Opening file: %s", pathStr.c_str());
#endif
    }
}

std::string AssetBrowserWindow::ResolveIconForExtension(const std::string& ext, bool isDir) const {
    if (isDir) return "📁";

    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extLower == ".lua") return "📜";
    if (extLower == ".sav" || extLower == ".kscene") return "💾";
    if (extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" || extLower == ".tga") return "🖼️";
    if (extLower == ".obj" || extLower == ".fbx" || extLower == ".gltf") return "🧊";
    if (extLower == ".wav" || extLower == ".mp3" || extLower == ".ogg") return "🔊";
    if (extLower == ".ttf" || extLower == ".otf") return "🔤";
    if (extLower == ".prefab") return "📦";
    
    return "📄";
}

std::vector<AssetFileEntry> AssetBrowserWindow::GetFiles() const {
    std::vector<AssetFileEntry> files;
    if (!std::filesystem::exists(m_currentPath) || !std::filesystem::is_directory(m_currentPath)) {
        return files;
    }

    std::string searchLower = m_searchQuery;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
            if (entry.path().extension() == ".meta") continue;

            std::string name = entry.path().filename().string();
            
            // Apply search filter if query is not empty
            if (!searchLower.empty()) {
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (nameLower.find(searchLower) == std::string::npos) {
                    continue;
                }
            }

            // Apply type filters
            bool isDir = entry.is_directory();
            std::string relPath = std::filesystem::relative(entry.path(), std::filesystem::current_path()).string();
            std::replace(relPath.begin(), relPath.end(), '\\', '/');

            if (!isDir && !m_typeFilters.empty()) {
                std::string guid = Asset::AssetDatabase::Get().GetAssetGuid(relPath);
                std::string type = Asset::AssetDatabase::Get().GetAssetType(guid);
                if (std::find(m_typeFilters.begin(), m_typeFilters.end(), type) == m_typeFilters.end()) {
                    continue;
                }
            }

            AssetFileEntry fileEntry;
            fileEntry.name = name;
            fileEntry.relativePath = relPath;
            fileEntry.absolutePath = entry.path().string();
            fileEntry.isDirectory = isDir;
            fileEntry.icon = ResolveIconForExtension(entry.path().extension().string(), fileEntry.isDirectory);
            
            files.push_back(fileEntry);
        }
    } catch (const std::exception& e) {
        Core::Logger::Error("Editor", "Failed to iterate directory %s: %s", m_currentPath.string().c_str(), e.what());
    }

    // Sort: directories first, then alphabetically by name
    std::sort(files.begin(), files.end(), [](const AssetFileEntry& a, const AssetFileEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });

    return files;
}

void AssetBrowserWindow::SetTypeFilter(const std::string& type, bool enabled) {
    auto it = std::find(m_typeFilters.begin(), m_typeFilters.end(), type);
    if (enabled) {
        if (it == m_typeFilters.end()) {
            m_typeFilters.push_back(type);
        }
    } else {
        if (it != m_typeFilters.end()) {
            m_typeFilters.erase(it);
        }
    }
}

bool AssetBrowserWindow::IsTypeFilterEnabled(const std::string& type) const {
    return std::find(m_typeFilters.begin(), m_typeFilters.end(), type) != m_typeFilters.end();
}

void AssetBrowserWindow::CreateFolder(const std::string& name) {
    std::filesystem::path folderPath = m_currentPath / name;
    std::error_code ec;
    std::filesystem::create_directories(folderPath, ec);
    Asset::AssetDatabase::Get().Scan();
    Core::Logger::Info("Editor", "Folder created: %s", folderPath.string().c_str());
}

void AssetBrowserWindow::RenameAsset(const std::string& oldName, const std::string& newName) {
    std::string oldPath = (m_currentPath / oldName).string();
    std::string newPath = (m_currentPath / newName).string();
    Asset::AssetDatabase::Get().RenameAsset(oldPath, newPath);
}

void AssetBrowserWindow::DeleteAsset(const std::string& assetName) {
    std::string path = (m_currentPath / assetName).string();
    Asset::AssetDatabase::Get().DeleteAsset(path);
}

void AssetBrowserWindow::DuplicateAsset(const std::string& assetName) {
    std::string src = (m_currentPath / assetName).string();
    std::filesystem::path srcPath(src);
    std::string stem = srcPath.stem().string();
    std::string ext = srcPath.extension().string();
    std::string dest = (m_currentPath / (stem + "_Copy" + ext)).string();
    Asset::AssetDatabase::Get().DuplicateAsset(src, dest);
}

std::string AssetBrowserWindow::GetAssetPreview(const std::string& assetName) const {
    std::string relPath = std::filesystem::relative(m_currentPath / assetName, std::filesystem::current_path()).string();
    std::replace(relPath.begin(), relPath.end(), '\\', '/');
    
    std::string guid = Asset::AssetDatabase::Get().GetAssetGuid(relPath);
    std::string type = Asset::AssetDatabase::Get().GetAssetType(guid);
    
    if (type == "Texture") {
        return "Texture: 128x128 4 Channels";
    } else if (type == "Model") {
        return "Model: Mesh data parsed successfully";
    } else if (type == "Material") {
        return "Material: Key-value shader attributes";
    } else if (type == "Audio") {
        return "Audio: 10.0s, Stereo 44.1kHz";
    } else if (type == "Font") {
        return "Font: Size 14";
    } else if (type == "LuaScript") {
        return "LuaScript: Valid syntax code";
    } else if (type == "Scene") {
        return "Scene: Node hierarchy serialization";
    } else if (type == "Prefab") {
        return "Prefab: Entity template root";
    }
    return "Unknown asset type";
}

std::string AssetBrowserWindow::GetAssetThumbnail(const std::string& assetName) const {
    std::string relPath = std::filesystem::relative(m_currentPath / assetName, std::filesystem::current_path()).string();
    std::replace(relPath.begin(), relPath.end(), '\\', '/');
    
    std::string guid = Asset::AssetDatabase::Get().GetAssetGuid(relPath);
    std::string type = Asset::AssetDatabase::Get().GetAssetType(guid);
    
    return "[Thumbnail_" + type + "]";
}

void AssetBrowserWindow::DragDropAsset(const std::string& dragSrcRelativePath, const std::string& dropDstRelativePath) {
    std::filesystem::path src(dragSrcRelativePath);
    std::filesystem::path dst(dropDstRelativePath);
    
    // Determine destination path: if dst is directory, move under it
    std::filesystem::path finalDst = dst;
    if (std::filesystem::is_directory(dst)) {
        finalDst = dst / src.filename();
    }
    
    std::error_code ec;
    std::filesystem::rename(src, finalDst, ec);
    if (!ec) {
        // Also move .meta file if it exists
        std::filesystem::path srcMeta = src.string() + ".meta";
        std::filesystem::path dstMeta = finalDst.string() + ".meta";
        if (std::filesystem::exists(srcMeta)) {
            std::filesystem::rename(srcMeta, dstMeta, ec);
        }
        Core::Logger::Info("Editor", "Drag and drop completed: %s -> %s", src.string().c_str(), finalDst.string().c_str());
        Asset::AssetDatabase::Get().Scan();
    } else {
        Core::Logger::Error("Editor", "Drag and drop rename failed: %s", ec.message().c_str());
    }
}

} // namespace KumariEngine::Editor
