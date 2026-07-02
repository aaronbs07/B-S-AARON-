#include "plugin_manager.hpp"
#include "core/logger.hpp"
#include <fstream>

namespace KumariEngine::Platform {

bool PluginManager::LoadPlugin(std::shared_ptr<Plugin> plugin) {
    if (!plugin) return false;

    std::string name = plugin->GetName();
    std::string reqVer = plugin->GetRequiredEngineVersion();

    if (reqVer != m_engineVersion) {
        Core::Logger::Error("Plugins", "Failed to load plugin '%s': Version Mismatch! Engine: %s, Plugin requests: %s",
                            name.c_str(), m_engineVersion.c_str(), reqVer.c_str());
        return false;
    }

    if (m_plugins.find(name) != m_plugins.end()) {
        Core::Logger::Warning("Plugins", "Plugin '%s' is already loaded.", name.c_str());
        return true;
    }

    plugin->OnInitialize();
    m_plugins[name] = plugin;
    Core::Logger::Info("Plugins", "Successfully loaded plugin: %s (v%s)", name.c_str(), plugin->GetVersion().c_str());
    return true;
}

bool PluginManager::UnloadPlugin(const std::string& name) {
    auto it = m_plugins.find(name);
    if (it != m_plugins.end()) {
        it->second->OnShutdown();
        m_plugins.erase(it);
        Core::Logger::Info("Plugins", "Unloaded plugin: %s", name.c_str());
        return true;
    }
    return false;
}

void PluginManager::Update(float dt) {
    for (auto& [name, plugin] : m_plugins) {
        (void)name;
        plugin->OnUpdate(dt);
    }
}

void PluginManager::ClearPlugins() {
    for (auto& [name, plugin] : m_plugins) {
        (void)name;
        plugin->OnShutdown();
    }
    m_plugins.clear();
}

void PluginManager::GenerateAPIDocumentation(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) return;

    out << "# Kumari Kandam Engine Plugin API Reference (v" << m_engineVersion << ")\n\n";
    out << "Welcome to the API Documentation export. Extend the engine using standard C++ modules or Lua bindings.\n\n";
    
    out << "## Core Classes and Components\n";
    out << "### ECS::Registry\n";
    out << "- `CreateEntity() -> Entity`: Spawns a new entity.\n";
    out << "- `DestroyEntity(Entity)`: Destroys entity and frees pools.\n";
    out << "- `AddComponent<T>(Entity, Args...)`: Adds component to entity.\n";
    out << "- `GetComponent<T>(Entity) -> T&`: Fetches reference to component.\n";
    out << "- `HasComponent<T>(Entity) -> bool`: Checks existence.\n\n";
    
    out << "### Animation::AnimationComponent\n";
    out << "- `stateMachine`: Current active state machine configurations.\n";
    out << "- `currentTime`: Time offset in seconds.\n";
    out << "- `isPlaying`: Playback control toggle.\n\n";

    out << "### AI::NavMeshComponent\n";
    out << "- `agentTarget`: Vector representing coordinates pathfinder traverses toward.\n";
    out << "- `agentSpeed`: Speed multiplier.\n\n";

    out << "### Scripting::VisualScriptingComponent\n";
    out << "- `graph`: Contains nodes and connections layout.\n";
    
    out << "\n## Plugin Interface Details\n";
    out << "```cpp\n";
    out << "class Plugin {\n";
    out << "public:\n";
    out << "    virtual void OnInitialize() = 0;\n";
    out << "    virtual void OnUpdate(float dt) = 0;\n";
    out << "    virtual void OnShutdown() = 0;\n";
    out << "};\n";
    out << "```\n";
}

} // namespace KumariEngine::Platform
