#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <sstream>
#include "ecs/ecs.hpp"

namespace KumariEngine::Scripting {

enum class PinType {
    Exec,
    Bool,
    Int,
    Float,
    String,
    Entity
};

struct VSPin {
    std::string id;
    std::string name;
    PinType type;
    bool isOutput = false;
    std::variant<bool, int, float, std::string, uint32_t> value;
};

struct VSNode {
    std::string id;
    std::string name;
    std::vector<VSPin> inputs;
    std::vector<VSPin> outputs;
    bool isBreakpoint = false;
    
    // Type of node for execution logic
    std::string nodeType; // "Event", "MathAdd", "Branch", "Print", "LuaCall"
};

struct VSConnection {
    std::string fromNodeId;
    std::string fromPinId;
    std::string toNodeId;
    std::string toPinId;
};

class VSGraph {
public:
    void AddNode(const VSNode& node) { m_nodes.push_back(node); }
    void Connect(const std::string& fromNode, const std::string& fromPin, const std::string& toNode, const std::string& toPin);
    void Disconnect(const std::string& fromNode, const std::string& fromPin);
    
    std::string Serialize() const;
    void Deserialize(const std::string& data);
    
    // Execution
    void Execute(ECS::Registry* registry, ECS::Entity entity, const std::string& eventNodeName);
    
    // Lua Compilation
    std::string CompileToLua() const;
    
    // Debugging API
    void Step();
    void Resume();
    bool HitBreakpoint() const { return m_hitBreakpoint; }
    const std::string& GetCurrentNodeId() const { return m_currentNodeId; }
    const std::vector<std::string>& GetExecutionTrace() const { return m_executionTrace; }
    void ClearTrace() { m_executionTrace.clear(); }

    const std::vector<VSNode>& GetNodes() const { return m_nodes; }
    const std::vector<VSConnection>& GetConnections() const { return m_connections; }
    
    // Node lookup helper
    VSNode* FindNode(const std::string& id);
    VSPin* FindPin(const std::string& nodeId, const std::string& pinId);

private:
    std::vector<VSNode> m_nodes;
    std::vector<VSConnection> m_connections;
    
    // Debugging state
    bool m_hitBreakpoint = false;
    std::string m_currentNodeId;
    std::vector<std::string> m_executionTrace;
    
    void ExecuteNode(ECS::Registry* registry, ECS::Entity entity, VSNode* node);
};

struct VisualScriptingComponent {
    VSGraph graph;
    std::string currentEventToTrigger;
};

class VisualScriptingSystem {
public:
    static VisualScriptingSystem& Get() {
        static VisualScriptingSystem instance;
        return instance;
    }
    
    void Update(ECS::Registry* registry, float deltaTime);
};

} // namespace KumariEngine::Scripting
