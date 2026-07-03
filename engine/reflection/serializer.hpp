#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <cstdint>
#include "reflection/type_info.hpp"
#include "reflection/property.hpp"
#include "reflection/type_registry.hpp"
#include "save/BinaryWriter.hpp"
#include "ecs/ecs.hpp"

// Forward declarations
namespace KumariEngine::Scene { class SceneManager; class SceneNode; }

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// Serialization format selector
// ---------------------------------------------------------------------------
enum class SerializeFormat : uint8_t {
    JSON   = 0,
    Binary = 1,
};

// ---------------------------------------------------------------------------
// Binary file header for reflection-serialized files
// ---------------------------------------------------------------------------
struct ReflectionFileHeader {
    char     magic[4]  = {'K','M','R','F'};   // "KMRF"
    uint32_t version   = 1;
    uint32_t typeId_hi = 0;
    uint32_t typeId_lo = 0;
    uint32_t payloadSize = 0;
};

// ---------------------------------------------------------------------------
// Serializer — converts reflected structs to JSON or binary
// ---------------------------------------------------------------------------
class Serializer {
public:
    // ------------------------------------------------------------------
    // Primitive struct serialization
    // ------------------------------------------------------------------

    /// Serialize a reflected struct to a JSON string
    static std::string SerializeToJSON(const void* data, const TypeInfo& type);

    /// Serialize a reflected struct to a JSON file (UTF-8)
    static bool SerializeToJSONFile(const void* data, const TypeInfo& type,
                                     std::string_view path);

    /// Serialize a reflected struct to binary via BinaryWriter
    static void SerializeToBinary(const void* data, const TypeInfo& type,
                                   Save::BinaryWriter& writer);

    /// Serialize to binary blob (returns byte vector)
    static std::vector<uint8_t> SerializeToBinaryBlob(const void* data,
                                                        const TypeInfo& type);

    // ------------------------------------------------------------------
    // ECS Entity
    // ------------------------------------------------------------------
    static std::string SerializeEntity(ECS::Entity entity,
                                        ECS::Registry* registry,
                                        SerializeFormat fmt = SerializeFormat::JSON);

    // ------------------------------------------------------------------
    // Scene
    // ------------------------------------------------------------------
    static std::string SerializeScene(Scene::SceneManager* scene,
                                       ECS::Registry* registry,
                                       SerializeFormat fmt = SerializeFormat::JSON);

    static bool SerializeSceneToFile(Scene::SceneManager* scene,
                                      ECS::Registry* registry,
                                      std::string_view path,
                                      SerializeFormat fmt = SerializeFormat::JSON);

    // ------------------------------------------------------------------
    // Prefab (entity subtree)
    // ------------------------------------------------------------------
    static std::string SerializePrefab(ECS::Entity rootEntity,
                                        ECS::Registry* registry,
                                        Scene::SceneManager* scene,
                                        SerializeFormat fmt = SerializeFormat::JSON);

    static bool SerializePrefabToFile(ECS::Entity rootEntity,
                                       ECS::Registry* registry,
                                       Scene::SceneManager* scene,
                                       std::string_view path,
                                       SerializeFormat fmt = SerializeFormat::JSON);

private:
    // ------------------------------------------------------------------
    // Internal JSON helpers
    // ------------------------------------------------------------------
    static void WriteJSONValue(std::ostringstream& ss,
                                const void* base,
                                const PropertyDescriptor& prop,
                                int indent);

    static void WriteJSONStruct(std::ostringstream& ss,
                                 const void* data,
                                 const TypeInfo& type,
                                 int indent);

    static void WriteJSONIndent(std::ostringstream& ss, int indent);

    // ------------------------------------------------------------------
    // Internal Binary helpers
    // ------------------------------------------------------------------
    static void WriteBinaryValue(Save::BinaryWriter& writer,
                                  const void* base,
                                  const PropertyDescriptor& prop);

    // ------------------------------------------------------------------
    // Entity/Scene/Prefab JSON helpers
    // ------------------------------------------------------------------
    static std::string SerializeEntityJSON(ECS::Entity entity,
                                            ECS::Registry* registry);

    static void WriteSceneNodeJSON(std::ostringstream& ss,
                                    Scene::SceneNode* node,
                                    ECS::Registry* registry,
                                    int indent);
};

} // namespace KumariEngine::Reflection
