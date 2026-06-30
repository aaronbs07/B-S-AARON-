#include "window.hpp"
#include "core/logger.hpp"
#include <GLFW/glfw3.h>
#include <string>

namespace KumariEngine::Window {

Window::Window() = default;
Window::~Window() {
    Shutdown();
}

bool Window::Initialize(std::string_view title, int width, int height) {
    m_width = width;
    m_height = height;

    Core::Logger::Info("Window", "Initializing GLFW...");
    if (!glfwInit()) {
        Core::Logger::Error("Window", "Failed to initialize GLFW.");
        return false;
    }

    // Configure GLFW for Vulkan (Disable OpenGL API context creation)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    Core::Logger::Info("Window", "Creating window client...");
    m_window = glfwCreateWindow(m_width, m_height, title.data(), nullptr, nullptr);
    if (!m_window) {
        Core::Logger::Error("Window", "Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }

    // Store this pointer for callback references
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

    Core::Logger::Info("Window", "Window client created successfully.");
    return true;
}

void Window::FramebufferResizeCallback(GLFWwindow* glfwWindow, int width, int height) {
    auto appWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    if (appWindow) {
        appWindow->m_width = width;
        appWindow->m_height = height;
        appWindow->m_resized = true;
        Core::Logger::Info("Window", "Window resized to %dx%d.", width, height);
    }
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::Shutdown() {
    if (m_window) {
        Core::Logger::Info("Window", "Destroying window surface...");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    Core::Logger::Info("Window", "Terminating GLFW runtime...");
    glfwTerminate();
}

void Window::SetTitle(std::string_view title) {
    if (m_window) {
        std::string temp(title);
        glfwSetWindowTitle(m_window, temp.c_str());
    }
}

} // namespace KumariEngine::Window
