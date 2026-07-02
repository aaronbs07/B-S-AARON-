#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

namespace KumariEngine::Core {

class Compression {
public:
    static std::vector<uint8_t> Compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        output.reserve(input.size());
        size_t i = 0;
        while (i < input.size()) {
            uint8_t b = input[i];
            size_t run = 1;
            while (i + run < input.size() && input[i + run] == b && run < 255) {
                run++;
            }
            if (run >= 3) {
                output.push_back(0xFF);
                output.push_back(static_cast<uint8_t>(run));
                output.push_back(b);
                i += run;
            } else {
                if (b == 0xFF) {
                    output.push_back(0xFF);
                    output.push_back(0x00);
                } else {
                    output.push_back(b);
                }
                i++;
            }
        }
        return output;
    }

    static std::vector<uint8_t> Decompress(const std::vector<uint8_t>& input, size_t expectedUncompressedSize) {
        std::vector<uint8_t> output;
        output.reserve(expectedUncompressedSize);
        size_t i = 0;
        while (i < input.size()) {
            uint8_t b = input[i];
            if (b == 0xFF) {
                if (i + 1 >= input.size()) break;
                uint8_t code = input[i + 1];
                if (code == 0) {
                    output.push_back(0xFF);
                    i += 2;
                } else {
                    if (i + 2 >= input.size()) break;
                    uint8_t val = input[i + 2];
                    output.insert(output.end(), code, val);
                    i += 3;
                }
            } else {
                output.push_back(b);
                i++;
            }
        }
        return output;
    }
};

} // namespace KumariEngine::Core
