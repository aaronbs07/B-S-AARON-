#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

namespace KumariEngine::Save {

template<typename T>
inline T SwapBytes(T val) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported swap size");
    
    if constexpr (sizeof(T) == 1) {
        return val;
    } else if constexpr (sizeof(T) == 2) {
        uint16_t u = 0;
        std::memcpy(&u, &val, 2);
        u = (u >> 8) | (u << 8);
        T res;
        std::memcpy(&res, &u, 2);
        return res;
    } else if constexpr (sizeof(T) == 4) {
        uint32_t u = 0;
        std::memcpy(&u, &val, 4);
        u = ((u >> 24) & 0x000000FF) |
            ((u >> 8)  & 0x0000FF00) |
            ((u << 8)  & 0x00FF0000) |
            ((u << 24) & 0xFF000000);
        T res;
        std::memcpy(&res, &u, 4);
        return res;
    } else if constexpr (sizeof(T) == 8) {
        uint64_t u = 0;
        std::memcpy(&u, &val, 8);
        u = ((u >> 56) & 0x00000000000000FFULL) |
            ((u >> 40) & 0x000000000000FF00ULL) |
            ((u >> 24) & 0x0000000000FF0000ULL) |
            ((u >> 8)  & 0x00000000FF000000ULL) |
            ((u << 8)  & 0x000000FF00000000ULL) |
            ((u << 24) & 0x0000FF0000000000ULL) |
            ((u << 40) & 0x00FF000000000000ULL) |
            ((u << 56) & 0xFF00000000000000ULL);
        T res;
        std::memcpy(&res, &u, 8);
        return res;
    }
}

template<typename T>
inline T NativeToLittle(T val) {
    if constexpr (std::endian::native == std::endian::big) {
        return SwapBytes(val);
    }
    return val;
}

template<typename T>
inline T LittleToNative(T val) {
    if constexpr (std::endian::native == std::endian::big) {
        return SwapBytes(val);
    }
    return val;
}

} // namespace KumariEngine::Save
