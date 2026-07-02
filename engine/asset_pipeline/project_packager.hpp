#pragma once
#include <string>
#include <vector>

namespace KumariEngine::Asset {

class ProjectPackager {
public:
    static bool PackProject(const std::string& assetsRoot, const std::string& outputDir, const std::vector<std::string>& rootAssets, const std::string& encryptionKey = "KumariKandamKey!");

private:
    static void CollectDependencies(const std::string& assetGuid, std::vector<std::string>& outGuids);
};

} // namespace KumariEngine::Asset
