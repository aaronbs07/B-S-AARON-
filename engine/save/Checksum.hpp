#pragma once
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Save {

class Checksum {
public:
    static uint32_t Calculate(const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < size; ++i) {
            uint8_t byte = bytes[i];
            crc ^= byte;
            for (int j = 0; j < 8; ++j) {
                uint32_t mask = (crc & 1) ? 0xFFFFFFFF : 0;
                crc = (crc >> 1) ^ (0xEDB88320 & mask);
            }
        }
        return ~crc;
    }
};

} // namespace KumariEngine::Save
