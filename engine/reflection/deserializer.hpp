#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include "reflection/type_info.hpp"
#include "reflection/property.hpp"
#include "reflection/type_registry.hpp"
#include "save/BinaryReader.hpp"
#include "ecs/ecs.hpp"

// Forward declarations
namespace KumariEngine::Scene { class SceneManager; class SceneNode; }

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// DeserializeResult — error/warning reporting from deserialization
// ---------------------------------------------------------------------------
struct DeserializeResult {
    bool        success = true;
    std::string error;
    int         unknownPropertiesSkipped = 0;
    int         missingPropertiesDefaulted = 0;
    uint32_t    formatVersion = 0;

    explicit operator bool() const { return success; }
};

// ---------------------------------------------------------------------------
// Deserializer — restores reflected structs from JSON or binary
// ---------------------------------------------------------------------------
class Deserializer {
public:
    // ------------------------------------------------------------------
    // JSON deserialization
    // ------------------------------------------------------------------

    /// Deserialize a JSON string into a pre-allocated struct buffer
    static DeserializeResult DeserializeFromJSON(void* data,
                                                   const TypeInfo& type,
                                                   std::string_view json);

    /// Deserialize a JSON file
    static DeserializeResult DeserializeFromJSONFile(void* data,
                                                       const TypeInfo& type,
                                                       std::string_view path);

    // ------------------------------------------------------------------
    // Binary deserialization
    // ------------------------------------------------------------------

    /// Deserialize binary data from a BinaryReader
    static DeserializeResult DeserializeFromBinary(void* data,
                                                     const TypeInfo& type,
                                                     Save::BinaryReader& reader);

    /// Deserialize from a raw binary blob
    static DeserializeResult DeserializeFromBinaryBlob(void* data,
                                                         const TypeInfo& type,
                                                         const std::vector<uint8_t>& blob);

    // ------------------------------------------------------------------
    // Scene deserialization
    // ------------------------------------------------------------------
    static DeserializeResult DeserializeScene(Scene::SceneManager* scene,
                                               ECS::Registry* registry,
                                               std::string_view json);

    static DeserializeResult DeserializeSceneFromFile(Scene::SceneManager* scene,
                                                        ECS::Registry* registry,
                                                        std::string_view path);

    // ------------------------------------------------------------------
    // Prefab deserialization
    // ------------------------------------------------------------------
    static DeserializeResult DeserializePrefabFromFile(ECS::Registry* registry,
                                                         Scene::SceneManager* scene,
                                                         std::string_view path);

private:
    // ------------------------------------------------------------------
    // Lightweight JSON parser state
    // ------------------------------------------------------------------
    struct JSONParser {
        std::string_view src;
        size_t pos = 0;

        void SkipWhitespace();
        bool AtEnd() const { return pos >= src.size(); }
        char Peek() const { return AtEnd() ? '\0' : src[pos]; }
        char Consume() { return AtEnd() ? '\0' : src[pos++]; }
        bool Match(char c);
        bool Expect(char c, DeserializeResult& res);

        std::string ParseString(DeserializeResult& res);
        double      ParseNumber(DeserializeResult& res);
        bool        ParseBool(DeserializeResult& res);
        bool        ParseNull();

        // Skip over any JSON value (unknown property handler)
        void SkipValue(DeserializeResult& res);
        void SkipObject(DeserializeResult& res);
        void SkipArray(DeserializeResult& res);

        bool ParseObject(
            std::function<void(std::string_view key, DeserializeResult&)> fieldHandler,
            DeserializeResult& res);

        void ParseArray(
            std::function<void(size_t index, DeserializeResult&)> elemHandler,
            DeserializeResult& res);
    };

    // ------------------------------------------------------------------
    // Recursive value deserializer
    // ------------------------------------------------------------------
    static void ReadJSONValue(JSONParser& p,
                               void* base,
                               const PropertyDescriptor& prop,
                               DeserializeResult& res);

    static void ReadJSONStruct(JSONParser& p,
                                void* data,
                                const TypeInfo& type,
                                DeserializeResult& res);

    // ------------------------------------------------------------------
    // Binary helpers
    // ------------------------------------------------------------------
    static void ReadBinaryValue(Save::BinaryReader& reader,
                                 void* base,
                                 const PropertyDescriptor& prop,
                                 DeserializeResult& res);

    // ------------------------------------------------------------------
    // Scene / Prefab node builder
    // ------------------------------------------------------------------
    static void ReadSceneNodeJSON(JSONParser& p,
                                   Scene::SceneManager* scene,
                                   Scene::SceneNode* parent,
                                   ECS::Registry* registry,
                                   DeserializeResult& res);
};

} // namespace KumariEngine::Reflection
