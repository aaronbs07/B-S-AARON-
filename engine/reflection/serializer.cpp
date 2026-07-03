#include "reflection/serializer.hpp"
#include "reflection/type_registry.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "save/EntityGUID.hpp"
#include "core/logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <stdexcept>

namespace KumariEngine::Reflection {

// ===========================================================================
// Internal JSON helpers
// ===========================================================================

static std::string EscapeJSON(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

void Serializer::WriteJSONIndent(std::ostringstream& ss, int indent) {
    for (int i = 0; i < indent; ++i) ss << "  ";
}

void Serializer::WriteJSONValue(std::ostringstream& ss,
                                 const void* base,
                                 const PropertyDescriptor& prop,
                                 int indent) {
    if (prop.IsTransient()) return;

    switch (prop.kind) {
        case PropertyKind::Bool:
            ss << (prop.GetRef<bool>(base) ? "true" : "false");
            break;
        case PropertyKind::Int8:
            ss << static_cast<int>(prop.GetRef<int8_t>(base));
            break;
        case PropertyKind::Int16:
            ss << prop.GetRef<int16_t>(base);
            break;
        case PropertyKind::Int32:
            ss << prop.GetRef<int32_t>(base);
            break;
        case PropertyKind::Int64:
            ss << prop.GetRef<int64_t>(base);
            break;
        case PropertyKind::UInt8:
            ss << static_cast<unsigned>(prop.GetRef<uint8_t>(base));
            break;
        case PropertyKind::UInt16:
            ss << prop.GetRef<uint16_t>(base);
            break;
        case PropertyKind::UInt32:
            ss << prop.GetRef<uint32_t>(base);
            break;
        case PropertyKind::UInt64:
            ss << prop.GetRef<uint64_t>(base);
            break;
        case PropertyKind::Float:
            ss << std::setprecision(7) << prop.GetRef<float>(base);
            break;
        case PropertyKind::Double:
            ss << std::setprecision(15) << prop.GetRef<double>(base);
            break;
        case PropertyKind::String:
            ss << '"' << EscapeJSON(prop.GetRef<std::string>(base)) << '"';
            break;
        case PropertyKind::Vec2: {
            const auto& v = prop.GetRef<glm::vec2>(base);
            ss << "[" << v.x << "," << v.y << "]";
            break;
        }
        case PropertyKind::Vec3: {
            const auto& v = prop.GetRef<glm::vec3>(base);
            ss << "[" << v.x << "," << v.y << "," << v.z << "]";
            break;
        }
        case PropertyKind::Vec4: {
            const auto& v = prop.GetRef<glm::vec4>(base);
            ss << "[" << v.x << "," << v.y << "," << v.z << "," << v.w << "]";
            break;
        }
        case PropertyKind::Quat: {
            const auto& q = prop.GetRef<glm::quat>(base);
            ss << "[" << q.x << "," << q.y << "," << q.z << "," << q.w << "]";
            break;
        }
        case PropertyKind::Mat4: {
            const auto& m = prop.GetRef<glm::mat4>(base);
            ss << "[";
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row) {
                    if (col > 0 || row > 0) ss << ",";
                    ss << m[col][row];
                }
            ss << "]";
            break;
        }
        case PropertyKind::Enum:
            // Serialize enums as their underlying integer value
            ss << *reinterpret_cast<const int32_t*>(
                static_cast<const uint8_t*>(base) + prop.offset);
            break;
        case PropertyKind::Entity:
            ss << prop.GetRef<uint32_t>(base);
            break;
        case PropertyKind::Struct: {
            // Recurse into nested reflected struct
            const TypeInfo* nested = TypeRegistry::Get().FindType(prop.typeId);
            if (nested) {
                WriteJSONStruct(ss,
                    static_cast<const uint8_t*>(base) + prop.offset,
                    *nested, indent);
            } else {
                ss << "null";
            }
            break;
        }
        default:
            ss << "null";
            break;
    }
}

void Serializer::WriteJSONStruct(std::ostringstream& ss,
                                  const void* data,
                                  const TypeInfo& type,
                                  int indent) {
    ss << "{\n";
    WriteJSONIndent(ss, indent + 1);
    ss << "\"__type\": \"" << type.name << "\",\n";
    WriteJSONIndent(ss, indent + 1);
    ss << "\"__version\": 1";

    for (const auto& prop : type.properties) {
        if (prop.IsTransient() || prop.IsHidden()) continue;
        ss << ",\n";
        WriteJSONIndent(ss, indent + 1);
        ss << '"' << EscapeJSON(prop.name) << "\": ";
        WriteJSONValue(ss, data, prop, indent + 1);
    }

    ss << "\n";
    WriteJSONIndent(ss, indent);
    ss << "}";
}

// ===========================================================================
// Public JSON API
// ===========================================================================

std::string Serializer::SerializeToJSON(const void* data, const TypeInfo& type) {
    std::ostringstream ss;
    WriteJSONStruct(ss, data, type, 0);
    return ss.str();
}

bool Serializer::SerializeToJSONFile(const void* data, const TypeInfo& type,
                                      std::string_view path) {
    std::string pathStr(path);
    std::ofstream outFile(pathStr, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        Core::Logger::Error("Reflection", "SerializeToJSONFile: cannot open '%s'", pathStr.c_str());
        return false;
    }
    outFile << SerializeToJSON(data, type);
    return outFile.good();
}

// ===========================================================================
// Binary serialization
// ===========================================================================

void Serializer::WriteBinaryValue(Save::BinaryWriter& writer,
                                   const void* base,
                                   const PropertyDescriptor& prop) {
    if (prop.IsTransient()) return;

    switch (prop.kind) {
        case PropertyKind::Bool:
            writer.WriteBool(prop.GetRef<bool>(base));
            break;
        case PropertyKind::Int8:
            writer.WriteInt8(prop.GetRef<int8_t>(base));
            break;
        case PropertyKind::Int16:
            writer.WriteInt16(prop.GetRef<int16_t>(base));
            break;
        case PropertyKind::Int32:
            writer.WriteInt32(prop.GetRef<int32_t>(base));
            break;
        case PropertyKind::Int64:
            writer.WriteInt64(prop.GetRef<int64_t>(base));
            break;
        case PropertyKind::UInt8:
            writer.WriteUint8(prop.GetRef<uint8_t>(base));
            break;
        case PropertyKind::UInt16:
            writer.WriteUint16(prop.GetRef<uint16_t>(base));
            break;
        case PropertyKind::UInt32:
            writer.WriteUint32(prop.GetRef<uint32_t>(base));
            break;
        case PropertyKind::UInt64:
            writer.WriteUint64(prop.GetRef<uint64_t>(base));
            break;
        case PropertyKind::Float:
            writer.WriteFloat(prop.GetRef<float>(base));
            break;
        case PropertyKind::Double:
            writer.WriteDouble(prop.GetRef<double>(base));
            break;
        case PropertyKind::String:
            writer.WriteString(prop.GetRef<std::string>(base));
            break;
        case PropertyKind::Vec2: {
            const auto& v = prop.GetRef<glm::vec2>(base);
            writer.WriteFloat(v.x); writer.WriteFloat(v.y);
            break;
        }
        case PropertyKind::Vec3: {
            const auto& v = prop.GetRef<glm::vec3>(base);
            writer.WriteFloat(v.x); writer.WriteFloat(v.y); writer.WriteFloat(v.z);
            break;
        }
        case PropertyKind::Vec4: {
            const auto& v = prop.GetRef<glm::vec4>(base);
            writer.WriteFloat(v.x); writer.WriteFloat(v.y);
            writer.WriteFloat(v.z); writer.WriteFloat(v.w);
            break;
        }
        case PropertyKind::Quat: {
            const auto& q = prop.GetRef<glm::quat>(base);
            writer.WriteFloat(q.x); writer.WriteFloat(q.y);
            writer.WriteFloat(q.z); writer.WriteFloat(q.w);
            break;
        }
        case PropertyKind::Mat4: {
            const auto& m = prop.GetRef<glm::mat4>(base);
            writer.WriteArray(&m[0][0], 16);
            break;
        }
        case PropertyKind::Enum:
            writer.WriteInt32(*reinterpret_cast<const int32_t*>(
                static_cast<const uint8_t*>(base) + prop.offset));
            break;
        case PropertyKind::Entity:
            writer.WriteUint32(prop.GetRef<uint32_t>(base));
            break;
        case PropertyKind::Struct: {
            const TypeInfo* nested = TypeRegistry::Get().FindType(prop.typeId);
            if (nested) {
                SerializeToBinary(
                    static_cast<const uint8_t*>(base) + prop.offset,
                    *nested, writer);
            }
            break;
        }
        default: break;
    }
}

void Serializer::SerializeToBinary(const void* data,
                                    const TypeInfo& type,
                                    Save::BinaryWriter& writer) {
    // Write type hash for validation
    writer.WriteUint64(type.typeId);
    // Write property count
    uint32_t propCount = 0;
    for (const auto& p : type.properties)
        if (!p.IsTransient()) ++propCount;
    writer.WriteUint32(propCount);

    for (const auto& prop : type.properties) {
        if (prop.IsTransient()) continue;
        // Write property name hash for backward-compat lookup
        writer.WriteUint64(Reflection::ConstexprHash(prop.name));
        WriteBinaryValue(writer, data, prop);
    }
}

std::vector<uint8_t> Serializer::SerializeToBinaryBlob(const void* data,
                                                          const TypeInfo& type) {
    std::ostringstream oss(std::ios::binary);
    Save::BinaryWriter writer(oss);
    SerializeToBinary(data, type, writer);
    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

// ===========================================================================
// Entity serialization
// ===========================================================================

std::string Serializer::SerializeEntityJSON(ECS::Entity entity,
                                             ECS::Registry* registry) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"__type\": \"Entity\",\n";
    ss << "  \"__version\": 1,\n";
    ss << "  \"entityId\": " << entity;

    // GUID
    auto guid = registry->GetGUID(entity);
    if (!guid.IsNull()) {
        ss << ",\n  \"guid\": \"" << guid.ToString() << "\"";
    }

    // Components: future extension point. Per-component serialization callbacks
    // will be registered separately when component types opt in.
    // TypeRegistry::Get().GetAllComponents() can be used here once a
    // component-serialize callback map is established.

    ss << "\n}";
    return ss.str();
}

std::string Serializer::SerializeEntity(ECS::Entity entity,
                                         ECS::Registry* registry,
                                         SerializeFormat fmt) {
    if (fmt == SerializeFormat::JSON) {
        return SerializeEntityJSON(entity, registry);
    }
    // Binary: wrap in blob
    std::ostringstream oss(std::ios::binary);
    Save::BinaryWriter writer(oss);
    writer.WriteUint32(entity);
    auto guid = registry->GetGUID(entity);
    writer.WriteUint64(guid.high);
    writer.WriteUint64(guid.low);
    std::string str = oss.str();
    return std::string(str.begin(), str.end());
}

// ===========================================================================
// Scene Node recursive JSON writer
// ===========================================================================

void Serializer::WriteSceneNodeJSON(std::ostringstream& ss,
                                     Scene::SceneNode* node,
                                     ECS::Registry* registry,
                                     int indent) {
    WriteJSONIndent(ss, indent);
    ss << "{\n";

    WriteJSONIndent(ss, indent + 1);
    ss << "\"name\": \"" << EscapeJSON(node->GetName()) << "\",\n";

    WriteJSONIndent(ss, indent + 1);
    ss << "\"entityId\": " << node->GetEntity() << ",\n";

    // Transform
    const auto& pos = node->GetLocalPosition();
    const auto& rot = node->GetLocalRotation();
    const auto& scl = node->GetLocalScale();

    WriteJSONIndent(ss, indent + 1);
    ss << "\"position\": [" << pos.x << "," << pos.y << "," << pos.z << "],\n";

    WriteJSONIndent(ss, indent + 1);
    ss << "\"rotation\": [" << rot.x << "," << rot.y << "," << rot.z << "," << rot.w << "],\n";

    WriteJSONIndent(ss, indent + 1);
    ss << "\"scale\": [" << scl.x << "," << scl.y << "," << scl.z << "],\n";

    // GUID
    if (node->GetEntity() != ECS::NULL_ENTITY && registry) {
        auto guid = registry->GetGUID(node->GetEntity());
        if (!guid.IsNull()) {
            WriteJSONIndent(ss, indent + 1);
            ss << "\"guid\": \"" << guid.ToString() << "\",\n";
        }
    }

    // Children
    WriteJSONIndent(ss, indent + 1);
    ss << "\"children\": [";
    const auto& children = node->GetChildren();
    if (!children.empty()) {
        ss << "\n";
        for (size_t i = 0; i < children.size(); ++i) {
            WriteSceneNodeJSON(ss, children[i].get(), registry, indent + 2);
            if (i + 1 < children.size()) ss << ",";
            ss << "\n";
        }
        WriteJSONIndent(ss, indent + 1);
    }
    ss << "]\n";

    WriteJSONIndent(ss, indent);
    ss << "}";
}

// ===========================================================================
// Scene serialization
// ===========================================================================

std::string Serializer::SerializeScene(Scene::SceneManager* scene,
                                        ECS::Registry* registry,
                                        SerializeFormat fmt) {
    if (fmt != SerializeFormat::JSON) {
        Core::Logger::Warning("Reflection", "Binary scene serialization not yet implemented -- using JSON");
    }

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"__type\": \"Scene\",\n";
    ss << "  \"__version\": 1,\n";

    auto* root = scene->GetRootNode();
    ss << "  \"root\": ";
    if (root) {
        WriteSceneNodeJSON(ss, root, registry, 1);
    } else {
        ss << "null";
    }
    ss << "\n}";
    return ss.str();
}

bool Serializer::SerializeSceneToFile(Scene::SceneManager* scene,
                                       ECS::Registry* registry,
                                       std::string_view path,
                                       SerializeFormat fmt) {
    std::string json = SerializeScene(scene, registry, fmt);
    std::string pathStr(path);
    std::ofstream outFile(pathStr, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        Core::Logger::Error("Reflection", "SerializeSceneToFile: cannot open '%s'", pathStr.c_str());
        return false;
    }
    outFile << json;
    Core::Logger::Info("Reflection", "Scene serialized to '%s'", pathStr.c_str());
    return outFile.good();
}

// ===========================================================================
// Prefab serialization
// ===========================================================================

std::string Serializer::SerializePrefab(ECS::Entity rootEntity,
                                         ECS::Registry* registry,
                                         Scene::SceneManager* scene,
                                         SerializeFormat /*fmt*/) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"__type\": \"Prefab\",\n";
    ss << "  \"__version\": 1,\n";

    // GUID of root
    auto guid = registry->GetGUID(rootEntity);
    ss << "  \"rootGuid\": \"" << guid.ToString() << "\",\n";

    // Node subtree
    auto* node = scene->GetNodeByEntity(rootEntity);
    ss << "  \"root\": ";
    if (node) {
        WriteSceneNodeJSON(ss, node, registry, 1);
    } else {
        ss << "null";
    }
    ss << "\n}";
    return ss.str();
}

bool Serializer::SerializePrefabToFile(ECS::Entity rootEntity,
                                        ECS::Registry* registry,
                                        Scene::SceneManager* scene,
                                        std::string_view path,
                                        SerializeFormat fmt) {
    std::string json = SerializePrefab(rootEntity, registry, scene, fmt);
    std::string pathStr(path);
    std::ofstream outFile(pathStr, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        Core::Logger::Error("Reflection", "SerializePrefabToFile: cannot open '%s'", pathStr.c_str());
        return false;
    }
    outFile << json;
    Core::Logger::Info("Reflection", "Prefab (root=%u) serialized to '%s'", rootEntity, pathStr.c_str());
    return outFile.good();
}

} // namespace KumariEngine::Reflection
