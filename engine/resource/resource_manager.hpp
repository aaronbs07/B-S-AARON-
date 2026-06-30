#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
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
                return std::static_pointer_cast<T>(sharedRes);
            }
        }

        std::shared_ptr<T> resource = loader();
        if (resource) {
            resource->SetPath(path);
            m_resources[path] = resource;
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
        }
    }

    void UnloadUnused();
    void Clear();

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::weak_ptr<Resource>> m_resources;
};

} // namespace KumariEngine::Resource
