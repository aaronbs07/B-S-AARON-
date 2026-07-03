#include "docking_system.hpp"
#include <fstream>
#include <sstream>
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void DockingSystem::SaveLayout(const std::string& filepath) {
    std::ofstream file(filepath, std::ios::app); // Append or write separately?
    // Let's write layout properties in a dedicated section or format
    if (!file.is_open()) {
        Core::Logger::Error("DockingSystem", "Failed to open config file for saving layout: %s", filepath.c_str());
        return;
    }

    file << "\n# Docking System Window Layouts\n";
    for (const auto& [title, layout] : m_layouts) {
        file << "layout_" << title << "_isOpen=" << (layout.isOpen ? "true" : "false") << "\n";
        file << "layout_" << title << "_isDocked=" << (layout.isDocked ? "true" : "false") << "\n";
        file << "layout_" << title << "_posX=" << layout.posX << "\n";
        file << "layout_" << title << "_posY=" << layout.posY << "\n";
        file << "layout_" << title << "_width=" << layout.width << "\n";
        file << "layout_" << title << "_height=" << layout.height << "\n";
    }
    Core::Logger::Info("DockingSystem", "Saved docking layouts to %s", filepath.c_str());
}

void DockingSystem::LoadLayout(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Core::Logger::Warning("DockingSystem", "Failed to open config file for loading layouts: %s", filepath.c_str());
        return;
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

                if (key.rfind("layout_", 0) == 0) {
                    // Extract window name and field
                    // Format: layout_Title_field
                    size_t firstUnderscore = 7; // after "layout_"
                    size_t lastUnderscore = key.find_last_of('_');
                    if (lastUnderscore != std::string::npos && lastUnderscore > firstUnderscore) {
                        std::string title = key.substr(firstUnderscore, lastUnderscore - firstUnderscore);
                        std::string field = key.substr(lastUnderscore + 1);

                        auto& layout = m_layouts[title];
                        layout.title = title;

                        if (field == "isOpen") {
                            layout.isOpen = (val == "true" || val == "1");
                        } else if (field == "isDocked") {
                            layout.isDocked = (val == "true" || val == "1");
                        } else if (field == "posX") {
                            layout.posX = std::stof(val);
                        } else if (field == "posY") {
                            layout.posY = std::stof(val);
                        } else if (field == "width") {
                            layout.width = std::stof(val);
                        } else if (field == "height") {
                            layout.height = std::stof(val);
                        }
                    }
                }
            }
        }
    }
    Core::Logger::Info("DockingSystem", "Loaded layouts successfully: %s", filepath.c_str());
}

void DockingSystem::UpdateLayout(const std::string& title, bool isOpen, bool isDocked, float x, float y, float w, float h) {
    auto& layout = m_layouts[title];
    layout.title = title;
    layout.isOpen = isOpen;
    layout.isDocked = isDocked;
    layout.posX = x;
    layout.posY = y;
    layout.width = w;
    layout.height = h;
}

} // namespace KumariEngine::Editor
