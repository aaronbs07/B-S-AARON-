#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <functional>
#include <cstdint>
#include "reflection/type_info.hpp"
#include "reflection/property.hpp"

namespace KumariEngine::Reflection {

// ---------------------------------------------------------------------------
// TypeBuilder — fluent builder returned by RegisterType/RegisterComponent
// Allows chaining .Property(...).Meta(...) calls before committing
// ---------------------------------------------------------------------------
class TypeBuilder {
public:
    explicit TypeBuilder(TypeInfo* info) : m_info(info) {}

    /// Add a property to the type being registered
    TypeBuilder& Property(PropertyDescriptor desc) {
        if (m_info) m_info->properties.push_back(std::move(desc));
        return *this;
    }

    /// Add top-level metadata to the type
    TypeBuilder& Meta(std::string_view key, MetaAttribute value) {
        if (m_info) m_info->metadata[std::string(key)] = std::move(value);
        return *this;
    }

private:
    TypeInfo* m_info;
};

// ---------------------------------------------------------------------------
// TypeRegistry — global singleton; owns all TypeInfo records
// ---------------------------------------------------------------------------
class TypeRegistry {
public:
    static TypeRegistry& Get() {
        static TypeRegistry instance;
        return instance;
    }

    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // ------------------------------------------------------------------
    // Registration API
    // ------------------------------------------------------------------

    /// Register a plain struct / type. Returns a builder for chaining.
    template<typename T>
    TypeBuilder RegisterType(std::string_view name,
                             TypeCategory category = TypeCategory::Struct) {
        std::lock_guard lock(m_mutex);
        return RegisterImpl<T>(name, category);
    }

    /// Register an ECS component type. Automatically marks as Component category.
    template<typename T>
    TypeBuilder RegisterComponent(std::string_view name) {
        std::lock_guard lock(m_mutex);
        auto builder = RegisterImpl<T>(name, TypeCategory::Component);
        m_componentTypeIds.push_back(TypeId<T>());
        return builder;
    }

    // ------------------------------------------------------------------
    // Lookup API
    // ------------------------------------------------------------------

    const TypeInfo* FindType(std::string_view name) const {
        std::lock_guard lock(m_mutex);
        auto it = m_nameIndex.find(std::string(name));
        if (it == m_nameIndex.end()) return nullptr;
        return &m_types.at(it->second);
    }

    const TypeInfo* FindType(uint64_t typeId) const {
        std::lock_guard lock(m_mutex);
        auto it = m_idIndex.find(typeId);
        if (it == m_idIndex.end()) return nullptr;
        return &m_types.at(it->second);
    }

    std::vector<const TypeInfo*> GetAllTypes() const {
        std::lock_guard lock(m_mutex);
        std::vector<const TypeInfo*> result;
        result.reserve(m_types.size());
        for (const auto& [id, info] : m_types) result.push_back(&info);
        return result;
    }

    std::vector<const TypeInfo*> GetAllComponents() const {
        std::lock_guard lock(m_mutex);
        std::vector<const TypeInfo*> result;
        for (uint64_t id : m_componentTypeIds) {
            auto it = m_idIndex.find(id);
            if (it != m_idIndex.end()) {
                result.push_back(&m_types.at(it->second));
            }
        }
        return result;
    }

    size_t TypeCount()      const { std::lock_guard lock(m_mutex); return m_types.size(); }
    size_t ComponentCount() const { std::lock_guard lock(m_mutex); return m_componentTypeIds.size(); }

    // ------------------------------------------------------------------
    // Hot-Reload hook — invalidates cached type data for a reloaded type
    // ------------------------------------------------------------------
    void OnTypeReloaded(uint64_t typeId);

    // ------------------------------------------------------------------
    // Listener for hot-reload events
    // ------------------------------------------------------------------
    using ReloadCallback = std::function<void(uint64_t typeId)>;
    void RegisterReloadListener(ReloadCallback cb) {
        std::lock_guard lock(m_mutex);
        m_reloadListeners.push_back(std::move(cb));
    }

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;

    template<typename T>
    TypeBuilder RegisterImpl(std::string_view name, TypeCategory category) {
        // Idempotent: if already registered, return builder to existing record
        uint64_t id = TypeId<T>();
        auto idIt = m_idIndex.find(id);
        if (idIt != m_idIndex.end()) {
            return TypeBuilder(&m_types.at(id));
        }

        TypeInfo info;
        info.name      = std::string(name);
        info.typeId    = id;
        info.size      = sizeof(T);
        info.alignment = alignof(T);
        info.category  = category;

        // Register default constructor / destructor
        if constexpr (std::is_default_constructible_v<T>) {
            info.construct = [](void* buf) -> void* {
                return new(buf) T();
            };
        }
        if constexpr (std::is_destructible_v<T>) {
            info.destruct = [](void* ptr) {
                static_cast<T*>(ptr)->~T();
            };
        }

        m_nameIndex[std::string(name)] = id;
        m_idIndex[id]                  = id;
        m_types[id]                    = std::move(info);

        return TypeBuilder(&m_types.at(id));
    }

    mutable std::recursive_mutex m_mutex;

    // Storage: typeId → TypeInfo
    std::unordered_map<uint64_t, TypeInfo>   m_types;
    // Indexes
    std::unordered_map<std::string, uint64_t> m_nameIndex;
    std::unordered_map<uint64_t, uint64_t>    m_idIndex;   // id → id (used as key)
    // Component type set
    std::vector<uint64_t>                     m_componentTypeIds;
    // Hot-reload listeners
    std::vector<ReloadCallback>               m_reloadListeners;
};

} // namespace KumariEngine::Reflection
