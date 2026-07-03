#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace KumariEngine::Editor {

struct WindowLayoutState {
    std::string title;
    bool isOpen = true;
    bool isDocked = true;
    float posX = 0.0f;
    float posY = 0.0f;
    float width = 300.0f;
    float height = 200.0f;
};

class DockingSystem {
public:
    static DockingSystem& Get() {
        static DockingSystem instance;
        return instance;
    }

    DockingSystem(const DockingSystem&) = delete;
    DockingSystem& operator=(const DockingSystem&) = delete;

    void SaveLayout(const std::string& filepath);
    void LoadLayout(const std::string& filepath);

    const std::unordered_map<std::string, WindowLayoutState>& GetLayouts() const { return m_layouts; }
    void UpdateLayout(const std::string& title, bool isOpen, bool isDocked, float x, float y, float w, float h);

private:
    DockingSystem() = default;
    ~DockingSystem() = default;

    std::unordered_map<std::string, WindowLayoutState> m_layouts;
};

} // namespace KumariEngine::Editor
