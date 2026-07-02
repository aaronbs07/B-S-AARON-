#pragma once
#include <string>

namespace KumariEngine::ECS {

struct ScriptComponent {
    std::string scriptPath;
    int envRef = -1;
    bool initialized = false;

    ScriptComponent() = default;
    explicit ScriptComponent(const std::string& path) : scriptPath(path), envRef(-1), initialized(false) {}

    ~ScriptComponent();

    ScriptComponent(ScriptComponent&& other) noexcept;
    ScriptComponent& operator=(ScriptComponent&& other) noexcept;

    ScriptComponent(const ScriptComponent&) = delete;
    ScriptComponent& operator=(const ScriptComponent&) = delete;

private:
    void Release();
};

} // namespace KumariEngine::ECS
