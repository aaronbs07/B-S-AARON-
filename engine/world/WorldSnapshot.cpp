#include "WorldSnapshot.hpp"
#include "save/BinaryWriter.hpp"
#include "save/BinaryReader.hpp"

namespace KumariEngine::World {

void WorldSnapshot::CaptureFromWorld(const World& world) {
    player = world.player;
    camera = world.camera;
    metadata = world.metadata;
}

void WorldSnapshot::ApplyToWorld(World& world) const {
    world.player = player;
    world.camera = camera;
    world.metadata = metadata;
}

void WorldSnapshot::Serialize(Save::BinaryWriter& writer) const {
    writer.WriteBytes(&player.position, sizeof(player.position));
    writer.WriteBytes(&player.rotation, sizeof(player.rotation));
    writer.WriteBytes(&player.velocity, sizeof(player.velocity));

    writer.WriteBytes(&camera.position, sizeof(camera.position));
    writer.WriteBytes(&camera.rotation, sizeof(camera.rotation));
    writer.WriteFloat(camera.fov);

    writer.WriteString(metadata.worldName);
    writer.WriteDouble(metadata.playtime);
    writer.WriteUint32(metadata.worldSeed);
}

void WorldSnapshot::Deserialize(Save::BinaryReader& reader) {
    reader.ReadBytes(&player.position, sizeof(player.position));
    reader.ReadBytes(&player.rotation, sizeof(player.rotation));
    reader.ReadBytes(&player.velocity, sizeof(player.velocity));

    reader.ReadBytes(&camera.position, sizeof(camera.position));
    reader.ReadBytes(&camera.rotation, sizeof(camera.rotation));
    reader.ReadFloat(camera.fov);

    reader.ReadString(metadata.worldName);
    reader.ReadDouble(metadata.playtime);
    reader.ReadUint32(metadata.worldSeed);
}

} // namespace KumariEngine::World
