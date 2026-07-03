#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "reflection/metadata.hpp"
#include "reflection/type_info.hpp"

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// PropertyKind — wire type used by serializer to dispatch reads/writes
// ---------------------------------------------------------------------------
enum class PropertyKind : uint8_t {
    Unknown = 0,
    Bool,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float, Double,
    String,
    Vec2, Vec3, Vec4,
    Quat,
    Mat4,
    Enum,
    Struct,     // nested reflected struct
    Entity,     // ECS::Entity (uint32_t alias)
    VectorOf,   // std::vector<T> — element kind stored separately
};

// ---------------------------------------------------------------------------
// PropertyKindTraits<T> — maps a C++ type to PropertyKind at compile time
// ---------------------------------------------------------------------------
template<typename T> struct PropertyKindTraits {
    static constexpr PropertyKind Kind = PropertyKind::Struct;
};
template<> struct PropertyKindTraits<bool>        { static constexpr PropertyKind Kind = PropertyKind::Bool;   };
template<> struct PropertyKindTraits<int8_t>      { static constexpr PropertyKind Kind = PropertyKind::Int8;   };
template<> struct PropertyKindTraits<int16_t>     { static constexpr PropertyKind Kind = PropertyKind::Int16;  };
template<> struct PropertyKindTraits<int32_t>     { static constexpr PropertyKind Kind = PropertyKind::Int32;  };
template<> struct PropertyKindTraits<int64_t>     { static constexpr PropertyKind Kind = PropertyKind::Int64;  };
template<> struct PropertyKindTraits<uint8_t>     { static constexpr PropertyKind Kind = PropertyKind::UInt8;  };
template<> struct PropertyKindTraits<uint16_t>    { static constexpr PropertyKind Kind = PropertyKind::UInt16; };
template<> struct PropertyKindTraits<uint32_t>    { static constexpr PropertyKind Kind = PropertyKind::UInt32; };
template<> struct PropertyKindTraits<uint64_t>    { static constexpr PropertyKind Kind = PropertyKind::UInt64; };
template<> struct PropertyKindTraits<float>       { static constexpr PropertyKind Kind = PropertyKind::Float;  };
template<> struct PropertyKindTraits<double>      { static constexpr PropertyKind Kind = PropertyKind::Double; };
template<> struct PropertyKindTraits<std::string> { static constexpr PropertyKind Kind = PropertyKind::String; };
template<> struct PropertyKindTraits<glm::vec2>   { static constexpr PropertyKind Kind = PropertyKind::Vec2;   };
template<> struct PropertyKindTraits<glm::vec3>   { static constexpr PropertyKind Kind = PropertyKind::Vec3;   };
template<> struct PropertyKindTraits<glm::vec4>   { static constexpr PropertyKind Kind = PropertyKind::Vec4;   };
template<> struct PropertyKindTraits<glm::quat>   { static constexpr PropertyKind Kind = PropertyKind::Quat;   };
template<> struct PropertyKindTraits<glm::mat4>   { static constexpr PropertyKind Kind = PropertyKind::Mat4;   };

// ---------------------------------------------------------------------------
// PropertyDescriptor — runtime record for a single reflected field
// ---------------------------------------------------------------------------
struct PropertyDescriptor {
    std::string   name;          // e.g. "position"
    std::string   typeName;      // human-readable type name, e.g. "float"
    uint64_t      typeId  = 0;   // TypeId<FieldType>()
    size_t        offset  = 0;   // byte offset from struct base (via offsetof)
    size_t        size    = 0;   // sizeof(FieldType)
    PropertyKind  kind    = PropertyKind::Unknown;
    PropertyFlags flags   = PropertyFlags::None;
    MetadataMap   metadata;

    // For VectorOf kind: the element descriptor
    PropertyKind  elementKind   = PropertyKind::Unknown;
    uint64_t      elementTypeId = 0;

    // Typed accessor helpers — zero virtual dispatch
    template<typename T>
    T& GetRef(void* base) const {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
    }

    template<typename T>
    const T& GetRef(const void* base) const {
        return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset);
    }

    template<typename T>
    void Set(void* base, const T& value) const {
        *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset) = value;
    }

    // Raw byte-level copy (generic fallback)
    void CopyValue(const void* src, void* dst) const {
        std::memcpy(
            static_cast<uint8_t*>(dst) + offset,
            static_cast<const uint8_t*>(src) + offset,
            size);
    }

    bool IsReadOnly()  const { return HasFlag(flags, PropertyFlags::ReadOnly); }
    bool IsHidden()    const { return HasFlag(flags, PropertyFlags::Hidden); }
    bool IsTransient() const { return HasFlag(flags, PropertyFlags::Transient); }

    // Metadata accessors
    bool HasMeta(std::string_view key) const {
        return metadata.find(std::string(key)) != metadata.end();
    }
    const MetaAttribute* GetMeta(std::string_view key) const {
        auto it = metadata.find(std::string(key));
        return it != metadata.end() ? &it->second : nullptr;
    }
};

// ---------------------------------------------------------------------------
// MakeProperty<Owner, FieldType> — factory helper used by registration macros
// ---------------------------------------------------------------------------
template<typename Owner, typename FieldType>
PropertyDescriptor MakeProperty(
    std::string_view  name,
    size_t            memberOffset,
    PropertyFlags     flags    = PropertyFlags::None,
    MetadataMap       meta     = {})
{
    PropertyDescriptor desc;
    desc.name    = std::string(name);
    desc.typeId  = TypeId<FieldType>();
    desc.offset  = memberOffset;
    desc.size    = sizeof(FieldType);
    desc.kind    = PropertyKindTraits<FieldType>::Kind;
    desc.flags   = flags;
    desc.metadata = std::move(meta);

    // Populate human-readable type name
    if constexpr (std::is_same_v<FieldType, bool>)        desc.typeName = "bool";
    else if constexpr (std::is_same_v<FieldType, int8_t>) desc.typeName = "int8";
    else if constexpr (std::is_same_v<FieldType, int16_t>)desc.typeName = "int16";
    else if constexpr (std::is_same_v<FieldType, int32_t>)desc.typeName = "int32";
    else if constexpr (std::is_same_v<FieldType, int64_t>)desc.typeName = "int64";
    else if constexpr (std::is_same_v<FieldType, uint8_t>)  desc.typeName = "uint8";
    else if constexpr (std::is_same_v<FieldType, uint16_t>) desc.typeName = "uint16";
    else if constexpr (std::is_same_v<FieldType, uint32_t>) desc.typeName = "uint32";
    else if constexpr (std::is_same_v<FieldType, uint64_t>) desc.typeName = "uint64";
    else if constexpr (std::is_same_v<FieldType, float>)   desc.typeName = "float";
    else if constexpr (std::is_same_v<FieldType, double>)  desc.typeName = "double";
    else if constexpr (std::is_same_v<FieldType, std::string>) desc.typeName = "string";
    else if constexpr (std::is_same_v<FieldType, glm::vec2>)   desc.typeName = "vec2";
    else if constexpr (std::is_same_v<FieldType, glm::vec3>)   desc.typeName = "vec3";
    else if constexpr (std::is_same_v<FieldType, glm::vec4>)   desc.typeName = "vec4";
    else if constexpr (std::is_same_v<FieldType, glm::quat>)   desc.typeName = "quat";
    else if constexpr (std::is_same_v<FieldType, glm::mat4>)   desc.typeName = "mat4";
    else if constexpr (std::is_enum_v<FieldType>) {
        desc.typeName = "enum";
        desc.kind = PropertyKind::Enum;
    }
    else desc.typeName = "struct";

    return desc;
}

} // namespace KumariEngine::Reflection
