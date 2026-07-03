#include "reflection/property.hpp"
#include "reflection/type_info.hpp"
#include <algorithm>

namespace KumariEngine::Reflection {

bool TypeInfo::HasProperty(std::string_view propName) const {
    return FindProperty(propName) != nullptr;
}

const PropertyDescriptor* TypeInfo::FindProperty(std::string_view propName) const {
    for (const auto& p : properties) {
        if (p.name == propName) return &p;
    }
    return nullptr;
}

const PropertyDescriptor* TypeInfo::FindPropertyById(uint64_t propTypeId) const {
    for (const auto& p : properties) {
        if (p.typeId == propTypeId) return &p;
    }
    return nullptr;
}

} // namespace KumariEngine::Reflection
