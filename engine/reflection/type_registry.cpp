#include "reflection/type_registry.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Reflection {

void TypeRegistry::OnTypeReloaded(uint64_t typeId) {
    std::lock_guard lock(m_mutex);

    auto it = m_types.find(typeId);
    if (it == m_types.end()) {
        Core::Logger::Warning("Reflection",
        "OnTypeReloaded: unknown typeId — not in registry");
        return;
    }

    Core::Logger::Info("Reflection",
        "Type '%s' reloaded — notifying %d listener(s)",
        it->second.name.c_str(),
        static_cast<int>(m_reloadListeners.size()));

    for (auto& cb : m_reloadListeners) {
        cb(typeId);
    }
}

} // namespace KumariEngine::Reflection
