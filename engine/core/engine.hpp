#pragma once
#include <memory>
#include <string_view>
#include <functional>

namespace KumariEngine {
namespace Window { class Window; }
namespace Input { class Input; }
namespace Renderer { class Renderer; }
namespace ECS { class Registry; }
namespace Physics { class PhysicsSystem; }
}

namespace KumariEngine::Core {

class DebugOverlay;

class Engine {
public:
    Engine();
    ~Engine();

    // Prevent copying
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool Initialize(std::string_view windowTitle, int width, int height, int argc = 0, char** argv = nullptr);
    void Run();
    void Shutdown();

    bool IsRunning() const { return m_running; }

    using FrameCallback = std::function<void(float)>;
    void SetUpdateCallback(FrameCallback callback) { m_updateCallback = callback; }
    void SetRenderCallback(std::function<void()> callback) { m_renderCallback = callback; }
    ECS::Registry* GetRegistry() const { return m_registry.get(); }

private:
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    std::unique_ptr<Window::Window> m_window;
    std::unique_ptr<Input::Input> m_input;
    std::unique_ptr<Renderer::Renderer> m_renderer;
    std::unique_ptr<DebugOverlay> m_debugOverlay;
    
    // ECS and Physics System
    std::unique_ptr<ECS::Registry> m_registry;
    std::unique_ptr<Physics::PhysicsSystem> m_physicsSystem;
    float m_physicsAccumulator = 0.0f;

    bool m_running = false;
    float m_lastFrameTime = 0.0f;

    FrameCallback m_updateCallback = nullptr;
    std::function<void()> m_renderCallback = nullptr;
};

} // namespace KumariEngine::Core
