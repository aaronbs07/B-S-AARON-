#pragma once
#include <string_view>

struct GLFWwindow;

namespace KumariEngine::Window {

class Window {
public:
    Window();
    ~Window();

    bool Initialize(std::string_view title, int width, int height);
    void Shutdown();

    bool ShouldClose() const;
    GLFWwindow* GetNativeWindow() const { return m_window; }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    bool WasResized() const { return m_resized; }
    void ResetResizeFlag() { m_resized = false; }

    void SetTitle(std::string_view title);

private:
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_resized = false;
};

} // namespace KumariEngine::Window
