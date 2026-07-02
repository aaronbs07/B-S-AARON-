#pragma once
#include "editor/window_system.hpp"
#include <string>

namespace KumariEngine::Editor {

struct TelemetryData {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t entityCount = 0;
    size_t drawCalls = 0;
    size_t triangles = 0;
    size_t luaScriptCount = 0;
    std::string networkStatus = "Inactive";
    float memoryUsageMB = 0.0f;
};

class StatisticsWindow : public EditorWindow {
public:
    StatisticsWindow() : EditorWindow("Statistics") {}

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    const TelemetryData& GetTelemetry() const { return m_telemetry; }

private:
    TelemetryData m_telemetry;
    float m_fpsAccumulator = 0.0f;
    int m_frameCount = 0;
};

} // namespace KumariEngine::Editor
