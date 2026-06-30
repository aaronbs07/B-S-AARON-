#include "resource_manager.hpp"

namespace KumariEngine::Resource {

void ResourceManager::UnloadUnused() {
    std::unique_lock<std::shared_mutex> writeLock(m_mutex);
    for (auto it = m_resources.begin(); it != m_resources.end();) {
        if (it->second.expired()) {
            it = m_resources.erase(it);
        } else {
            ++it;
        }
    }
}

void ResourceManager::Clear() {
    std::unique_lock<std::shared_mutex> writeLock(m_mutex);
    m_resources.clear();
}

} // namespace KumariEngine::Resource
