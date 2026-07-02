#include "gameplay/DialogueSystem.hpp"
#include "scripting/script_engine.hpp"
#include "core/logger.hpp"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace KumariEngine::Gameplay {

// DialogueDatabase Implementation
void DialogueDatabase::RegisterDialogue(const DialogueTree& tree) {
    m_dialogues[tree.dialogueId] = tree;
}

const DialogueTree* DialogueDatabase::GetDialogue(const std::string& dialogueId) const {
    auto it = m_dialogues.find(dialogueId);
    if (it != m_dialogues.end()) {
        return &it->second;
    }
    return nullptr;
}

void DialogueDatabase::Clear() {
    m_dialogues.clear();
}

static bool EvaluateLuaCondition(const std::string& code) {
    if (code.empty()) return true;
    lua_State* L = Scripting::ScriptEngine::Get().GetLuaState();
    if (!L) return true;

    // Wrap in a return statement if it doesn't have one to evaluate expression, 
    // or run it directly if it's a full return block.
    std::string fullCode = code;
    if (code.rfind("return", 0) != 0) {
        fullCode = "return (" + code + ")";
    }

    int top = lua_gettop(L);
    int status = luaL_dostring(L, fullCode.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        Core::Logger::Warning("DialogueSystem", "Lua condition error: %s", err ? err : "unknown");
        lua_settop(L, top);
        return false;
    }

    bool result = true;
    if (lua_gettop(L) > top) {
        if (lua_isboolean(L, -1)) {
            result = lua_toboolean(L, -1) != 0;
        }
        lua_pop(L, 1);
    }
    return result;
}

static void ExecuteLuaCallback(const std::string& code) {
    if (code.empty()) return;
    lua_State* L = Scripting::ScriptEngine::Get().GetLuaState();
    if (!L) return;

    int top = lua_gettop(L);
    int status = luaL_dostring(L, code.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        Core::Logger::Warning("DialogueSystem", "Lua callback execution error: %s", err ? err : "unknown");
    }
    lua_settop(L, top);
}

bool DialogueDatabase::StartDialogue(ECS::Registry* registry, ECS::Entity player, const std::string& dialogueId) {
    if (!registry || !registry->IsAlive(player)) return false;

    const auto* tree = GetDialogue(dialogueId);
    if (!tree) return false;

    if (!registry->HasComponent<DialogueComponent>(player)) {
        registry->AddComponent<DialogueComponent>(player);
    }

    auto& dc = registry->GetComponent<DialogueComponent>(player);
    dc.currentDialogueId = dialogueId;
    dc.currentNodeId = tree->startNodeId;
    dc.isInDialogue = true;

    // Trigger starting node callback
    auto nodeIt = tree->nodes.find(dc.currentNodeId);
    if (nodeIt != tree->nodes.end()) {
        ExecuteLuaCallback(nodeIt->second.callbackLua);
        Core::Logger::Info("DialogueSystem", "Dialogue started on node: %s", dc.currentNodeId.c_str());
    } else {
        // Empty tree/invalid node, end dialogue immediately
        dc.isInDialogue = false;
        return false;
    }

    return true;
}

bool DialogueDatabase::ChooseOption(ECS::Registry* registry, ECS::Entity player, size_t choiceIndex) {
    if (!registry || !registry->IsAlive(player) || !registry->HasComponent<DialogueComponent>(player)) return false;

    auto& dc = registry->GetComponent<DialogueComponent>(player);
    if (!dc.isInDialogue) return false;

    const auto* tree = GetDialogue(dc.currentDialogueId);
    if (!tree) {
        dc.isInDialogue = false;
        return false;
    }

    auto nodeIt = tree->nodes.find(dc.currentNodeId);
    if (nodeIt == tree->nodes.end()) {
        dc.isInDialogue = false;
        return false;
    }

    const auto& node = nodeIt->second;

    std::string nextNodeId;

    if (node.choices.empty()) {
        // Auto-advance
        nextNodeId = node.nextNodeId;
    } else {
        // Index check
        if (choiceIndex >= node.choices.size()) return false;
        const auto& choice = node.choices[choiceIndex];

        // Evaluate choice condition
        if (!EvaluateLuaCondition(choice.conditionLua)) {
            return false; // Condition not met
        }
        nextNodeId = choice.nextNodeId;
    }

    if (nextNodeId.empty()) {
        // Dialog ends
        dc.isInDialogue = false;
        Core::Logger::Info("DialogueSystem", "Dialogue finished");
        return true;
    }

    dc.currentNodeId = nextNodeId;
    auto nextNodeIt = tree->nodes.find(dc.currentNodeId);
    if (nextNodeIt == tree->nodes.end()) {
        dc.isInDialogue = false;
        Core::Logger::Info("DialogueSystem", "Dialogue finished (invalid next node %s)", nextNodeId.c_str());
        return true;
    }

    // Trigger next node callback
    ExecuteLuaCallback(nextNodeIt->second.callbackLua);
    Core::Logger::Info("DialogueSystem", "Dialogue advanced to node: %s", dc.currentNodeId.c_str());
    return true;
}

const DialogueNode* DialogueDatabase::GetCurrentNode(ECS::Registry* registry, ECS::Entity player) const {
    if (!registry || !registry->IsAlive(player) || !registry->HasComponent<DialogueComponent>(player)) return nullptr;

    const auto& dc = registry->GetComponent<DialogueComponent>(player);
    if (!dc.isInDialogue) return nullptr;

    const auto* tree = GetDialogue(dc.currentDialogueId);
    if (!tree) return nullptr;

    auto it = tree->nodes.find(dc.currentNodeId);
    if (it != tree->nodes.end()) {
        return &it->second;
    }

    return nullptr;
}

} // namespace KumariEngine::Gameplay
