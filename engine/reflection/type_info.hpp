#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>
#include "reflection/metadata.hpp"

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// Stable compile-time FNV-1a 64-bit hash — no RTTI required
// ---------------------------------------------------------------------------
constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

constexpr uint64_t ConstexprHash(std::string_view str) noexcept {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// TypeId<T>() — RTTI-free compile-time stable type identifier
// Uses the mangled function signature as a seed for FNV-1a.
// ---------------------------------------------------------------------------
template<typename T>
constexpr uint64_t TypeId() noexcept {
#if defined(_MSC_VER)
    return ConstexprHash(__FUNCSIG__);
#else
    return ConstexprHash(__PRETTY_FUNCTION__);
#endif
}

// ---------------------------------------------------------------------------
// TypeCategory — semantic classification
// ---------------------------------------------------------------------------
enum class TypeCategory : uint8_t {
    Unknown   = 0,
    Primitive = 1,
    Struct    = 2,
    Component = 3,
    Enum      = 4,
    Asset     = 5,
};

// Forward-declare PropertyDescriptor (defined in property.hpp)
struct PropertyDescriptor;

// ---------------------------------------------------------------------------
// TypeInfo — complete reflection record for one type
// ---------------------------------------------------------------------------
struct TypeInfo {
    std::string     name;
    uint64_t        typeId       = 0;
    size_t          size         = 0;
    size_t          alignment    = 0;
    TypeCategory    category     = TypeCategory::Unknown;
    MetadataMap     metadata;

    // Optional default constructor / destructor function pointers
    std::function<void*(void*)>  construct;    // placement-new into a pre-allocated buffer
    std::function<void(void*)>   destruct;     // call destructor without deallocation

    // Registered properties (ordered by registration)
    std::vector<PropertyDescriptor> properties;

    // Helpers
    bool IsComponent() const { return category == TypeCategory::Component; }
    bool HasProperty(std::string_view propName) const;
    const PropertyDescriptor* FindProperty(std::string_view propName) const;
    const PropertyDescriptor* FindPropertyById(uint64_t propTypeId) const;
};

} // namespace KumariEngine::Reflection
