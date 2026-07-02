#pragma once
#include <memory>
#include <string_view>
#include "editor_config.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Editor {

class Editor {
public:
    Editor();
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    bool Initialize(ECS::Registry* registry);
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    const EditorConfig& GetConfig() const { return m_config; }

    // Scene dirty tracking & Save integration
    bool IsDirty() const;
    void SetDirty(bool dirty = true);

    bool SaveScene(const std::string& filepath);
    bool SaveScene(); 
    bool SaveAs(const std::string& filepath);
    bool OpenScene(const std::string& filepath);
    void NewScene();

    const std::string& GetCurrentScenePath() const { return m_currentScenePath; }
    bool RequestClose();

private:
    EditorConfig m_config;
    ECS::Registry* m_registry = nullptr;
    bool m_initialized = false;
    bool m_isDirty = false;
    std::string m_currentScenePath;
};

} // namespace KumariEngine::Editor
