#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>

namespace KumariEngine::Save {

struct EntityGUID {
    uint64_t high = 0;
    uint64_t low = 0;

    bool operator==(const EntityGUID& other) const {
        return high == other.high && low == other.low;
    }

    bool operator!=(const EntityGUID& other) const {
        return !(*this == other);
    }

    bool operator<(const EntityGUID& other) const {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }

    bool IsNull() const {
        return high == 0 && low == 0;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') 
           << std::setw(16) << high 
           << std::setw(16) << low;
        return ss.str();
    }
};

inline const EntityGUID NULL_GUID = { 0, 0 };

class GUIDGenerator {
public:
    static EntityGUID Generate() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        EntityGUID guid;
        do {
            guid.high = gen();
            guid.low = gen();
        } while (guid.IsNull());
        return guid;
    }

    static EntityGUID GenerateDeterministic(uint32_t seed, uint32_t sequence) {
        EntityGUID guid;
        guid.high = seed;
        guid.low = sequence;
        if (guid.IsNull()) {
            guid.low = 1; // prevent Null
        }
        return guid;
    }
};

} // namespace KumariEngine::Save

// Provide std::hash specialization for EntityGUID so it can be used in std::unordered_map
namespace std {
    template<>
    struct hash<KumariEngine::Save::EntityGUID> {
        size_t operator()(const KumariEngine::Save::EntityGUID& g) const {
            return (static_cast<size_t>(g.high) * 397) ^ static_cast<size_t>(g.low);
        }
    };
}
