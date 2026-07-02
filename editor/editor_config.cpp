#include "editor_config.hpp"
#include <fstream>
#include <sstream>
#include "core/logger.hpp"

namespace KumariEngine::Editor {

bool EditorConfig::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Core::Logger::Warning("EditorConfig", "Failed to open config file for loading, using defaults: %s", path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key;
        if (std::getline(iss, key, '=')) {
            std::string val;
            if (std::getline(iss, val)) {
                // Trim key and val
                key.erase(key.find_last_not_of(" \t") + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                val.erase(val.find_last_not_of(" \t") + 1);
                val.erase(0, val.find_first_not_of(" \t"));

                if (key == "enableDocking") {
                    enableDocking = (val == "true" || val == "1");
                } else if (key == "startMaximized") {
                    startMaximized = (val == "true" || val == "1");
                } else if (key == "assetRootPath") {
                    assetRootPath = val;
                } else if (key == "updateIntervalMs") {
                    updateIntervalMs = std::stof(val);
                } else if (key == "showStats") {
                    showStats = (val == "true" || val == "1");
                }
            }
        }
    }
    Core::Logger::Info("EditorConfig", "Loaded config successfully: %s", path.c_str());
    return true;
}

bool EditorConfig::Save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        Core::Logger::Error("EditorConfig", "Failed to open config file for saving: %s", path.c_str());
        return false;
    }

    file << "# Kumari Editor Configuration\n";
    file << "enableDocking=" << (enableDocking ? "true" : "false") << "\n";
    file << "startMaximized=" << (startMaximized ? "true" : "false") << "\n";
    file << "assetRootPath=" << assetRootPath << "\n";
    file << "updateIntervalMs=" << updateIntervalMs << "\n";
    file << "showStats=" << (showStats ? "true" : "false") << "\n";

    Core::Logger::Info("EditorConfig", "Saved config successfully: %s", path.c_str());
    return true;
}

} // namespace KumariEngine::Editor
