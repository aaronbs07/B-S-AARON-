#include "input.hpp"
#include <cstring>
#include <GLFW/glfw3.h>

namespace KumariEngine::Input {

Input::Input() {
    std::memset(m_keys, 0, sizeof(m_keys));
    std::memset(m_keysPrev, 0, sizeof(m_keysPrev));
    std::memset(m_mouseButtons, 0, sizeof(m_mouseButtons));
    std::memset(m_mouseButtonsPrev, 0, sizeof(m_mouseButtonsPrev));
}

Input::~Input() = default;

void Input::Initialize(GLFWwindow* window) {
    m_window = window;
}

void Input::Update() {
    if (!m_window) return;

    // Buffer previous state for edge detection (press/release)
    std::memcpy(m_keysPrev, m_keys, sizeof(m_keys));
    std::memcpy(m_mouseButtonsPrev, m_mouseButtons, sizeof(m_mouseButtons));

    // Poll standard keyboard range
    for (int i = 32; i < 348; ++i) {
        m_keys[i] = (glfwGetKey(m_window, i) == GLFW_PRESS);
    }

    // Poll mouse buttons
    for (int i = 0; i < 8; ++i) {
        m_mouseButtons[i] = (glfwGetMouseButton(m_window, i) == GLFW_PRESS);
    }

    // Poll mouse position
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);

    if (m_firstMouseInput) {
        m_mouseX = xpos;
        m_mouseY = ypos;
        m_firstMouseInput = false;
    }

    m_mouseDeltaX = xpos - m_mouseX;
    m_mouseDeltaY = ypos - m_mouseY;

    m_mouseX = xpos;
    m_mouseY = ypos;
}

bool Input::IsKeyPressed(int key) const {
    if (key < 0 || key >= 512) return false;
    return m_keys[key] && !m_keysPrev[key];
}

bool Input::IsKeyReleased(int key) const {
    if (key < 0 || key >= 512) return false;
    return !m_keys[key] && m_keysPrev[key];
}

bool Input::IsKeyDown(int key) const {
    if (key < 0 || key >= 512) return false;
    return m_keys[key];
}

bool Input::IsMouseButtonPressed(int button) const {
    if (button < 0 || button >= 8) return false;
    return m_mouseButtons[button] && !m_mouseButtonsPrev[button];
}

bool Input::IsMouseButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return m_mouseButtons[button];
}

} // namespace KumariEngine::Input
