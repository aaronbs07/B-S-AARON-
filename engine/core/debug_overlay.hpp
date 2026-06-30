#pragma once
#include <string>

namespace KumariEngine::Window { class Window; }

namespace KumariEngine::Core {

class DebugOverlay {
public:
    DebugOverlay() = default;

    // Tick the overlay system, recalculating stats and updating window title / logs
    void Update(float deltaTime, Window::Window* window);

private:
    float m_accumulatedTime = 0.0f;
    int m_frameCount = 0;
    float m_dashboardTimer = 0.0f;
};

} // namespace KumariEngine::Core
