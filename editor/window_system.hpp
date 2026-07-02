#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ecs/ecs.hpp"

namespace KumariEngine::Editor {

class EditorWindow {
public:
    EditorWindow(const std::string& title, bool defaultOpen = true)
        : m_title(title), m_isOpen(defaultOpen), m_isDocked(true) {}
    virtual ~EditorWindow() = default;

    virtual void Initialize() {}
    virtual void Shutdown() {}
    virtual void Update(float deltaTime) { (void)deltaTime; }
    virtual void RenderUI() {}

    const std::string& GetTitle() const { return m_title; }
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }
    bool IsDocked() const { return m_isDocked; }
    void SetDocked(bool docked) { m_isDocked = docked; }

protected:
    std::string m_title;
    bool m_isOpen = true;
    bool m_isDocked = true;
};

class WindowSystem {
public:
    static WindowSystem& Get() {
        static WindowSystem instance;
        return instance;
    }

    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();
    void Update(float deltaTime);
    void RenderUI();

    void AddWindow(std::shared_ptr<EditorWindow> window);
    std::shared_ptr<EditorWindow> GetWindow(const std::string& title) const;
    const std::vector<std::shared_ptr<EditorWindow>>& GetWindows() const { return m_windows; }

    ECS::Registry* GetRegistry() const { return m_registry; }

private:
    WindowSystem() = default;
    ~WindowSystem() = default;

    std::vector<std::shared_ptr<EditorWindow>> m_windows;
    ECS::Registry* m_registry = nullptr;
};

} // namespace KumariEngine::Editor
