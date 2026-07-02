#pragma once
#include <string>

namespace KumariEngine::Editor {

struct EditorConfig {
    bool enableDocking = true;
    bool startMaximized = true;
    std::string assetRootPath = "./";
    float updateIntervalMs = 16.67f;
    bool showStats = true;

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
};

} // namespace KumariEngine::Editor
