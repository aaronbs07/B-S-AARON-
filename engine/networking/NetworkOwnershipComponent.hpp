#pragma once
#include <cstdint>

namespace KumariEngine::Networking {

struct NetworkOwnershipComponent {
    uint32_t ownerPeerId = 0; // 0 represents server-owned / unowned; 1+ represents client owned
};

} // namespace KumariEngine::Networking
