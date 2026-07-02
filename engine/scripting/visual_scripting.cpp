#include "visual_scripting.hpp"
#include "core/logger.hpp"
#include "scripting/script_engine.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

namespace KumariEngine::Scripting {

void VSGraph::Connect(const std::string& fromNode, const std::string& fromPin, const std::string& toNode, const std::string& toPin) {
    // Prevent duplicate connections to the same input pin
    Disconnect(toNode, toPin);
    m_connections.push_back({fromNode, fromPin, toNode, toPin});
}

void VSGraph::Disconnect(const std::string& toNode, const std::string& toPin) {
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(), [&](const VSConnection& conn) {
            return conn.toNodeId == toNode && conn.toPinId == toPin;
        }),
        m_connections.end()
    );
}

VSNode* VSGraph::FindNode(const std::string& id) {
    for (auto& node : m_nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

VSPin* VSGraph::FindPin(const std::string& nodeId, const std::string& pinId) {
    VSNode* node = FindNode(nodeId);
    if (!node) return nullptr;
    for (auto& pin : node->inputs) {
        if (pin.id == pinId) return &pin;
    }
    for (auto& pin : node->outputs) {
        if (pin.id == pinId) return &pin;
    }
    return nullptr;
}

std::string VSGraph::Serialize() const {
    std::stringstream ss;
    ss << "[NODES_COUNT] " << m_nodes.size() << "\n";
    for (const auto& node : m_nodes) {
        ss << "[NODE] " << node.id << " " << node.name << " " << node.nodeType << " " << (node.isBreakpoint ? 1 : 0) << "\n";
        ss << "  [INPUTS] " << node.inputs.size() << "\n";
        for (const auto& pin : node.inputs) {
            ss << "    [PIN] " << pin.id << " " << pin.name << " " << (int)pin.type << " " << pin.value.index() << " ";
            if (pin.value.index() == 0) ss << (std::get<bool>(pin.value) ? 1 : 0);
            else if (pin.value.index() == 1) ss << std::get<int>(pin.value);
            else if (pin.value.index() == 2) ss << std::get<float>(pin.value);
            else if (pin.value.index() == 3) {
                std::string s = std::get<std::string>(pin.value);
                if (s.empty()) s = "\"\"";
                ss << s;
            }
            else if (pin.value.index() == 4) ss << std::get<uint32_t>(pin.value);
            ss << "\n";
        }
        ss << "  [OUTPUTS] " << node.outputs.size() << "\n";
        for (const auto& pin : node.outputs) {
            ss << "    [PIN] " << pin.id << " " << pin.name << " " << (int)pin.type << " " << pin.value.index() << " ";
            if (pin.value.index() == 0) ss << (std::get<bool>(pin.value) ? 1 : 0);
            else if (pin.value.index() == 1) ss << std::get<int>(pin.value);
            else if (pin.value.index() == 2) ss << std::get<float>(pin.value);
            else if (pin.value.index() == 3) {
                std::string s = std::get<std::string>(pin.value);
                if (s.empty()) s = "\"\"";
                ss << s;
            }
            else if (pin.value.index() == 4) ss << std::get<uint32_t>(pin.value);
            ss << "\n";
        }
    }
    ss << "[CONNS_COUNT] " << m_connections.size() << "\n";
    for (const auto& conn : m_connections) {
        ss << "[CONN] " << conn.fromNodeId << " " << conn.fromPinId << " " << conn.toNodeId << " " << conn.toPinId << "\n";
    }
    return ss.str();
}

void VSGraph::Deserialize(const std::string& data) {
    m_nodes.clear();
    m_connections.clear();
    m_hitBreakpoint = false;
    m_currentNodeId.clear();
    m_executionTrace.clear();

    std::stringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        std::stringstream lineSs(line);
        std::string token;
        lineSs >> token;
        if (token == "[NODE]") {
            VSNode node;
            int bp = 0;
            lineSs >> node.id >> node.name >> node.nodeType >> bp;
            node.isBreakpoint = (bp != 0);

            // Read Inputs
            std::getline(ss, line); // Inputs header
            std::stringstream inHeadSs(line);
            std::string dummy;
            size_t inCount = 0;
            inHeadSs >> dummy >> inCount;
            for (size_t i = 0; i < inCount; ++i) {
                std::getline(ss, line);
                std::stringstream pinSs(line);
                VSPin pin;
                int typeIdx = 0, valIndex = 0;
                pinSs >> dummy >> pin.id >> pin.name >> typeIdx >> valIndex;
                pin.type = (PinType)typeIdx;
                pin.isOutput = false;
                
                if (valIndex == 0) {
                    int b = 0; pinSs >> b; pin.value = (b != 0);
                } else if (valIndex == 1) {
                    int val = 0; pinSs >> val; pin.value = val;
                } else if (valIndex == 2) {
                    float val = 0.0f; pinSs >> val; pin.value = val;
                } else if (valIndex == 3) {
                    std::string val; pinSs >> val;
                    if (val == "\"\"") val = "";
                    pin.value = val;
                } else if (valIndex == 4) {
                    uint32_t val = 0; pinSs >> val; pin.value = val;
                }
                node.inputs.push_back(pin);
            }

            // Read Outputs
            std::getline(ss, line); // Outputs header
            std::stringstream outHeadSs(line);
            size_t outCount = 0;
            outHeadSs >> dummy >> outCount;
            for (size_t i = 0; i < outCount; ++i) {
                std::getline(ss, line);
                std::stringstream pinSs(line);
                VSPin pin;
                int typeIdx = 0, valIndex = 0;
                pinSs >> dummy >> pin.id >> pin.name >> typeIdx >> valIndex;
                pin.type = (PinType)typeIdx;
                pin.isOutput = true;

                if (valIndex == 0) {
                    int b = 0; pinSs >> b; pin.value = (b != 0);
                } else if (valIndex == 1) {
                    int val = 0; pinSs >> val; pin.value = val;
                } else if (valIndex == 2) {
                    float val = 0.0f; pinSs >> val; pin.value = val;
                } else if (valIndex == 3) {
                    std::string val; pinSs >> val;
                    if (val == "\"\"") val = "";
                    pin.value = val;
                } else if (valIndex == 4) {
                    uint32_t val = 0; pinSs >> val; pin.value = val;
                }
                node.outputs.push_back(pin);
            }
            m_nodes.push_back(node);
        } else if (token == "[CONN]") {
            VSConnection conn;
            lineSs >> conn.fromNodeId >> conn.fromPinId >> conn.toNodeId >> conn.toPinId;
            m_connections.push_back(conn);
        }
    }
}

std::string VSGraph::CompileToLua() const {
    std::stringstream lua;
    lua << "-- Generated Lua code from Visual Script Graph\n";
    lua << "function ExecuteGraph(entity)\n";
    
    // Simple mock compiler output verifying structure mapping
    for (const auto& node : m_nodes) {
        if (node.nodeType == "Print") {
            lua << "    print(\"Visual Script print node executing...\")\n";
        } else if (node.nodeType == "MathAdd") {
            lua << "    local sum = 10 + 20\n";
        } else if (node.nodeType == "LuaCall") {
            lua << "    if KumariEngine ~= nil then print(\"API found\") end\n";
        }
    }
    
    lua << "end\n";
    return lua.str();
}

void VSGraph::Execute(ECS::Registry* registry, ECS::Entity entity, const std::string& eventNodeName) {
    if (m_hitBreakpoint) return;

    // Find starting event node
    VSNode* startNode = nullptr;
    for (auto& node : m_nodes) {
        if (node.nodeType == "Event" && node.name == eventNodeName) {
            startNode = &node;
            break;
        }
    }

    if (startNode) {
        ExecuteNode(registry, entity, startNode);
    }
}

void VSGraph::ExecuteNode(ECS::Registry* registry, ECS::Entity entity, VSNode* node) {
    if (!node) return;

    // Breakpoint check
    if (node->isBreakpoint) {
        m_hitBreakpoint = true;
        m_currentNodeId = node->id;
        Core::Logger::Warning("VisualScripting", "Hit breakpoint on Node ID: %s (%s)", node->id.c_str(), node->name.c_str());
        return;
    }

    m_executionTrace.push_back(node->id);

    // Node-specific action execution
    if (node->nodeType == "MathAdd") {
        VSPin* inA = FindPin(node->id, "A");
        VSPin* inB = FindPin(node->id, "B");
        VSPin* outVal = FindPin(node->id, "Sum");
        if (inA && inB && outVal) {
            float a = std::get<float>(inA->value);
            float b = std::get<float>(inB->value);
            outVal->value = a + b;
        }
    } else if (node->nodeType == "Print") {
        VSPin* msgPin = FindPin(node->id, "Message");
        if (msgPin) {
            std::string msg = std::get<std::string>(msgPin->value);
            Core::Logger::Info("VisualScripting", "Print Node output: %s", msg.c_str());
        }
    } else if (node->nodeType == "LuaCall") {
        // Mock execute inside scripting engine state
        lua_State* L = ScriptEngine::Get().GetLuaState();
        if (L) {
            Core::Logger::Info("VisualScripting", "LuaCall node routed to Lua State");
        }
    }

    // Follow connection from output Exec pin
    std::string nextNodeId;
    for (const auto& conn : m_connections) {
        if (conn.fromNodeId == node->id && conn.fromPinId == "OutExec") {
            nextNodeId = conn.toNodeId;
            break;
        }
    }

    if (!nextNodeId.empty()) {
        VSNode* nextNode = FindNode(nextNodeId);
        ExecuteNode(registry, entity, nextNode);
    }
}

void VSGraph::Step() {
    if (!m_hitBreakpoint || m_currentNodeId.empty()) return;
    
    VSNode* node = FindNode(m_currentNodeId);
    if (!node) return;

    m_hitBreakpoint = false; // Temporarily unset to allow this node to run
    m_currentNodeId.clear();
    
    // Find next node via Exec link
    std::string nextNodeId;
    for (const auto& conn : m_connections) {
        if (conn.fromNodeId == node->id && conn.fromPinId == "OutExec") {
            nextNodeId = conn.toNodeId;
            break;
        }
    }

    if (!nextNodeId.empty()) {
        VSNode* nextNode = FindNode(nextNodeId);
        if (nextNode) {
            ExecuteNode(nullptr, 0, nextNode);
        }
    }
}

void VSGraph::Resume() {
    if (!m_hitBreakpoint || m_currentNodeId.empty()) return;

    std::string nextNodeId = m_currentNodeId;
    m_currentNodeId.clear();
    m_hitBreakpoint = false;
    
    VSNode* nextNode = FindNode(nextNodeId);
    if (nextNode) {
        bool wasBreakpoint = nextNode->isBreakpoint;
        nextNode->isBreakpoint = false;
        ExecuteNode(nullptr, 0, nextNode);
        nextNode->isBreakpoint = wasBreakpoint;
    }
}


// --- VisualScriptingSystem implementation ---

void VisualScriptingSystem::Update(ECS::Registry* registry, float deltaTime) {
    (void)deltaTime;
    if (!registry) return;

    registry->Each<VisualScriptingComponent>([&](ECS::Entity entity, VisualScriptingComponent& vsc) {
        if (!vsc.currentEventToTrigger.empty()) {
            vsc.graph.Execute(registry, entity, vsc.currentEventToTrigger);
            vsc.currentEventToTrigger.clear();
        } else {
            // Default frame update trigger
            vsc.graph.Execute(registry, entity, "Update");
        }
    });
}

} // namespace KumariEngine::Scripting
