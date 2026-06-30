#pragma once

struct GLFWwindow;

namespace KumariEngine::Input {

class Input {
public:
    Input();
    ~Input();

    void Initialize(GLFWwindow* window);
    void Update();

    bool IsKeyPressed(int key) const;
    bool IsKeyReleased(int key) const;
    bool IsKeyDown(int key) const;

    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonDown(int button) const;

    void GetMousePosition(double& x, double& y) const { x = m_mouseX; y = m_mouseY; }
    void GetMouseDelta(double& dx, double& dy) const { dx = m_mouseDeltaX; dy = m_mouseDeltaY; }

private:
    GLFWwindow* m_window = nullptr;

    bool m_keys[512];
    bool m_keysPrev[512];

    bool m_mouseButtons[8];
    bool m_mouseButtonsPrev[8];

    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
    double m_mouseDeltaX = 0.0;
    double m_mouseDeltaY = 0.0;
    bool m_firstMouseInput = true;
};

} // namespace KumariEngine::Input
