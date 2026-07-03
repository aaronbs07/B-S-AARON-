#include "reflection/deserializer.hpp"
#include "reflection/type_registry.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "save/EntityGUID.hpp"
#include "core/logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <charconv>
#include <cstring>

namespace KumariEngine::Reflection {

// ===========================================================================
// JSONParser helpers
// ===========================================================================

void Deserializer::JSONParser::SkipWhitespace() {
    while (!AtEnd() && (src[pos] == ' ' || src[pos] == '\t' ||
                        src[pos] == '\n' || src[pos] == '\r'))
        ++pos;
}

bool Deserializer::JSONParser::Match(char c) {
    SkipWhitespace();
    if (!AtEnd() && src[pos] == c) { ++pos; return true; }
    return false;
}

bool Deserializer::JSONParser::Expect(char c, DeserializeResult& res) {
    SkipWhitespace();
    if (!AtEnd() && src[pos] == c) { ++pos; return true; }
    if (res.success) {
        res.success = false;
        res.error = std::string("Expected '") + c + "' at pos " + std::to_string(pos);
    }
    return false;
}

std::string Deserializer::JSONParser::ParseString(DeserializeResult& res) {
    SkipWhitespace();
    if (AtEnd() || src[pos] != '"') {
        res.success = false;
        res.error = "Expected '\"' for string";
        return {};
    }
    ++pos;
    std::string out;
    while (!AtEnd()) {
        char c = src[pos++];
        if (c == '"') break;
        if (c == '\\' && !AtEnd()) {
            char esc = src[pos++];
            switch (esc) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += esc;  break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

double Deserializer::JSONParser::ParseNumber(DeserializeResult& res) {
    SkipWhitespace();
    size_t start = pos;
    if (!AtEnd() && (src[pos] == '-' || src[pos] == '+')) ++pos;
    while (!AtEnd() && (std::isdigit(src[pos]) || src[pos] == '.' ||
                        src[pos] == 'e' || src[pos] == 'E' ||
                        src[pos] == '+' || src[pos] == '-'))
        ++pos;
    if (pos == start) {
        res.success = false;
        res.error = "Expected number at pos " + std::to_string(pos);
        return 0.0;
    }
    try {
        return std::stod(std::string(src.substr(start, pos - start)));
    } catch (...) {
        res.success = false;
        res.error = "Invalid number at pos " + std::to_string(start);
        return 0.0;
    }
}

bool Deserializer::JSONParser::ParseBool(DeserializeResult& res) {
    SkipWhitespace();
    if (src.substr(pos, 4) == "true")  { pos += 4; return true;  }
    if (src.substr(pos, 5) == "false") { pos += 5; return false; }
    res.success = false;
    res.error = "Expected bool at pos " + std::to_string(pos);
    return false;
}

bool Deserializer::JSONParser::ParseNull() {
    SkipWhitespace();
    if (src.substr(pos, 4) == "null") { pos += 4; return true; }
    return false;
}

void Deserializer::JSONParser::SkipValue(DeserializeResult& res) {
    SkipWhitespace();
    if (AtEnd()) return;
    char c = Peek();
    if (c == '"')  { ParseString(res); }
    else if (c == '{') { SkipObject(res); }
    else if (c == '[') { SkipArray(res); }
    else if (c == 't' || c == 'f') { ParseBool(res); }
    else if (c == 'n') { ParseNull(); }
    else { ParseNumber(res); }
}

void Deserializer::JSONParser::SkipObject(DeserializeResult& res) {
    Expect('{', res);
    SkipWhitespace();
    if (Match('}')) return;
    do {
        ParseString(res);
        Expect(':', res);
        SkipValue(res);
    } while (Match(','));
    Expect('}', res);
}

void Deserializer::JSONParser::SkipArray(DeserializeResult& res) {
    Expect('[', res);
    SkipWhitespace();
    if (Match(']')) return;
    do { SkipValue(res); } while (Match(','));
    Expect(']', res);
}

bool Deserializer::JSONParser::ParseObject(
    std::function<void(std::string_view, DeserializeResult&)> fieldHandler,
    DeserializeResult& res)
{
    if (!Expect('{', res)) return false;
    SkipWhitespace();
    if (Match('}')) return true;
    do {
        SkipWhitespace();
        std::string key = ParseString(res);
        if (!res) return false;
        if (!Expect(':', res)) return false;
        SkipWhitespace();
        fieldHandler(key, res);
        if (!res) return false;
        SkipWhitespace();
    } while (Match(','));
    return Expect('}', res);
}

void Deserializer::JSONParser::ParseArray(
    std::function<void(size_t, DeserializeResult&)> elemHandler,
    DeserializeResult& res)
{
    Expect('[', res);
    SkipWhitespace();
    if (Match(']')) return;
    size_t idx = 0;
    do {
        SkipWhitespace();
        elemHandler(idx++, res);
        SkipWhitespace();
    } while (Match(','));
    Expect(']', res);
}

// ===========================================================================
// ReadJSONValue — dispatch by property kind
// ===========================================================================

// ReadNextFloat — file-local helper; uses lambda inside ReadJSONValue instead
// (removed: JSONParser is private in Deserializer, so a free function cannot access it)

void Deserializer::ReadJSONValue(JSONParser& p,
                                  void* base,
                                  const PropertyDescriptor& prop,
                                  DeserializeResult& res) {
    if (prop.IsTransient()) { p.SkipValue(res); return; }

    // Lambda replaces the removed free-function ReadNextFloat
    auto readF = [&](DeserializeResult& r) -> float {
        return static_cast<float>(p.ParseNumber(r));
    };

    switch (prop.kind) {
        case PropertyKind::Bool:
            prop.Set<bool>(base, p.ParseBool(res));
            break;
        case PropertyKind::Int8:
            prop.Set<int8_t>(base, static_cast<int8_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::Int16:
            prop.Set<int16_t>(base, static_cast<int16_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::Int32:
            prop.Set<int32_t>(base, static_cast<int32_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::Int64:
            prop.Set<int64_t>(base, static_cast<int64_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::UInt8:
            prop.Set<uint8_t>(base, static_cast<uint8_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::UInt16:
            prop.Set<uint16_t>(base, static_cast<uint16_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::UInt32:
        case PropertyKind::Entity:
            prop.Set<uint32_t>(base, static_cast<uint32_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::UInt64:
            prop.Set<uint64_t>(base, static_cast<uint64_t>(p.ParseNumber(res)));
            break;
        case PropertyKind::Float:
            prop.Set<float>(base, static_cast<float>(p.ParseNumber(res)));
            break;
        case PropertyKind::Double:
            prop.Set<double>(base, p.ParseNumber(res));
            break;
        case PropertyKind::String:
            prop.Set<std::string>(base, p.ParseString(res));
            break;
        case PropertyKind::Vec2: {
            glm::vec2 v;
            p.ParseArray([&](size_t i, DeserializeResult& r) {
                float val = readF(r);
                if (i == 0) v.x = val;
                else if (i == 1) v.y = val;
            }, res);
            prop.Set<glm::vec2>(base, v);
            break;
        }
        case PropertyKind::Vec3: {
            glm::vec3 v;
            p.ParseArray([&](size_t i, DeserializeResult& r) {
                float val = readF(r);
                if (i == 0) v.x = val;
                else if (i == 1) v.y = val;
                else if (i == 2) v.z = val;
            }, res);
            prop.Set<glm::vec3>(base, v);
            break;
        }
        case PropertyKind::Vec4: {
            glm::vec4 v;
            p.ParseArray([&](size_t i, DeserializeResult& r) {
                float val = readF(r);
                if (i == 0) v.x = val;
                else if (i == 1) v.y = val;
                else if (i == 2) v.z = val;
                else if (i == 3) v.w = val;
            }, res);
            prop.Set<glm::vec4>(base, v);
            break;
        }
        case PropertyKind::Quat: {
            glm::quat q;
            p.ParseArray([&](size_t i, DeserializeResult& r) {
                float val = readF(r);
                if (i == 0) q.x = val;
                else if (i == 1) q.y = val;
                else if (i == 2) q.z = val;
                else if (i == 3) q.w = val;
            }, res);
            prop.Set<glm::quat>(base, q);
            break;
        }
        case PropertyKind::Mat4: {
            glm::mat4 m(1.0f);
            int n = 0;
            p.ParseArray([&](size_t /*i*/, DeserializeResult& r) {
                float val = readF(r);
                if (n < 16) m[n / 4][n % 4] = val;
                ++n;
            }, res);
            prop.Set<glm::mat4>(base, m);
            break;
        }
        case PropertyKind::Enum:
            *reinterpret_cast<int32_t*>(
                static_cast<uint8_t*>(base) + prop.offset) =
                    static_cast<int32_t>(p.ParseNumber(res));
            break;
        case PropertyKind::Struct: {
            const TypeInfo* nested = TypeRegistry::Get().FindType(prop.typeId);
            if (nested) {
                ReadJSONStruct(p,
                    static_cast<uint8_t*>(base) + prop.offset,
                    *nested, res);
            } else {
                p.SkipValue(res);
            }
            break;
        }
        default:
            p.SkipValue(res);
            break;
    }
}

// ===========================================================================
// ReadJSONStruct
// ===========================================================================

void Deserializer::ReadJSONStruct(JSONParser& p,
                                   void* data,
                                   const TypeInfo& type,
                                   DeserializeResult& res) {
    p.ParseObject([&](std::string_view key, DeserializeResult& r) {
        // Skip meta-keys
        if (key == "__type" || key == "__version") {
            p.SkipValue(r);
            if (key == "__version")
                r.formatVersion = static_cast<uint32_t>(0); // already consumed
            return;
        }

        const PropertyDescriptor* prop = type.FindProperty(key);
        if (!prop) {
            ++r.unknownPropertiesSkipped;
            p.SkipValue(r);
            return;
        }
        ReadJSONValue(p, data, *prop, r);
    }, res);
}

// ===========================================================================
// Public JSON API
// ===========================================================================

DeserializeResult Deserializer::DeserializeFromJSON(void* data,
                                                     const TypeInfo& type,
                                                     std::string_view json) {
    DeserializeResult res;
    JSONParser p;
    p.src = json;
    ReadJSONStruct(p, data, type, res);
    return res;
}

DeserializeResult Deserializer::DeserializeFromJSONFile(void* data,
                                                          const TypeInfo& type,
                                                          std::string_view path) {
    std::string pathStr(path);
    std::ifstream inFile(pathStr);
    if (!inFile.is_open()) {
        return { false, std::string("Cannot open file: ") + pathStr };
    }
    std::ostringstream ss;
    ss << inFile.rdbuf();
    return DeserializeFromJSON(data, type, ss.str());
}

// ===========================================================================
// Binary deserialization
// ===========================================================================

void Deserializer::ReadBinaryValue(Save::BinaryReader& reader,
                                    void* base,
                                    const PropertyDescriptor& prop,
                                    DeserializeResult& /*res*/) {
    // Helper lambdas that use BinaryReader's output-parameter API
    auto readF = [&]() -> float  { float  v{}; reader.ReadFloat(v);  return v; };
    auto readD = [&]() -> double { double v{}; reader.ReadDouble(v); return v; };

    switch (prop.kind) {
        case PropertyKind::Bool: {
            bool v{};
            reader.ReadBool(v);
            prop.Set<bool>(base, v);
            break;
        }
        case PropertyKind::Int8: {
            int8_t v{};
            reader.ReadInt8(v);
            prop.Set<int8_t>(base, v);
            break;
        }
        case PropertyKind::Int16: {
            int16_t v{};
            reader.ReadInt16(v);
            prop.Set<int16_t>(base, v);
            break;
        }
        case PropertyKind::Int32: {
            int32_t v{};
            reader.ReadInt32(v);
            prop.Set<int32_t>(base, v);
            break;
        }
        case PropertyKind::Int64: {
            int64_t v{};
            reader.ReadInt64(v);
            prop.Set<int64_t>(base, v);
            break;
        }
        case PropertyKind::UInt8: {
            uint8_t v{};
            reader.ReadUint8(v);
            prop.Set<uint8_t>(base, v);
            break;
        }
        case PropertyKind::UInt16: {
            uint16_t v{};
            reader.ReadUint16(v);
            prop.Set<uint16_t>(base, v);
            break;
        }
        case PropertyKind::UInt32:
        case PropertyKind::Entity: {
            uint32_t v{};
            reader.ReadUint32(v);
            prop.Set<uint32_t>(base, v);
            break;
        }
        case PropertyKind::UInt64: {
            uint64_t v{};
            reader.ReadUint64(v);
            prop.Set<uint64_t>(base, v);
            break;
        }
        case PropertyKind::Float:
            prop.Set<float>(base, readF());
            break;
        case PropertyKind::Double:
            prop.Set<double>(base, readD());
            break;
        case PropertyKind::String: {
            std::string v;
            reader.ReadString(v);
            prop.Set<std::string>(base, v);
            break;
        }
        case PropertyKind::Vec2:
            prop.Set<glm::vec2>(base, { readF(), readF() });
            break;
        case PropertyKind::Vec3:
            prop.Set<glm::vec3>(base, { readF(), readF(), readF() });
            break;
        case PropertyKind::Vec4:
            prop.Set<glm::vec4>(base, { readF(), readF(), readF(), readF() });
            break;
        case PropertyKind::Quat: {
            float x = readF(), y = readF(), z = readF(), w = readF();
            prop.Set<glm::quat>(base, glm::quat(w, x, y, z));
            break;
        }
        case PropertyKind::Mat4: {
            glm::mat4 m;
            for (int i = 0; i < 16; ++i)
                (&m[0][0])[i] = readF();
            prop.Set<glm::mat4>(base, m);
            break;
        }
        case PropertyKind::Enum: {
            int32_t v{};
            reader.ReadInt32(v);
            *reinterpret_cast<int32_t*>(
                static_cast<uint8_t*>(base) + prop.offset) = v;
            break;
        }
        default: break;
    }
}

DeserializeResult Deserializer::DeserializeFromBinary(void* data,
                                                        const TypeInfo& type,
                                                        Save::BinaryReader& reader) {
    DeserializeResult res;

    // Read and validate type hash
    uint64_t storedTypeId{};
    reader.ReadUint64(storedTypeId);
    if (storedTypeId != type.typeId) {
        Core::Logger::Warning("Reflection",
            "Binary type mismatch: expected typeId %llu, got %llu",
            static_cast<unsigned long long>(type.typeId),
            static_cast<unsigned long long>(storedTypeId));
    }

    uint32_t storedPropCount{};
    reader.ReadUint32(storedPropCount);
    for (uint32_t i = 0; i < storedPropCount; ++i) {
        uint64_t nameHash{};
        reader.ReadUint64(nameHash);

        // Find property by name hash
        const PropertyDescriptor* found = nullptr;
        for (const auto& p : type.properties) {
            if (ConstexprHash(p.name) == nameHash) {
                found = &p;
                break;
            }
        }

        if (!found) {
            ++res.unknownPropertiesSkipped;
            // We cannot safely skip binary data of unknown size here without
            // type info, so we log a warning and abort remaining fields.
            Core::Logger::Warning("Reflection",
                "DeserializeFromBinary: unknown property hash -- "
                "aborting remaining fields (forward-compat limit)");
            break;
        }
        ReadBinaryValue(reader, data, *found, res);
    }

    // Apply defaults for missing properties
    for (const auto& p : type.properties) {
        // Properties not in the stored data retain their default value
        // (the caller is responsible for calling the default constructor first)
        (void)p;
    }

    return res;
}

DeserializeResult Deserializer::DeserializeFromBinaryBlob(
    void* data, const TypeInfo& type, const std::vector<uint8_t>& blob)
{
    // Wrap blob in a string-based stream
    std::string str(blob.begin(), blob.end());
    std::istringstream iss(str, std::ios::binary);
    Save::BinaryReader reader(iss);
    return DeserializeFromBinary(data, type, reader);
}

// ===========================================================================
// Scene node builder from JSON
// ===========================================================================

void Deserializer::ReadSceneNodeJSON(JSONParser& p,
                                      Scene::SceneManager* scene,
                                      Scene::SceneNode* parent,
                                      ECS::Registry* registry,
                                      DeserializeResult& res) {
    std::string name;
    uint32_t entityId = ECS::NULL_ENTITY;
    std::string guidStr;
    glm::vec3 pos(0.0f), scl(1.0f);
    glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
    std::vector<size_t> childOffsets; // track child JSON positions

    // Temporary child parse callback list
    std::vector<std::function<void()>> childCallbacks;

    p.ParseObject([&](std::string_view key, DeserializeResult& r) {
        if (key == "name") {
            name = p.ParseString(r);
        } else if (key == "entityId") {
            entityId = static_cast<uint32_t>(p.ParseNumber(r));
        } else if (key == "guid") {
            guidStr = p.ParseString(r);
        } else if (key == "position") {
            p.ParseArray([&](size_t i, DeserializeResult& ar) {
                float v = static_cast<float>(p.ParseNumber(ar));
                if (i == 0) pos.x = v;
                else if (i == 1) pos.y = v;
                else if (i == 2) pos.z = v;
            }, r);
        } else if (key == "rotation") {
            p.ParseArray([&](size_t i, DeserializeResult& ar) {
                float v = static_cast<float>(p.ParseNumber(ar));
                if (i == 0) rot.x = v;
                else if (i == 1) rot.y = v;
                else if (i == 2) rot.z = v;
                else if (i == 3) rot.w = v;
            }, r);
        } else if (key == "scale") {
            p.ParseArray([&](size_t i, DeserializeResult& ar) {
                float v = static_cast<float>(p.ParseNumber(ar));
                if (i == 0) scl.x = v;
                else if (i == 1) scl.y = v;
                else if (i == 2) scl.z = v;
            }, r);
        } else if (key == "children") {
            // Parse children array — each element is a recursive node
            p.ParseArray([&](size_t /*i*/, DeserializeResult& ar) {
                childCallbacks.emplace_back([&]() {
                    ReadSceneNodeJSON(p, scene, nullptr, registry, ar);
                });
                // We need to parse inline rather than defer, since parser is sequential
                childCallbacks.clear();
                // Actually parse children inline during the array iteration
                ReadSceneNodeJSON(p, scene, nullptr, registry, ar);
            }, r);
        } else {
            ++r.unknownPropertiesSkipped;
            p.SkipValue(r);
        }
    }, res);

    // Create or reuse entity
    ECS::Entity entity = ECS::NULL_ENTITY;
    if (!guidStr.empty() && registry) {
        // Try to find existing entity by GUID
        Save::EntityGUID guid;
        // Parse the hex string
        if (guidStr.size() >= 32) {
            try {
                guid.high = std::stoull(guidStr.substr(0, 16), nullptr, 16);
                guid.low  = std::stoull(guidStr.substr(16, 16), nullptr, 16);
                entity = registry->GetEntityByGUID(guid);
                if (entity == ECS::NULL_ENTITY) {
                    entity = registry->CreateEntity();
                    registry->AssignGUID(entity, guid);
                }
            } catch (...) {
                entity = registry->CreateEntity();
            }
        } else {
            entity = registry->CreateEntity();
        }
    } else if (registry) {
        entity = registry->CreateEntity();
    }

    // Create scene node
    if (scene && !name.empty()) {
        auto* node = scene->CreateNode(name, parent);
        if (node) {
            node->SetLocalPosition(pos);
            node->SetLocalRotation(rot);
            node->SetLocalScale(scl);
            if (entity != ECS::NULL_ENTITY) {
                node->SetEntity(entity);
                scene->RegisterEntityNode(entity, node);
            }
        }
    }
}

// ===========================================================================
// Public Scene API
// ===========================================================================

DeserializeResult Deserializer::DeserializeScene(Scene::SceneManager* scene,
                                                   ECS::Registry* registry,
                                                   std::string_view json) {
    DeserializeResult res;
    JSONParser p;
    p.src = json;

    p.ParseObject([&](std::string_view key, DeserializeResult& r) {
        if (key == "__type" || key == "__version") {
            p.SkipValue(r);
        } else if (key == "root") {
            if (p.Peek() == 'n') {
                p.ParseNull();
            } else {
                ReadSceneNodeJSON(p, scene, nullptr, registry, r);
            }
        } else {
            ++r.unknownPropertiesSkipped;
            p.SkipValue(r);
        }
    }, res);

    return res;
}

DeserializeResult Deserializer::DeserializeSceneFromFile(
    Scene::SceneManager* scene, ECS::Registry* registry, std::string_view path)
{
    std::string pathStr(path);
    std::ifstream inFile(pathStr);
    if (!inFile.is_open()) {
        return { false, std::string("Cannot open scene file: ") + pathStr };
    }
    std::ostringstream ss;
    ss << inFile.rdbuf();
    auto res = DeserializeScene(scene, registry, ss.str());
    if (res) {
        Core::Logger::Info("Reflection", "Scene loaded from '%s'", pathStr.c_str());
    } else {
        Core::Logger::Error("Reflection", "Scene load failed: %s", res.error.c_str());
    }
    return res;
}

// ===========================================================================
// Prefab
// ===========================================================================

DeserializeResult Deserializer::DeserializePrefabFromFile(
    ECS::Registry* registry, Scene::SceneManager* scene, std::string_view path)
{
    std::string pathStr(path);
    std::ifstream inFile(pathStr);
    if (!inFile.is_open()) {
        return { false, std::string("Cannot open prefab file: ") + pathStr };
    }
    std::ostringstream ss;
    ss << inFile.rdbuf();
    std::string json = ss.str();

    DeserializeResult res;
    JSONParser p;
    p.src = json;

    p.ParseObject([&](std::string_view key, DeserializeResult& r) {
        if (key == "__type" || key == "__version" || key == "rootGuid") {
            p.SkipValue(r);
        } else if (key == "root") {
            if (p.Peek() == 'n') {
                p.ParseNull();
            } else {
                ReadSceneNodeJSON(p, scene, nullptr, registry, r);
            }
        } else {
            ++r.unknownPropertiesSkipped;
            p.SkipValue(r);
        }
    }, res);

    if (res) {
        Core::Logger::Info("Reflection", "Prefab loaded from '%s'", std::string(path).c_str());
    } else {
        Core::Logger::Error("Reflection", "Prefab load failed: %s", res.error.c_str());
    }
    return res;
}

} // namespace KumariEngine::Reflection
