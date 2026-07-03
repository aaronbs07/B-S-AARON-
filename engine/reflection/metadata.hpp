#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <cstdint>

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// PropertyFlags — bitmask controlling inspector/serializer behavior
// ---------------------------------------------------------------------------
enum class PropertyFlags : uint32_t {
    None          = 0,
    ReadOnly      = 1 << 0,   // Cannot be modified via inspector
    Hidden        = 1 << 1,   // Not shown in inspector
    Transient     = 1 << 2,   // Excluded from serialization
    AssetRef      = 1 << 3,   // Value is an asset path/GUID
    Color         = 1 << 4,   // Render as color picker
    HasRange      = 1 << 5,   // Min/Max metadata available
    Multiline     = 1 << 6,   // String field uses multiline edit
    NoDefault     = 1 << 7,   // No default-value fallback on deserialize
};

inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) {
    return static_cast<PropertyFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline PropertyFlags operator&(PropertyFlags a, PropertyFlags b) {
    return static_cast<PropertyFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool HasFlag(PropertyFlags flags, PropertyFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ---------------------------------------------------------------------------
// MetaAttribute — typed metadata value stored per-property or per-type
// ---------------------------------------------------------------------------
using MetaValue = std::variant<
    std::monostate,   // empty / not set
    bool,
    int64_t,
    double,
    std::string
>;

struct MetaAttribute {
    MetaValue value;

    MetaAttribute() = default;
    explicit MetaAttribute(bool v)             : value(v) {}
    explicit MetaAttribute(int64_t v)          : value(v) {}
    explicit MetaAttribute(double v)           : value(v) {}
    explicit MetaAttribute(std::string_view v) : value(std::string(v)) {}

    bool        AsBool()   const { return std::get<bool>(value); }
    int64_t     AsInt()    const { return std::get<int64_t>(value); }
    double      AsDouble() const { return std::get<double>(value); }
    std::string AsString() const { return std::get<std::string>(value); }

    bool IsEmpty()  const { return std::holds_alternative<std::monostate>(value); }
    bool IsBool()   const { return std::holds_alternative<bool>(value); }
    bool IsInt()    const { return std::holds_alternative<int64_t>(value); }
    bool IsDouble() const { return std::holds_alternative<double>(value); }
    bool IsString() const { return std::holds_alternative<std::string>(value); }
};

// ---------------------------------------------------------------------------
// MetadataMap — per-property/per-type key-value store
// ---------------------------------------------------------------------------
using MetadataMap = std::unordered_map<std::string, MetaAttribute>;

// Standard metadata keys (use these constants to avoid typos)
namespace Meta {
    inline constexpr std::string_view DisplayName    = "DisplayName";
    inline constexpr std::string_view Tooltip        = "Tooltip";
    inline constexpr std::string_view Category       = "Category";
    inline constexpr std::string_view Min            = "Min";
    inline constexpr std::string_view Max            = "Max";
    inline constexpr std::string_view ReadOnly       = "ReadOnly";
    inline constexpr std::string_view Hidden         = "Hidden";
    inline constexpr std::string_view Color          = "Color";
    inline constexpr std::string_view AssetReference = "AssetReference";
    inline constexpr std::string_view Step           = "Step";
    inline constexpr std::string_view EnumNames      = "EnumNames";
} // namespace Meta

} // namespace KumariEngine::Reflection
