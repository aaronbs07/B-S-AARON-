#include "window_system.hpp"

namespace KumariEngine::Editor {

void WindowSystem::Initialize(ECS::Registry* registry) {
    if (m_initialized) {
        // Already initialized – do not re-initialize or call Initialize() on
        // windows a second time, which would leak or reset their state.
        return;
    }
    m_registry = registry;
    for (auto& window : m_windows) {
        if (window) {
            window->Initialize();
        }
    }
    m_initialized = true;
}

void WindowSystem::Shutdown() {
    if (!m_initialized) return;
    for (auto& window : m_windows) {
        if (window) {
            window->Shutdown();
        }
    }
    m_windows.clear();
    m_registry = nullptr;
    m_initialized = false;
}

void WindowSystem::Update(float deltaTime) {
    for (auto& window : m_windows) {
        if (window && window->IsOpen()) {
            window->Update(deltaTime);
        }
    }
}

void WindowSystem::RenderUI() {
    for (auto& window : m_windows) {
        if (window && window->IsOpen()) {
            window->RenderUI();
        }
    }
}

void WindowSystem::AddWindow(std::shared_ptr<EditorWindow> window) {
    m_windows.push_back(window);
    if (m_registry && window) {
        window->Initialize();
    }
}

std::shared_ptr<EditorWindow> WindowSystem::GetWindow(const std::string& title) const {
    for (auto& window : m_windows) {
        if (window && window->GetTitle() == title) {
            return window;
        }
    }
    return nullptr;
}

} // namespace KumariEngine::Editor
