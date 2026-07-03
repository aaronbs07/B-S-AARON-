#include "project_window.hpp"
#include "core/logger.hpp"
#include <filesystem>

namespace KumariEngine::Editor {

void ProjectWindow::Initialize() {
    Core::Logger::Info("Editor", "Project Window Initialized.");
    m_recentScenes = { "game/assets/scenes/main.prefab" };
    RefreshAssets();
}

void ProjectWindow::Shutdown() {}

void ProjectWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void ProjectWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Project Window] ===");
    Core::Logger::Info("EditorUI", "  Asset Database Summary:");
    Core::Logger::Info("EditorUI", "    Scenes: %d | Scripts: %d", m_sceneCount, m_scriptCount);
    Core::Logger::Info("EditorUI", "    Meshes: %d | Textures: %d", m_meshCount, m_textureCount);
    Core::Logger::Info("EditorUI", "  Recent Scenes:");
    for (const auto& path : m_recentScenes) {
        Core::Logger::Info("EditorUI", "    - %s", path.c_str());
    }
}

void ProjectWindow::RefreshAssets() {
    m_meshCount = 0;
    m_textureCount = 0;
    m_scriptCount = 0;
    m_sceneCount = 0;

    std::string root = "game/assets";
    if (std::filesystem::exists(root)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                // Normalize extension to lowercase
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (ext == ".mesh") m_meshCount++;
                else if (ext == ".png" || ext == ".dds" || ext == ".jpg") m_textureCount++;
                else if (ext == ".lua") m_scriptCount++;
                else if (ext == ".scene" || ext == ".prefab") m_sceneCount++;
            }
        }
    }
}

} // namespace KumariEngine::Editor
