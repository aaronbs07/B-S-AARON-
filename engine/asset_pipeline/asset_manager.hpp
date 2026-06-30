#pragma once
#include <string>
#include <memory>
#include <future>
#include <functional>
#include "resource/resource_manager.hpp"

namespace KumariEngine::Asset {

class AssetManager {
public:
    static AssetManager& Get() {
        static AssetManager instance;
        return instance;
    }

    // Asynchronously loads a resource of type T and registers it in the ResourceManager
    template<typename T, typename LoaderFunc>
    std::future<std::shared_ptr<T>> LoadAsync(const std::string& path, LoaderFunc&& loader) {
        return std::async(std::launch::async, [path, loader = std::forward<LoaderFunc>(loader)]() {
            auto resource = loader();
            if (resource) {
                Resource::ResourceManager::Get().Add<T>(path, resource);
            }
            return resource;
        });
    }

private:
    AssetManager() = default;
    ~AssetManager() = default;
};

} // namespace KumariEngine::Asset
