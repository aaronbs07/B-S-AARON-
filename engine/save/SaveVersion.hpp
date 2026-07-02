#pragma once
#include <cstdint>
#include <array>

namespace KumariEngine::Save {

// 4-byte magic identifier for Kumari Kandam save files: "KMSV" (Kumari Save)
inline constexpr std::array<char, 4> SAVE_MAGIC = {'K', 'M', 'S', 'V'};

// Current save version of the binary serialization format
inline constexpr uint32_t CURRENT_SAVE_VERSION = 5;

} // namespace KumariEngine::Save
