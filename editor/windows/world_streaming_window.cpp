#include "world_streaming_window.hpp"
#include "terrain/terrain_manager.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void WorldStreamingWindow::RenderUI() {
    auto& tm = Terrain::TerrainManager::Get();
    Core::Logger::Info("EditorUI", "--- World Streaming Visualization ---");
    Core::Logger::Info("EditorUI", "  Loaded Chunks: %zu", tm.GetLoadedChunkCount());
    Core::Logger::Info("EditorUI", "  Loading Queue Size: %zu", tm.GetLoadingQueueSize());
    Core::Logger::Info("EditorUI", "  Average Gen Time: %.2f ms", tm.GetAverageGenTime());
    Core::Logger::Info("EditorUI", "  Cache Hits: %zu, Cache Misses: %zu", tm.GetCacheHitCount(), tm.GetCacheMissCount());
    Core::Logger::Info("EditorUI", "  Memory Estimate: %.2f KB", static_cast<double>(tm.GetMemoryEstimate()) / 1024.0);
    Core::Logger::Info("EditorUI", "  Chunk Borders Visualizer: %s", tm.IsChunkBordersEnabled() ? "Enabled" : "Disabled");
}

void WorldStreamingWindow::ToggleChunkBorders() {
    Terrain::TerrainManager::Get().ToggleChunkBorders();
    Core::Logger::Info("WorldStreaming", "Toggled chunk borders visualizer.");
}

} // namespace KumariEngine::Editor
