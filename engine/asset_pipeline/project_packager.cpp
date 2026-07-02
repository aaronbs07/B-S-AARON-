#include "project_packager.hpp"
#include "asset_database.hpp"
#include "core/compression.hpp"
#include "core/encryption.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>

namespace KumariEngine::Asset {

void ProjectPackager::CollectDependencies(const std::string& assetGuid, std::vector<std::string>& outGuids) {
    if (assetGuid.empty()) return;
    if (std::find(outGuids.begin(), outGuids.end(), assetGuid) != outGuids.end()) {
        return;
    }

    outGuids.push_back(assetGuid);

    std::vector<std::string> deps = AssetDatabase::Get().GetDependencies(assetGuid);
    for (const auto& dep : deps) {
        CollectDependencies(dep, outGuids);
    }
}

bool ProjectPackager::PackProject(const std::string& assetsRoot, const std::string& outputDir, const std::vector<std::string>& rootAssets, const std::string& encryptionKey) {
    Core::Logger::Info("Packager", "Starting project asset packaging. Root: %s, Out: %s", assetsRoot.c_str(), outputDir.c_str());

    std::filesystem::create_directories(outputDir);

    // Initialize Asset Database if needed
    AssetDatabase::Get().Initialize(assetsRoot);
    AssetDatabase::Get().Scan();

    // Resolve dependencies from roots
    std::vector<std::string> referencedGuids;
    for (const auto& rootAssetPath : rootAssets) {
        std::string guid = AssetDatabase::Get().GetAssetGuid(rootAssetPath);
        if (guid.empty()) {
            Core::Logger::Warning("Packager", "Root asset path '%s' has no GUID in database, adding path itself as fallback...", rootAssetPath.c_str());
            // If it is not in the database, we can check if it exists as GUID
            guid = rootAssetPath;
        }
        CollectDependencies(guid, referencedGuids);
    }

    // If no referenced GUIDs found or roots were empty, gather all assets in database as fallback
    if (referencedGuids.empty()) {
        Core::Logger::Warning("Packager", "No dependencies found from roots, packaging all assets in database.");
        const auto& guidMap = AssetDatabase::Get().GetGuidToPathMap();
        for (const auto& pair : guidMap) {
            referencedGuids.push_back(pair.first);
        }
    }

    Core::Logger::Info("Packager", "Dependency analysis completed: %zu referenced assets found.", referencedGuids.size());

    // Separate into bundle groups
    std::vector<std::string> scriptGuids;
    std::vector<std::string> prefabGuids;
    std::vector<std::string> otherGuids;

    for (const auto& guid : referencedGuids) {
        std::string relPath = AssetDatabase::Get().GetAssetPath(guid);
        if (relPath.empty()) {
            // Check if it exists as path
            relPath = guid;
        }
        
        std::string lowerPath = relPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lowerPath.rfind(".lua") == relPath.size() - 4) {
            scriptGuids.push_back(guid);
        } else if (lowerPath.rfind(".prefab") == relPath.size() - 7) {
            prefabGuids.push_back(guid);
        } else {
            otherGuids.push_back(guid);
        }
    }

    struct BundleGroup {
        std::string bundleName;
        std::vector<std::string> guids;
    };

    std::vector<BundleGroup> groups = {
        { "scripts.kpak", scriptGuids },
        { "prefabs.kpak", prefabGuids },
        { "assets.kpak", otherGuids }
    };

    // Open version.manifest for writing
    std::string manifestPath = (std::filesystem::path(outputDir) / "version.manifest").string();
    std::ofstream manifest(manifestPath);
    if (!manifest.is_open()) {
        Core::Logger::Error("Packager", "Failed to create manifest file at: %s", manifestPath.c_str());
        return false;
    }

    manifest << "VERSION 1.0.0\n";

    for (const auto& group : groups) {
        if (group.guids.empty()) continue;

        std::string bundlePath = (std::filesystem::path(outputDir) / group.bundleName).string();
        std::ofstream bundle(bundlePath, std::ios::binary);
        if (!bundle.is_open()) {
            Core::Logger::Error("Packager", "Failed to create asset bundle file: %s", bundlePath.c_str());
            return false;
        }

        manifest << "BUNDLE " << group.bundleName << "\n";

        Core::Logger::Info("Packager", "Packaging bundle '%s' with %zu items...", group.bundleName.c_str(), group.guids.size());

        for (const auto& guid : group.guids) {
            std::string relPath = AssetDatabase::Get().GetAssetPath(guid);
            if (relPath.empty()) {
                relPath = guid;
            }

            std::filesystem::path fullPath = std::filesystem::path(assetsRoot) / relPath;
            if (!std::filesystem::exists(fullPath)) {
                fullPath = relPath;
            }
            std::ifstream assetFile(fullPath, std::ios::binary);
            if (!assetFile.is_open()) {
                Core::Logger::Warning("Packager", "Failed to open asset file: %s", fullPath.string().c_str());
                continue;
            }

            // Read original data
            assetFile.seekg(0, std::ios::end);
            size_t originalSize = static_cast<size_t>(assetFile.tellg());
            assetFile.seekg(0, std::ios::beg);

            std::vector<uint8_t> originalData(originalSize);
            assetFile.read(reinterpret_cast<char*>(originalData.data()), originalSize);
            assetFile.close();

            // Compress
            std::vector<uint8_t> compressedData = Core::Compression::Compress(originalData);

            // Encrypt
            Core::Encryption::Encrypt(compressedData, encryptionKey);

            // Record bundle offset
            uint64_t offset = static_cast<uint64_t>(bundle.tellp());
            uint64_t compressedSize = static_cast<uint64_t>(compressedData.size());

            // Write to bundle
            bundle.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());

            // Write metadata to manifest
            manifest << "ASSET " << guid << " " << relPath << " " << offset << " " << compressedSize << " " << originalSize << "\n";
        }
        bundle.close();
    }

    manifest.close();
    Core::Logger::Info("Packager", "Project packaging completed successfully.");
    return true;
}

} // namespace KumariEngine::Asset
