#include "asset_database.hpp"
#include "scripting/file_watcher.hpp"
#include "scripting/script_engine.hpp"
#include "resource/resource_manager.hpp"
#include "resource/resource_types.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iostream>

namespace KumariEngine::Asset {

static std::string GenerateGUID() {
    static const char hex[] = "0123456789abcdef";
    std::string guid;
    guid.reserve(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    for (int i = 0; i < 32; ++i) {
        guid += hex[dis(gen)];
    }
    return guid;
}

std::string AssetDatabase::NormalizePath(const std::string& path) const {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    // Remove leading "./" if present
    if (norm.rfind("./", 0) == 0) {
        norm = norm.substr(2);
    }
    return norm;
}

void AssetDatabase::Initialize(const std::string& assetsPath) {
    m_assetsRoot = NormalizePath(assetsPath);
    std::error_code ec;
    if (!std::filesystem::exists(m_assetsRoot, ec)) {
        std::filesystem::create_directories(m_assetsRoot, ec);
    }

    m_fileWatcher = std::make_unique<Scripting::FileWatcher>();
    m_fileWatcher->AddDirectory(m_assetsRoot);
    
    Core::Logger::Info("AssetDatabase", "Asset Database initialized at: %s", m_assetsRoot.c_str());
    Scan();
}

void AssetDatabase::Shutdown() {
    m_fileWatcher.reset();
    m_guidToPath.clear();
    m_pathToGuid.clear();
    m_guidToType.clear();
    m_loadedAssets.clear();
    m_dependencies.clear();
    m_referencers.clear();
    Core::Logger::Info("AssetDatabase", "Asset Database shut down.");
}

void AssetDatabase::Scan() {
    Core::Logger::Info("AssetDatabase", "Scanning assets root: %s", m_assetsRoot.c_str());
    
    // Step 1: Detect renamed and moved assets
    DetectRenamedMovedAssets();

    // Step 2: Recursively scan the directory
    ScanDirectory(m_assetsRoot);
}

void AssetDatabase::ScanDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string normPath = NormalizePath(path);
            
            // Skip .meta files
            if (entry.path().extension() == ".meta") {
                continue;
            }

            // Load or create metadata
            AssetMetadata meta;
            if (!LoadMetadata(normPath, meta)) {
                // Generate a new metadata file
                meta.guid = GenerateGUID();
                meta.type = GetTypeFromExtension(entry.path().extension().string());
                SaveMetadata(normPath, meta);
                Core::Logger::Info("AssetDatabase", "Generated meta file for: %s (GUID: %s)", normPath.c_str(), meta.guid.c_str());
            }

            // Register asset
            RegisterAsset(normPath, meta.guid, meta.type);

            // Re-populate dependencies
            for (const auto& dep : meta.dependencies) {
                RegisterDependency(meta.guid, dep);
            }

            // Run import/reimport to generate engine-ready data
            ReimportAsset(normPath);
        }
    }
}

void AssetDatabase::DetectRenamedMovedAssets() {
    // Collect all files currently on disk
    std::unordered_map<std::string, uint64_t> diskFiles; // relativePath -> file_size
    std::vector<std::string> untrackedFiles;
    
    std::error_code ec;
    if (!std::filesystem::exists(m_assetsRoot, ec)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(m_assetsRoot, ec)) {
        if (entry.is_regular_file() && entry.path().extension() != ".meta") {
            std::string normPath = NormalizePath(entry.path().string());
            diskFiles[normPath] = entry.file_size(ec);
            
            // If the path is not registered in our database, it's a candidate for a move target
            if (m_pathToGuid.find(normPath) == m_pathToGuid.end()) {
                untrackedFiles.push_back(normPath);
            }
        }
    }

    // Find registered assets that are missing from disk
    std::vector<std::string> missingAssets;
    for (const auto& pair : m_pathToGuid) {
        if (diskFiles.find(pair.first) == diskFiles.end()) {
            missingAssets.push_back(pair.first);
        }
    }

    // Attempt to match missing assets with untracked assets by file size and name/type
    for (const auto& missingPath : missingAssets) {
        std::string guid = m_pathToGuid[missingPath];
        std::string type = m_guidToType[guid];
        std::filesystem::path mp(missingPath);
        std::string missingExt = mp.extension().string();

        for (auto it = untrackedFiles.begin(); it != untrackedFiles.end(); ++it) {
            std::filesystem::path up(*it);
            if (up.extension().string() == missingExt) {
                // Found a candidate. Let's move the .meta file from missingPath to up
                std::string oldMetaPath = missingPath + ".meta";
                std::string newMetaPath = *it + ".meta";
                
                std::error_code renameEc;
                if (std::filesystem::exists(oldMetaPath, renameEc)) {
                    std::filesystem::rename(oldMetaPath, newMetaPath, renameEc);
                    if (!renameEc) {
                        Core::Logger::Info("AssetDatabase", "Detected moved asset: %s -> %s (Preserving GUID: %s)", missingPath.c_str(), it->c_str(), guid.c_str());
                        
                        // Update in-memory mapping
                        m_pathToGuid.erase(missingPath);
                        m_guidToPath[guid] = *it;
                        m_pathToGuid[*it] = guid;
                        
                        // Remove from untracked candidates
                        untrackedFiles.erase(it);
                        break;
                    }
                }
            }
        }
    }
}

std::string AssetDatabase::GetAssetPath(const std::string& guid) const {
    auto it = m_guidToPath.find(guid);
    if (it != m_guidToPath.end()) {
        return it->second;
    }
    return "";
}

std::string AssetDatabase::GetAssetGuid(const std::string& relativePath) const {
    auto norm = NormalizePath(relativePath);
    auto it = m_pathToGuid.find(norm);
    if (it != m_pathToGuid.end()) {
        return it->second;
    }
    return "";
}

std::string AssetDatabase::GetAssetType(const std::string& guid) const {
    auto it = m_guidToType.find(guid);
    if (it != m_guidToType.end()) {
        return it->second;
    }
    return "Unknown";
}

bool AssetDatabase::RegisterAsset(const std::string& relativePath, const std::string& guid, const std::string& type) {
    auto norm = NormalizePath(relativePath);
    m_guidToPath[guid] = norm;
    m_pathToGuid[norm] = guid;
    m_guidToType[guid] = type;
    return true;
}

bool AssetDatabase::UnregisterAsset(const std::string& relativePath) {
    auto norm = NormalizePath(relativePath);
    auto it = m_pathToGuid.find(norm);
    if (it != m_pathToGuid.end()) {
        std::string guid = it->second;
        m_guidToPath.erase(guid);
        m_guidToType.erase(guid);
        m_pathToGuid.erase(it);
        
        // Remove dependencies
        m_dependencies.erase(guid);
        
        // Remove from referencers
        for (auto& pair : m_referencers) {
            auto& refs = pair.second;
            refs.erase(std::remove(refs.begin(), refs.end(), guid), refs.end());
        }
        return true;
    }
    return false;
}

void AssetDatabase::RegisterDependency(const std::string& assetGuid, const std::string& dependencyGuid) {
    if (dependencyGuid.empty()) return;
    auto& deps = m_dependencies[assetGuid];
    if (std::find(deps.begin(), deps.end(), dependencyGuid) == deps.end()) {
        deps.push_back(dependencyGuid);
    }

    auto& refs = m_referencers[dependencyGuid];
    if (std::find(refs.begin(), refs.end(), assetGuid) == refs.end()) {
        refs.push_back(assetGuid);
    }
}

void AssetDatabase::UnregisterDependency(const std::string& assetGuid, const std::string& dependencyGuid) {
    auto it = m_dependencies.find(assetGuid);
    if (it != m_dependencies.end()) {
        auto& deps = it->second;
        deps.erase(std::remove(deps.begin(), deps.end(), dependencyGuid), deps.end());
    }

    auto rit = m_referencers.find(dependencyGuid);
    if (rit != m_referencers.end()) {
        auto& refs = rit->second;
        refs.erase(std::remove(refs.begin(), refs.end(), assetGuid), refs.end());
    }
}

std::vector<std::string> AssetDatabase::GetDependencies(const std::string& guid) const {
    auto it = m_dependencies.find(guid);
    if (it != m_dependencies.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> AssetDatabase::GetReferencers(const std::string& guid) const {
    auto it = m_referencers.find(guid);
    if (it != m_referencers.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> AssetDatabase::GetMissingReferences() const {
    std::vector<std::pair<std::string, std::string>> missing;
    for (const auto& [assetGuid, deps] : m_dependencies) {
        for (const auto& depGuid : deps) {
            if (m_guidToPath.find(depGuid) == m_guidToPath.end()) {
                missing.push_back({assetGuid, depGuid});
            }
        }
    }
    return missing;
}

void AssetDatabase::ResolveMissingReference(const std::string& missingGuid, const std::string& replacementGuid) {
    auto referencers = GetReferencers(missingGuid);
    for (const auto& refGuid : referencers) {
        // Update dependency registration
        UnregisterDependency(refGuid, missingGuid);
        RegisterDependency(refGuid, replacementGuid);

        // Update the file contents of the referencer if it references the missing GUID!
        std::string refPath = GetAssetPath(refGuid);
        if (!refPath.empty()) {
            std::ifstream file(refPath);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                file.close();
                
                std::string contents = ss.str();
                size_t pos = 0;
                bool changed = false;
                while ((pos = contents.find(missingGuid, pos)) != std::string::npos) {
                    contents.replace(pos, missingGuid.length(), replacementGuid);
                    pos += replacementGuid.length();
                    changed = true;
                }
                
                if (changed) {
                    std::ofstream outFile(refPath);
                    outFile << contents;
                    outFile.close();
                    
                    // Reimport referencer
                    ReimportAsset(refPath);
                }
            }

            // Also update .meta of referencer if dependencies are stored there
            AssetMetadata refMeta;
            if (LoadMetadata(refPath, refMeta)) {
                auto& metaDeps = refMeta.dependencies;
                auto it = std::find(metaDeps.begin(), metaDeps.end(), missingGuid);
                if (it != metaDeps.end()) {
                    *it = replacementGuid;
                    SaveMetadata(refPath, refMeta);
                }
            }
        }
    }
}

bool AssetDatabase::CreateFolder(const std::string& folderPath) {
    std::string norm = NormalizePath(m_assetsRoot + "/" + folderPath);
    std::error_code ec;
    return std::filesystem::create_directories(norm, ec);
}

bool AssetDatabase::RenameAsset(const std::string& oldPath, const std::string& newPath) {
    std::string oldNorm = NormalizePath(oldPath);
    std::string newNorm = NormalizePath(newPath);

    std::error_code ec;
    if (!std::filesystem::exists(oldNorm, ec)) return false;

    // Move file
    std::filesystem::rename(oldNorm, newNorm, ec);
    if (ec) return false;

    // Move meta file
    std::string oldMeta = oldNorm + ".meta";
    std::string newMeta = newNorm + ".meta";
    if (std::filesystem::exists(oldMeta, ec)) {
        std::filesystem::rename(oldMeta, newMeta, ec);
    }

    // Update DB
    auto it = m_pathToGuid.find(oldNorm);
    if (it != m_pathToGuid.end()) {
        std::string guid = it->second;
        m_pathToGuid.erase(it);
        m_pathToGuid[newNorm] = guid;
        m_guidToPath[guid] = newNorm;
    }

    Core::Logger::Info("AssetDatabase", "Renamed asset from %s to %s", oldNorm.c_str(), newNorm.c_str());
    ReimportAsset(newNorm);
    return true;
}

bool AssetDatabase::DeleteAsset(const std::string& path) {
    std::string norm = NormalizePath(path);
    std::error_code ec;
    if (!std::filesystem::exists(norm, ec)) return false;

    // Delete file
    std::filesystem::remove(norm, ec);
    
    // Delete meta
    std::string metaPath = norm + ".meta";
    if (std::filesystem::exists(metaPath, ec)) {
        std::filesystem::remove(metaPath, ec);
    }

    UnregisterAsset(norm);
    Core::Logger::Info("AssetDatabase", "Deleted asset: %s", norm.c_str());
    return true;
}

bool AssetDatabase::DuplicateAsset(const std::string& sourcePath, const std::string& destPath) {
    std::string srcNorm = NormalizePath(sourcePath);
    std::string dstNorm = NormalizePath(destPath);

    std::error_code ec;
    if (!std::filesystem::exists(srcNorm, ec)) return false;

    // Copy file
    std::filesystem::copy(srcNorm, dstNorm, ec);
    if (ec) return false;

    // Generate new GUID for duplicated file
    AssetMetadata srcMeta;
    AssetMetadata dstMeta;
    dstMeta.guid = GenerateGUID();
    dstMeta.type = GetTypeFromExtension(std::filesystem::path(dstNorm).extension().string());
    
    if (LoadMetadata(srcNorm, srcMeta)) {
        dstMeta.dependencies = srcMeta.dependencies;
        dstMeta.importerSettings = srcMeta.importerSettings;
    }
    
    SaveMetadata(dstNorm, dstMeta);
    RegisterAsset(dstNorm, dstMeta.guid, dstMeta.type);
    
    for (const auto& dep : dstMeta.dependencies) {
        RegisterDependency(dstMeta.guid, dep);
    }

    Core::Logger::Info("AssetDatabase", "Duplicated asset %s to %s with new GUID %s", srcNorm.c_str(), dstNorm.c_str(), dstMeta.guid.c_str());
    ReimportAsset(dstNorm);
    return true;
}

void AssetDatabase::Update(float dt) {
    (void)dt;
    if (!m_fileWatcher) return;

    m_fileWatcher->Update([this](const std::string& filePath, Scripting::FileEvent event) {
        std::string norm = NormalizePath(filePath);
        if (filePath.find(".meta") != std::string::npos) return; // ignore meta updates

        if (event == Scripting::FileEvent::Modified) {
            Core::Logger::Info("AssetDatabase", "File modified: %s. Reimporting...", norm.c_str());
            ReimportAsset(norm);
        } else if (event == Scripting::FileEvent::Added) {
            Core::Logger::Info("AssetDatabase", "File added: %s. Scanning...", norm.c_str());
            Scan();
        } else if (event == Scripting::FileEvent::Deleted) {
            Core::Logger::Info("AssetDatabase", "File deleted: %s. Removing...", norm.c_str());
            UnregisterAsset(norm);
        }
    });
}

void AssetDatabase::ReimportAsset(const std::string& path) {
    std::string normPath = NormalizePath(path);
    std::string ext = std::filesystem::path(normPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string guid = GetAssetGuid(normPath);
    if (guid.empty()) return;

    std::shared_ptr<Resource::Resource> importedRes;

    // Support importing different asset types
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
        auto tex = std::make_shared<Resource::TextureAsset>();
        tex->width = 128; // mock values
        tex->height = 128;
        tex->channels = 4;
        tex->pixelData.assign(128 * 128 * 4, 255);
        Resource::ResourceManager::Get().Add<Resource::TextureAsset>(normPath, tex);
        importedRes = tex;
    } 
    else if (ext == ".obj") {
        auto model = std::make_shared<Resource::ModelAsset>();
        std::ifstream file(normPath);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.rfind("v ", 0) == 0) {
                    float x, y, z;
                    if (sscanf_s(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3) {
                        Resource::ModelAsset::Vertex v{};
                        v.position = glm::vec3(x, y, z);
                        model->vertices.push_back(v);
                    }
                } else if (line.rfind("f ", 0) == 0) {
                    int v1, v2, v3;
                    if (sscanf_s(line.c_str() + 2, "%d %d %d", &v1, &v2, &v3) == 3) {
                        model->indices.push_back(v1 - 1);
                        model->indices.push_back(v2 - 1);
                        model->indices.push_back(v3 - 1);
                    }
                }
            }
            file.close();
        }
        Resource::ResourceManager::Get().Add<Resource::ModelAsset>(normPath, model);
        importedRes = model;
    } 
    else if (ext == ".mat" || ext == ".kmat") {
        auto mat = std::make_shared<Resource::MaterialAsset>();
        std::ifstream file(normPath);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    key.erase(0, key.find_first_not_of(" \t\r\n"));
                    key.erase(key.find_last_not_of(" \t\r\n") + 1);
                    val.erase(0, val.find_first_not_of(" \t\r\n"));
                    val.erase(val.find_last_not_of(" \t\r\n") + 1);

                    if (key == "color") {
                        float r, g, b;
                        if (sscanf_s(val.c_str(), "%f %f %f", &r, &g, &b) == 3) {
                            mat->albedoColor = glm::vec3(r, g, b);
                        }
                    } else if (key == "texture") {
                        mat->albedoTextureGuid = val;
                        RegisterDependency(guid, val);
                    } else if (key == "metallic") {
                        mat->metallic = std::stof(val);
                    } else if (key == "roughness") {
                        mat->roughness = std::stof(val);
                    }
                }
            }
            file.close();
        }
        Resource::ResourceManager::Get().Add<Resource::MaterialAsset>(normPath, mat);
        importedRes = mat;
    } 
    else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
        auto audio = std::make_shared<Resource::AudioAsset>();
        audio->duration = 10.0f;
        audio->channels = 2;
        audio->sampleRate = 44100;
        Resource::ResourceManager::Get().Add<Resource::AudioAsset>(normPath, audio);
        importedRes = audio;
    } 
    else if (ext == ".ttf" || ext == ".otf") {
        auto font = std::make_shared<Resource::FontAsset>();
        font->fontName = std::filesystem::path(normPath).stem().string();
        font->size = 14;
        Resource::ResourceManager::Get().Add<Resource::FontAsset>(normPath, font);
        importedRes = font;
    } 
    else if (ext == ".lua") {
        auto script = std::make_shared<Resource::LuaScriptAsset>();
        std::ifstream file(normPath);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            script->sourceCode = ss.str();
            file.close();

            // Dependency parsing: e.g. require("scripts/other") or loading by GUID
            size_t pos = 0;
            std::string code = script->sourceCode;
            while ((pos = code.find("require", pos)) != std::string::npos) {
                size_t startQuote = code.find_first_of("\"'", pos);
                if (startQuote != std::string::npos) {
                    size_t endQuote = code.find_first_of("\"'", startQuote + 1);
                    if (endQuote != std::string::npos) {
                        std::string reqPath = code.substr(startQuote + 1, endQuote - startQuote - 1);
                        std::string depGuid = GetAssetGuid("Assets/" + reqPath + ".lua");
                        if (!depGuid.empty()) {
                            RegisterDependency(guid, depGuid);
                        }
                    }
                }
                pos += 7;
            }
        }
        Resource::ResourceManager::Get().Add<Resource::LuaScriptAsset>(normPath, script);
        importedRes = script;
        
        // Also trigger script reload in hot reload / script engine
        Scripting::ScriptEngine::Get().ReloadScript(normPath);
    } 
    else if (ext == ".sav" || ext == ".kscene") {
        auto scene = std::make_shared<Resource::SceneAsset>();
        scene->sceneName = std::filesystem::path(normPath).stem().string();
        std::ifstream file(normPath, std::ios::binary);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            scene->serializedData = ss.str();
            file.close();
            
            // Scan scene dependencies by searching for GUID patterns (32 character hex)
            std::string data = scene->serializedData;
            // Scan for any 32-hex string that is registered in our asset database
            for (size_t i = 0; i < data.size(); ++i) {
                if (i + 32 <= data.size()) {
                    std::string potentialGuid = data.substr(i, 32);
                    if (m_guidToPath.find(potentialGuid) != m_guidToPath.end()) {
                        if (potentialGuid != guid) { // Avoid self-dependency
                            RegisterDependency(guid, potentialGuid);
                        }
                    }
                }
            }
        }
        Resource::ResourceManager::Get().Add<Resource::SceneAsset>(normPath, scene);
        importedRes = scene;
    }

    if (importedRes) {
        m_loadedAssets[normPath] = importedRes;
    }
}

std::string AssetDatabase::GetTypeFromExtension(const std::string& ext) const {
    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" || extLower == ".tga") return "Texture";
    if (extLower == ".obj" || extLower == ".fbx" || extLower == ".gltf") return "Model";
    if (extLower == ".mat" || extLower == ".kmat") return "Material";
    if (extLower == ".wav" || extLower == ".mp3" || extLower == ".ogg") return "Audio";
    if (extLower == ".ttf" || extLower == ".otf") return "Font";
    if (extLower == ".lua") return "LuaScript";
    if (extLower == ".sav" || extLower == ".kscene") return "Scene";
    if (extLower == ".prefab") return "Prefab";
    return "Unknown";
}

bool AssetDatabase::LoadMetadata(const std::string& assetPath, AssetMetadata& outMeta) {
    std::string metaPath = assetPath + ".meta";
    std::ifstream file(metaPath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);

            if (key == "guid") {
                outMeta.guid = val;
            } else if (key == "type") {
                outMeta.type = val;
            } else if (key == "dependencies") {
                outMeta.dependencies.clear();
                std::stringstream ss(val);
                std::string dep;
                while (std::getline(ss, dep, ',')) {
                    dep.erase(0, dep.find_first_not_of(" \t\r\n"));
                    dep.erase(dep.find_last_not_of(" \t\r\n") + 1);
                    if (!dep.empty()) {
                        outMeta.dependencies.push_back(dep);
                    }
                }
            } else if (key.rfind("settings_", 0) == 0) {
                outMeta.importerSettings[key.substr(9)] = val;
            }
        }
    }
    file.close();
    return !outMeta.guid.empty();
}

bool AssetDatabase::SaveMetadata(const std::string& assetPath, const AssetMetadata& meta) {
    std::string metaPath = assetPath + ".meta";
    std::ofstream file(metaPath);
    if (!file.is_open()) return false;

    file << "guid: " << meta.guid << "\n";
    file << "type: " << meta.type << "\n";
    file << "dependencies: ";
    for (size_t i = 0; i < meta.dependencies.size(); ++i) {
        file << meta.dependencies[i];
        if (i + 1 < meta.dependencies.size()) file << ",";
    }
    file << "\n";

    for (const auto& [k, v] : meta.importerSettings) {
        file << "settings_" << k << ": " << v << "\n";
    }
    file.close();
    return true;
}

} // namespace KumariEngine::Asset
