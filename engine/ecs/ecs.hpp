#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include <limits>
#include <tuple>
#include <algorithm>
#include <cassert>
#include "core/logger.hpp"
#include "save/EntityGUID.hpp"

namespace KumariEngine::ECS {

using Entity = uint32_t;
constexpr Entity NULL_ENTITY = 0;

/// <summary>
/// Interface for component pools to manage entity-deletion triggers.
/// </summary>
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(Entity entity) = 0;
    virtual void EntityDestroyed(Entity entity) = 0;
};

/// <summary>
/// Contiguous cache-friendly component storage mapped via sparse set.
/// </summary>
template<typename T>
class ComponentPool : public IComponentPool {
public:
    ComponentPool() {
        m_dense.reserve(4096);
        m_denseEntities.reserve(4096);
    }

    ~ComponentPool() override {
        KumariEngine::Core::Logger::Info("ECS_Perf", "ComponentPool destructor called");
    }

    T& Add(Entity entity, T&& component) {
        if (Has(entity)) {
            throw std::runtime_error("Component already exists on this Entity.");
        }

        if (entity >= m_sparse.size()) {
            m_sparse.resize(entity + 1, -1);
        }

        m_sparse[entity] = static_cast<int>(m_dense.size());
        m_denseEntities.push_back(entity);
        m_dense.push_back(std::move(component));

        return m_dense.back();
    }

    void Remove(Entity entity) override {
        if (!Has(entity)) return;

        size_t indexToRemove = m_sparse[entity];
        size_t lastIndex = m_dense.size() - 1;

        if (indexToRemove != lastIndex) {
            // Swap last element with the one being removed to maintain contiguity
            m_dense[indexToRemove] = std::move(m_dense[lastIndex]);
            Entity lastEntity = m_denseEntities[lastIndex];
            m_denseEntities[indexToRemove] = lastEntity;
            m_sparse[lastEntity] = static_cast<int>(indexToRemove);
        }

        m_dense.pop_back();
        m_denseEntities.pop_back();
        m_sparse[entity] = -1;
    }

    bool Has(Entity entity) const {
        if (entity >= m_sparse.size()) return false;
        return m_sparse[entity] != -1;
    }

    T& Get(Entity entity) {
        if (!Has(entity)) {
            throw std::runtime_error("Component does not exist on this Entity.");
        }
        return m_dense[m_sparse[entity]];
    }

    void EntityDestroyed(Entity entity) override {
        Remove(entity);
    }

    void Reserve(size_t capacity) {
        m_dense.reserve(capacity);
        m_denseEntities.reserve(capacity);
    }

    const std::vector<T>& GetData() const { return m_dense; }
    const std::vector<Entity>& GetEntities() const { return m_denseEntities; }
    size_t Size() const { return m_dense.size(); }

private:
    std::vector<T> m_dense;
    std::vector<Entity> m_denseEntities;
    std::vector<int> m_sparse;
};

/// <summary>
/// Coordinates Entities, component allocation, and queries.
/// </summary>
class Registry {
public:
    Registry();
    ~Registry();

    Entity CreateEntity();
    Entity CreateEntityWithID(Entity entity);
    void DestroyEntity(Entity entity);

    // Entity GUID Mapping support
    Save::EntityGUID GetGUID(Entity entity) const;
    Entity GetEntityByGUID(const Save::EntityGUID& guid) const;
    void AssignGUID(Entity entity, const Save::EntityGUID& guid);
    Save::EntityGUID CreateGUID(Entity entity);

    template<typename T>
    void RegisterComponent() {
        auto type = std::type_index(typeid(T));
        if (m_componentPools.find(type) == m_componentPools.end()) {
            m_componentPools[type] = std::make_unique<ComponentPool<T>>();
        }
    }

    template<typename T>
    ComponentPool<T>* GetPool() {
        auto type = std::type_index(typeid(T));
        auto it = m_componentPools.find(type);
        if (it == m_componentPools.end()) {
            RegisterComponent<T>();
            return static_cast<ComponentPool<T>*>(m_componentPools[type].get());
        }
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    bool IsAlive(Entity entity) const {
        if (entity == NULL_ENTITY || entity >= m_nextEntity) return false;
        if (entity < m_activeEntities.size()) {
            return m_activeEntities[entity];
        }
        return false;
    }

    template<typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args) {
        assert(IsAlive(entity) && "Cannot add component to a dead or inactive entity");
        auto pool = GetPool<T>();
        return pool->Add(entity, T(std::forward<Args>(args)...));
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        assert(IsAlive(entity) && "Cannot remove component from a dead or inactive entity");
        auto pool = GetPool<T>();
        pool->Remove(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        assert(IsAlive(entity) && "Cannot get component from a dead or inactive entity");
        auto pool = GetPool<T>();
        return pool->Get(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) {
        if (!IsAlive(entity)) return false;
        auto pool = GetPool<T>();
        return pool->Has(entity);
    }

    template<typename T>
    void Reserve(size_t capacity) {
        GetPool<T>()->Reserve(capacity);
    }

    std::vector<Entity> GetAliveEntities() const;

    /// <summary>
    /// Returns a list of entities possessing all specified components.
    /// Iterates over the smallest pool for optimal performance.
    /// </summary>
    template<typename... Components>
    std::vector<Entity> View() {
        std::vector<Entity> results;
        
        // Return empty vector if no components specified
        if constexpr (sizeof...(Components) == 0) {
            return results;
        }

        // Get the pool sizes to find the smallest pool
        std::vector<size_t> sizes = { GetPool<Components>()->Size()... };
        size_t minSize = std::numeric_limits<size_t>::max();
        size_t minIndex = 0;
        
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i] < minSize) {
                minSize = sizes[i];
                minIndex = i;
            }
        }

        if (minSize == 0) return results;

        // Tuple helper to access pools
        auto poolsTuple = std::make_tuple(GetPool<Components>()...);

        // Fetch entities of the smallest pool and filter them
        results.reserve(minSize);
        
        // Execute dynamic helper depending on the index of the smallest pool
        auto processEntities = [&](auto poolPtr) {
            const auto& entities = poolPtr->GetEntities();
            for (Entity entity : entities) {
                if ((HasComponent<Components>(entity) && ...)) {
                    results.push_back(entity);
                }
            }
        };

        std::apply([&](auto&&... pools) {
            size_t idx = 0;
            ((idx++ == minIndex ? processEntities(pools) : void()), ...);
        }, poolsTuple);

        return results;
    }

    /// <summary>
    /// Executes a system function on all entities with matching components.
    /// Zero allocations: optimal for real-time loops and high entity counts.
    /// </summary>
    template<typename... Components, typename Func>
    void Each(Func&& func) {
        if constexpr (sizeof...(Components) == 0) {
            return;
        }

        // Find smallest pool
        std::vector<size_t> sizes = { GetPool<Components>()->Size()... };
        size_t minSize = std::numeric_limits<size_t>::max();
        size_t minIndex = 0;
        
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i] < minSize) {
                minSize = sizes[i];
                minIndex = i;
            }
        }

        if (minSize == 0) return;

        // Tuple helper to access pools
        auto poolsTuple = std::make_tuple(GetPool<Components>()...);

        auto processEntities = [&](auto poolPtr) {
            const auto& entities = poolPtr->GetEntities();
            for (Entity entity : entities) {
                if ((HasComponent<Components>(entity) && ...)) {
                    func(entity, GetComponent<Components>(entity)...);
                }
            }
        };

        std::apply([&](auto&&... pools) {
            size_t idx = 0;
            ((idx++ == minIndex ? processEntities(pools) : void()), ...);
        }, poolsTuple);
    }

private:
    Entity m_nextEntity = 1;
    std::vector<Entity> m_freeEntities;
    std::vector<bool> m_activeEntities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_componentPools;
    std::unordered_map<Entity, Save::EntityGUID> m_entityToGUID;
    std::unordered_map<Save::EntityGUID, Entity> m_guidToEntity;
};

} // namespace KumariEngine::ECS
