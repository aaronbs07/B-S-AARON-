#pragma once

namespace KumariEngine::Window { class Window; }

namespace KumariEngine::Renderer {

/// <summary>
/// Abstract base class for the rendering engine.
/// Allows backend modularity (Vulkan, DX12, OpenGL, etc.).
/// </summary>
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool Initialize(Window::Window* window) = 0;
    virtual void Shutdown() = 0;

    virtual void BeginFrame() = 0;
    virtual void DrawFrame() = 0;
    virtual void EndFrame() = 0;
};

} // namespace KumariEngine::Renderer
