#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace KumariEngine::Networking {

struct InputFrame {
    uint32_t sequenceNumber = 0;
    uint64_t timestamp = 0; // Client-side timestamp in milliseconds
    glm::vec3 moveDirection = glm::vec3(0.0f);
    float verticalVelocity = 0.0f;
    bool requestJump = false;
    uint32_t actionButtonsBitmask = 0; // Bitmask for action buttons (e.g. interaction, script-actions)
    float deltaTime = 0.016f; // Delta time of this frame
};

struct InputBufferComponent {
    std::vector<InputFrame> buffer;
    size_t maxBufferSize = 60; // Keep up to 60 frames (1 second at 60Hz)

    void AddInputFrame(const InputFrame& frame) {
        buffer.push_back(frame);
        if (buffer.size() > maxBufferSize) {
            buffer.erase(buffer.begin());
        }
    }
};

} // namespace KumariEngine::Networking
