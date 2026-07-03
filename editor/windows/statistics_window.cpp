#include "statistics_window.hpp"
#include "terrain/terrain_manager.hpp"
#include "scripting/script_engine.hpp"
#include "networking/NetworkManager.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void StatisticsWindow::Initialize() {
    Core::Logger::Info("Editor", "Statistics Window Initialized");
}

void StatisticsWindow::Update(float deltaTime) {
    m_fpsAccumulator += deltaTime;
    m_frameCount++;

    if (m_fpsAccumulator >= 1.0f) {
        m_telemetry.fps = static_cast<float>(m_frameCount) / m_fpsAccumulator;
        m_telemetry.frameTimeMs = (m_fpsAccumulator / static_cast<float>(m_frameCount)) * 1000.0f;
        m_fpsAccumulator = 0.0f;
        m_frameCount = 0;
    }

    // 1. Entity Count
    auto* registry = WindowSystem::Get().GetRegistry();
    if (registry) {
        m_telemetry.entityCount = registry->GetAliveEntities().size();
    } else {
        m_telemetry.entityCount = 0;
    }

    // 2. Draw Calls and Triangles
    auto& tm = Terrain::TerrainManager::Get();
    m_telemetry.drawCalls = tm.GetDrawCallsCount() + 1; // Terrain draw calls + hardcoded screen-space triangle draw call
    m_telemetry.triangles = tm.GetRenderedTrianglesCount() + 1; // Terrain triangles + 1 screen-space triangle

    // 3. Lua Script Count
    auto& se = Scripting::ScriptEngine::Get();
    m_telemetry.luaScriptCount = se.GetLoadedScripts().size();

    // 4. Network Status
    auto& nm = Networking::NetworkManager::Get();
    switch (nm.GetState()) {
        case Networking::NetworkState::Inactive: m_telemetry.networkStatus = "Inactive"; break;
        case Networking::NetworkState::Server: m_telemetry.networkStatus = "Server"; break;
        case Networking::NetworkState::ClientConnecting: m_telemetry.networkStatus = "Client Connecting"; break;
        case Networking::NetworkState::ClientConnected: m_telemetry.networkStatus = "Client Connected"; break;
        case Networking::NetworkState::ClientFailedToConnect: m_telemetry.networkStatus = "Client Failed"; break;
        case Networking::NetworkState::ClientReconnecting: m_telemetry.networkStatus = "Client Reconnecting"; break;
        default: m_telemetry.networkStatus = "Unknown"; break;
    }

    // 5. Memory Usage Estimate (Terrain MB + Script KB -> MB)
    float terrainMemMB = static_cast<float>(tm.GetMemoryEstimate()) / (1024.0f * 1024.0f);
    float scriptMemMB = static_cast<float>(se.GetTotalMemoryAllocated()) / (1024.0f * 1024.0f);
    m_telemetry.memoryUsageMB = terrainMemMB + scriptMemMB;
}

void StatisticsWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Statistics Window] ===");
    Core::Logger::Info("EditorUI", "  FPS: %.2f | FrameTime: %.2f ms", m_telemetry.fps, m_telemetry.frameTimeMs);
    Core::Logger::Info("EditorUI", "  Entities: %d | Draw Calls: %d | Triangles: %d", m_telemetry.entityCount, m_telemetry.drawCalls, m_telemetry.triangles);
    Core::Logger::Info("EditorUI", "  Lua Scripts: %d | Network Status: %s", m_telemetry.luaScriptCount, m_telemetry.networkStatus.c_str());
    Core::Logger::Info("EditorUI", "  Memory Usage Estimate: %.2f MB", m_telemetry.memoryUsageMB);
}

} // namespace KumariEngine::Editor
