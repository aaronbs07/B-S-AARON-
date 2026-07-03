#pragma once
#include <string>
#include <chrono>

namespace KumariEngine::HotReload {

enum class ReloadEventType {
    Created,
    Modified,
    Deleted,
    Renamed
};

struct ReloadEvent {
    std::string path;
    std::string oldPath; // Only used for Renamed event
    ReloadEventType type;
    std::chrono::system_clock::time_point timestamp;
    std::string assetType; // e.g. "Shader", "Texture", "Material", "Script", "Unknown"
};

} // namespace KumariEngine::HotReload
