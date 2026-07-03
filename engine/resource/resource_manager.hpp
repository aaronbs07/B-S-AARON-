#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <list>
#include <mutex>
#include "resource.hpp"

namespace KumariEngine::Resource {

class ResourceManager {
public:
    static ResourceManager& Get() {
        static ResourceManager instance;
        return instance;
    }

    // Loads or retrieves a resource of type T.
    // The loader function performs the actual load if it is not cached.
    template<typename T, typename LoaderFunc>
    std::shared_ptr<T> Load(const std::string& path, LoaderFunc&& loader) {
        {
            std::shared_lock<std::shared_mutex> readLock(m_mutex);
            auto it = m_resources.find(path);
            if (it != m_resources.end()) {
                auto sharedRes = it->second.lock();
                if (sharedRes) {
                    Touch(path, sharedRes);
                    return std::static_pointer_cast<T>(sharedRes);
                }
            }
        }

        // Resource not in cache or expired, generate it under write lock
        std::unique_lock<std::shared_mutex> writeLock(m_mutex);
        
        // Double-checked locking
        auto it = m_resources.find(path);
        if (it != m_resources.end()) {
            auto sharedRes = it->second.lock();
            if (sharedRes) {
                Touch(path, sharedRes);
                return std::static_pointer_cast<T>(sharedRes);
            }
        }

        std::shared_ptr<T> resource = loader();
        if (resource) {
            resource->SetPath(path);
            m_resources[path] = resource;
            Touch(path, resource);
        }
        return resource;
    }

    template<typename T>
    std::shared_ptr<T> Get(const std::string& path) {
        std::shared_lock<std::shared_mutex> readLock(m_mutex);
        auto it = m_resources.find(path);
        if (it != m_resources.end()) {
            auto sharedRes = it->second.lock();
            if (sharedRes) {
                Touch(path, sharedRes);
                return std::static_pointer_cast<T>(sharedRes);
            }
        }
        return nullptr;
    }

    template<typename T>
    void Add(const std::string& path, std::shared_ptr<T> resource) {
        std::unique_lock<std::shared_mutex> writeLock(m_mutex);
        if (resource) {
            resource->SetPath(path);
            m_resources[path] = resource;
            Touch(path, resource);
        }
    }

    void UnloadUnused();
    void Clear();

    void SetCapacity(size_t capacity) {
        std::lock_guard<std::mutex> lruLock(m_lruMutex);
        m_lruCapacity = capacity;
    }
    size_t GetCapacity() const {
        std::lock_guard<std::mutex> lruLock(m_lruMutex);
        return m_lruCapacity;
    }

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    void Touch(const std::string& path, std::shared_ptr<Resource> resource) {
        if (!resource) return;
        std::lock_guard<std::mutex> lruLock(m_lruMutex);
        auto it = m_lruMap.find(path);
        if (it != m_lruMap.end()) {
            m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
        } else {
            m_lruList.push_front({path, resource});
            m_lruMap[path] = m_lruList.begin();
            if (m_lruList.size() > m_lruCapacity) {
                auto last = m_lruList.back();
                m_lruMap.erase(last.first);
                m_lruList.pop_back();
            }
        }
    }

    void ClearLRU() {
        std::lock_guard<std::mutex> lruLock(m_lruMutex);
        m_lruList.clear();
        m_lruMap.clear();
    }

    std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::weak_ptr<Resource>> m_resources;

    // LRU members
    mutable std::mutex m_lruMutex;
    size_t m_lruCapacity = 64;
    std::list<std::pair<std::string, std::shared_ptr<Resource>>> m_lruList;
    std::unordered_map<std::string, std::list<std::pair<std::string, std::shared_ptr<Resource>>>::iterator> m_lruMap;
};

} // namespace KumariEngine::Resource
