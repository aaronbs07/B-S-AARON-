#include "debug_overlay.hpp"
#include "window/window.hpp"
#include "terrain/terrain_manager.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "core/logger.hpp"
#include "scripting/script_engine.hpp"
#include "networking/NetworkManager.hpp"
#include "networking/ReplicationManager.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace KumariEngine::Core {

void DebugOverlay::Update(float deltaTime, Window::Window* window) {
    m_accumulatedTime += deltaTime;
    m_frameCount++;
    m_dashboardTimer += deltaTime;

    if (m_accumulatedTime >= 1.0f) {
        float fps = static_cast<float>(m_frameCount) / m_accumulatedTime;
        float frameTime = (m_accumulatedTime / static_cast<float>(m_frameCount)) * 1000.0f;

        auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
        glm::vec3 camPos(0.0f);
        int currentChunkX = 0;
        int currentChunkZ = 0;
        int currentLOD = 0;

        auto& tm = Terrain::TerrainManager::Get();
        float chunkSize = tm.GetChunkSize();

        if (activeCam) {
            camPos = activeCam->GetCurrentPosition();
            currentChunkX = static_cast<int>(std::floor(camPos.x / chunkSize));
            currentChunkZ = static_cast<int>(std::floor(camPos.z / chunkSize));

            const auto& activeChunks = tm.GetActiveChunks();
            auto it = activeChunks.find(Terrain::ChunkCoord{currentChunkX, currentChunkZ});
            if (it != activeChunks.end()) {
                currentLOD = it->second->GetLOD();
            }
        }

        size_t loadedChunks = tm.GetLoadedChunkCount();
        size_t queueSize = tm.GetLoadingQueueSize();
        float avgGenTime = tm.GetAverageGenTime();
        size_t memEstimate = tm.GetMemoryEstimate();
        float memMB = static_cast<float>(memEstimate) / (1024.0f * 1024.0f);

        size_t scriptCount = Scripting::ScriptEngine::Get().GetLoadedScripts().size();
        float scriptMemKB = static_cast<float>(Scripting::ScriptEngine::Get().GetTotalMemoryAllocated()) / 1024.0f;

        // Fetch network stats
        auto& nm = Networking::NetworkManager::Get();
        auto& rm = Networking::ReplicationManager::Get();
        std::string netStateStr = "Inactive";
        if (nm.GetState() == Networking::NetworkState::Server) netStateStr = "Server";
        else if (nm.GetState() == Networking::NetworkState::ClientConnecting) netStateStr = "Connecting";
        else if (nm.GetState() == Networking::NetworkState::ClientConnected) netStateStr = "Client";
        else if (nm.GetState() == Networking::NetworkState::ClientFailedToConnect) netStateStr = "Failed";
        else if (nm.GetState() == Networking::NetworkState::ClientReconnecting) netStateStr = "Reconnecting";

        // Format window title string
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1)
           << "Kumari Kandam Engine | FPS: " << fps << " (" << frameTime << "ms) | "
           << "Net: " << netStateStr << " (" << nm.GetConnectedPeerCount() << " peers, "
           << (nm.GetBytesSentRate() / 1024.0) << "/" << (nm.GetBytesReceivedRate() / 1024.0) << " KB/s) | "
           << "Repl: " << rm.GetLastReplicationTimeMs() << "ms | "
           << "Cam: (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ") | "
           << "Chunk: (" << currentChunkX << ", " << currentChunkZ << ") | "
           << "LOD: " << currentLOD << " | "
           << "Chunks: " << loadedChunks << " | "
           << "Queue: " << queueSize << " | "
           << "Stream: " << tm.GetLastStreamingTimeMs() << "ms | "
           << "Upload: " << tm.GetLastGPUUploadTimeMs() << "ms | "
           << "Stitch (R/U): " << tm.GetStitchingRebuildCount() << "/" << tm.GetStitchingUploadCount() << " | "
           << "Memory: " << memMB << "MB | "
           << "Scripts: " << scriptCount << " (" << std::fixed << std::setprecision(1) << scriptMemKB << "KB) | "
           << "Wireframe: " << (tm.IsWireframe() ? "ON" : "OFF");

        if (window) {
            window->SetTitle(ss.str());
        }

        // Print telemetry summary dashboard every 5 seconds
        if (m_dashboardTimer >= 5.0f) {
            Logger::Info("Telemetry", "=== TELEMETRY SUMMARY ===");
            Logger::Info("Telemetry", "FPS: %.1f | Frame Time: %.2f ms | Memory Estimate: %.2f MB", fps, frameTime, memMB);
            Logger::Info("Telemetry", "Camera Pos: [%.1f, %.1f, %.1f] | Current Chunk: (%d, %d)", camPos.x, camPos.y, camPos.z, currentChunkX, currentChunkZ);
            Logger::Info("Telemetry", "Active Chunks: %zu | Background Generation Queue: %zu | Avg Gen Time: %.1f ms", loadedChunks, queueSize, avgGenTime);
            Logger::Info("Telemetry", "Streaming Time: %.2f ms | GPU Upload Time: %.2f ms", tm.GetLastStreamingTimeMs(), tm.GetLastGPUUploadTimeMs());
            Logger::Info("Telemetry", "Stitching Rebuilds: %zu | Stitching Uploads: %zu", tm.GetStitchingRebuildCount(), tm.GetStitchingUploadCount());
            
            // Add Network Telemetry
            Logger::Info("Telemetry", "Net State: %s | Peers: %zu | Tx Rate: %.1f KB/s (%.1f pkts/s) | Rx Rate: %.1f KB/s (%.1f pkts/s)",
                         netStateStr.c_str(), nm.GetConnectedPeerCount(),
                         nm.GetBytesSentRate() / 1024.0, nm.GetPacketsSentRate(),
                         nm.GetBytesReceivedRate() / 1024.0, nm.GetPacketsReceivedRate());
            Logger::Info("Telemetry", "Repl Tick: %.2f ms | Serialization: %.2f ms | Deserialization: %.2f ms",
                         rm.GetLastReplicationTimeMs(), rm.GetLastSerializationTimeMs(), rm.GetLastDeserializationTimeMs());

            Logger::Info("Telemetry", "Toggles: Wireframe: %s | Borders: %s | Debug LOD: %s",
                         tm.IsWireframe() ? "ON" : "OFF",
                         tm.IsChunkBordersEnabled() ? "ON" : "OFF",
                         tm.IsDebugVisEnabled() ? "ON" : "OFF");
            
            std::string scriptStats = Scripting::ScriptEngine::Get().GetActiveScriptStats();
            std::string profilerInfo = Scripting::ScriptEngine::Get().DumpProfilerInfo();
            Logger::Info("Telemetry", "\n%s\n%s", scriptStats.c_str(), profilerInfo.c_str());
            
            Logger::Info("Telemetry", "=========================");
            m_dashboardTimer = 0.0f;
        }

        m_accumulatedTime = 0.0f;
        m_frameCount = 0;
    }
}

} // namespace KumariEngine::Core
