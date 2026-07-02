#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Core {

class Encryption {
public:
    static void Encrypt(std::vector<uint8_t>& data, const std::string& key) {
        if (key.empty()) return;
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= static_cast<uint8_t>(key[i % key.size()]);
        }
    }

    static void Decrypt(std::vector<uint8_t>& data, const std::string& key) {
        Encrypt(data, key); // XOR is symmetric
    }
};

} // namespace KumariEngine::Core
