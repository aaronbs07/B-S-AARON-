#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Forward declarations
namespace KumariEngine::Save {
class BinaryWriter;
class BinaryReader;
}

namespace KumariEngine::World {

// Player state representation
struct PlayerState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
};

// Camera state representation
struct CameraState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float fov = 60.0f;
};

// World metadata representation
struct WorldMetadata {
    std::string worldName;
    double playtime = 0.0;
    uint32_t worldSeed = 0;
};

// Simple mock World structure representing runtime gameplay state
struct World {
    PlayerState player;
    CameraState camera;
    WorldMetadata metadata;
};

// Intermediate snapshot data container
class WorldSnapshot {
public:
    WorldSnapshot() = default;
    ~WorldSnapshot() = default;

    // Captured plain data fields
    PlayerState player;
    CameraState camera;
    WorldMetadata metadata;

    /**
     * @brief Extracts runtime state from the World structure and stores it in the snapshot.
     */
    void CaptureFromWorld(const World& world);

    /**
     * @brief Restores the snapshot data back into the runtime World structure.
     */
    void ApplyToWorld(World& world) const;

    /**
     * @brief Serializes the snapshot to binary format.
     */
    void Serialize(Save::BinaryWriter& writer) const;

    /**
     * @brief Deserializes the snapshot from binary format.
     */
    void Deserialize(Save::BinaryReader& reader);
};

} // namespace KumariEngine::World
