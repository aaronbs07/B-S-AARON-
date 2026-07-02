#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace KumariEngine::Platform {

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual void OnInitialize() = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnShutdown() = 0;
    
    virtual std::string GetName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::string GetRequiredEngineVersion() const = 0;
};

class PluginManager {
public:
    static PluginManager& Get() {
        static PluginManager instance;
        return instance;
    }
    
    bool LoadPlugin(std::shared_ptr<Plugin> plugin);
    bool UnloadPlugin(const std::string& name);
    void Update(float dt);
    
    const std::unordered_map<std::string, std::shared_ptr<Plugin>>& GetPlugins() const { return m_plugins; }
    void ClearPlugins();
    
    // Engine Version configuration
    std::string GetEngineVersion() const { return m_engineVersion; }
    void SetEngineVersion(const std::string& version) { m_engineVersion = version; }
    
    // API Export
    void GenerateAPIDocumentation(const std::string& filename) const;

private:
    PluginManager() = default;
    ~PluginManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<Plugin>> m_plugins;
    std::string m_engineVersion = "1.0.0";
};

} // namespace KumariEngine::Platform
