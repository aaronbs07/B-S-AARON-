#include "scripting/script_component.hpp"
#include "scripting/script_engine.hpp"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include "core/logger.hpp"
#include "core/event_manager.hpp"
#include "scene/scene_manager.hpp"
#include "scene/scene_node.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "timeline/cinematic_system.hpp"
#include "save/BinaryWriter.hpp"
#include "save/BinaryReader.hpp"
#include "scripting/hot_reload_manager.hpp"
#include "networking/RPCManager.hpp"
#include "networking/NetworkManager.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "gameplay/GameInstance.hpp"
#include "gameplay/LevelManager.hpp"
#include "gameplay/World.hpp"
#include "gameplay/InventorySystem.hpp"
#include "gameplay/EquipmentSystem.hpp"
#include "gameplay/CraftingSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/DialogueSystem.hpp"
#include "gameplay/InteractionSystem.hpp"
#include <stdexcept>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace KumariEngine::ECS {

ScriptComponent::~ScriptComponent() {
    Release();
}

ScriptComponent::ScriptComponent(ScriptComponent&& other) noexcept
    : scriptPath(std::move(other.scriptPath)),
      envRef(other.envRef),
      initialized(other.initialized) {
    other.envRef = -1;
    other.initialized = false;
}

ScriptComponent& ScriptComponent::operator=(ScriptComponent&& other) noexcept {
    if (this != &other) {
        Release();
        scriptPath = std::move(other.scriptPath);
        envRef = other.envRef;
        initialized = other.initialized;
        other.envRef = -1;
        other.initialized = false;
    }
    return *this;
}

void ScriptComponent::Release() {
    if (envRef != -1 && Scripting::ScriptEngine::Get().GetLuaState()) {
        Scripting::ScriptEngine::Get().UnloadScript(envRef);
        envRef = -1;
    }
}

} // namespace KumariEngine::ECS

namespace KumariEngine::Scripting {

static void ClearQueuedEvents(ScriptEngine::ScriptInfo& info, lua_State* L) {
    for (auto& qe : info.queuedEvents) {
        if (qe.customDataRef != -1 && L) {
            luaL_unref(L, LUA_REGISTRYINDEX, qe.customDataRef);
        }
    }
    info.queuedEvents.clear();
}

static void ParseLuaError(const std::string& err, std::string& outFilename, int& outLineNum, std::string& outDetails) {
    outLineNum = 0;
    outFilename = "";
    outDetails = err;

    size_t startPos = 0;
    if (err.size() > 2 && err[1] == ':' && (err[2] == '\\' || err[2] == '/')) {
        startPos = 3;
    }

    size_t colon1 = err.find(':', startPos);
    if (colon1 != std::string::npos) {
        size_t colon2 = err.find(':', colon1 + 1);
        if (colon2 != std::string::npos) {
            std::string lineStr = err.substr(colon1 + 1, colon2 - colon1 - 1);
            try {
                outLineNum = std::stoi(lineStr);
                outFilename = err.substr(0, colon1);
                outDetails = err.substr(colon2 + 1);
                if (!outDetails.empty() && outDetails[0] == ' ') {
                    outDetails = outDetails.substr(1);
                }
            } catch (...) {}
        }
    }
}

static int Lua_MessageHandler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg) {
        luaL_traceback(L, L, msg, 1);
    } else {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

static int Lua_GetPosition(lua_State* L) {
    ECS::Entity ent = ScriptEngine::Get().GetCurrentEntity();
    if (ent == ECS::NULL_ENTITY) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }
    auto* node = Scene::SceneManager::Get().GetNodeByEntity(ent);
    if (node) {
        glm::vec3 pos = node->GetLocalPosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3;
    }
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    return 3;
}

static int Lua_SetPosition(lua_State* L) {
    ECS::Entity ent = ScriptEngine::Get().GetCurrentEntity();
    if (ent == ECS::NULL_ENTITY) return 0;
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    double z = luaL_checknumber(L, 3);

    auto* node = Scene::SceneManager::Get().GetNodeByEntity(ent);
    if (node) {
        node->SetLocalPosition(glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
        node->MarkDirty();
    }
    return 0;
}

static int Lua_GetHealth(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::HealthComponent>(entity)) {
        auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
        lua_pushnumber(L, hc.currentHealth);
        return 1;
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

static int Lua_SetHealth(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    double value = luaL_checknumber(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::HealthComponent>(entity)) {
        auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
        float oldHealth = hc.currentHealth;
        hc.currentHealth = std::clamp(static_cast<float>(value), 0.0f, hc.maxHealth);
        if (hc.onHealthChanged) {
            hc.onHealthChanged(oldHealth, hc.currentHealth);
        }
        if (hc.currentHealth <= 0.0f && hc.onDeath) {
            hc.onDeath();
        }
    }
    return 0;
}

static int Lua_GetMaxHealth(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::HealthComponent>(entity)) {
        auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
        lua_pushnumber(L, hc.maxHealth);
        return 1;
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

// --- Gameplay Systems Lua Bindings ---

static int Lua_InventoryAddItem(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 3) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* itemId = luaL_checkstring(L, nextArg);
    uint32_t quantity = static_cast<uint32_t>(luaL_checkinteger(L, nextArg + 1));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool success = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        success = ic.AddItem(itemId, quantity);
    }
    lua_pushboolean(L, success);
    return 1;
}

static int Lua_InventoryRemoveItem(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 3) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* itemId = luaL_checkstring(L, nextArg);
    uint32_t quantity = static_cast<uint32_t>(luaL_checkinteger(L, nextArg + 1));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool success = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        success = ic.RemoveItem(itemId, quantity);
    }
    lua_pushboolean(L, success);
    return 1;
}

static int Lua_InventoryHasItem(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 3) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* itemId = luaL_checkstring(L, nextArg);
    uint32_t quantity = static_cast<uint32_t>(luaL_checkinteger(L, nextArg + 1));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool has = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        has = ic.HasItem(itemId, quantity);
    }
    lua_pushboolean(L, has);
    return 1;
}

static int Lua_InventoryGetItemQuantity(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* itemId = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    uint32_t qty = 0;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        for (const auto& slot : ic.slots) {
            if (slot.itemId == itemId) {
                qty += slot.quantity;
            }
        }
    }
    lua_pushinteger(L, qty);
    return 1;
}

static int Lua_CraftingCanCraft(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* recipeId = luaL_checkstring(L, nextArg);
    const char* stationType = "";
    if (lua_gettop(L) >= nextArg + 1 && lua_isstring(L, nextArg + 1)) {
        stationType = lua_tostring(L, nextArg + 1);
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool can = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        can = Gameplay::CanCraft(recipeId, ic, stationType);
    }
    lua_pushboolean(L, can);
    return 1;
}

static int Lua_CraftingCraft(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* recipeId = luaL_checkstring(L, nextArg);
    const char* stationType = "";
    if (lua_gettop(L) >= nextArg + 1 && lua_isstring(L, nextArg + 1)) {
        stationType = lua_tostring(L, nextArg + 1);
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool ok = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::InventoryComponent>(entity)) {
        auto& ic = registry->GetComponent<Gameplay::InventoryComponent>(entity);
        ok = Gameplay::ExecuteCraft(recipeId, ic, stationType);
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_QuestAccept(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* questId = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool ok = false;
    if (registry && registry->IsAlive(entity)) {
        ok = Gameplay::QuestManager::Get().AcceptQuest(registry, entity, questId);
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_QuestUpdateObjective(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 4) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* type = luaL_checkstring(L, nextArg);
    const char* targetId = luaL_checkstring(L, nextArg + 1);
    int32_t count = static_cast<int32_t>(luaL_checkinteger(L, nextArg + 2));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity)) {
        Gameplay::QuestManager::Get().ProgressObjective(registry, entity, type, targetId, count);
    }
    return 0;
}

static int Lua_QuestIsCompleted(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* questId = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool completed = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::QuestComponent>(entity)) {
        auto& qc = registry->GetComponent<Gameplay::QuestComponent>(entity);
        completed = std::find(qc.completedQuests.begin(), qc.completedQuests.end(), questId) != qc.completedQuests.end();
    }
    lua_pushboolean(L, completed);
    return 1;
}

static int Lua_QuestIsActive(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* questId = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool active = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::QuestComponent>(entity)) {
        auto& qc = registry->GetComponent<Gameplay::QuestComponent>(entity);
        active = qc.activeQuests.find(questId) != qc.activeQuests.end();
    }
    lua_pushboolean(L, active);
    return 1;
}

static int Lua_DialogueStart(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* dialogueId = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool ok = false;
    if (registry && registry->IsAlive(entity)) {
        ok = Gameplay::DialogueDatabase::Get().StartDialogue(registry, entity, dialogueId);
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_DialogueSelectChoice(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    int choiceIdx = static_cast<int>(luaL_checkinteger(L, nextArg));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool ok = false;
    if (registry && registry->IsAlive(entity)) {
        ok = Gameplay::DialogueDatabase::Get().ChooseOption(registry, entity, choiceIdx);
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_DialogueGetCurrentText(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    std::string text = "";
    if (registry && registry->IsAlive(entity)) {
        const auto* node = Gameplay::DialogueDatabase::Get().GetCurrentNode(registry, entity);
        if (node) text = node->text;
    }
    lua_pushstring(L, text.c_str());
    return 1;
}

static int Lua_DialogueIsInDialogue(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool inDialogue = false;
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::DialogueComponent>(entity)) {
        auto& dc = registry->GetComponent<Gameplay::DialogueComponent>(entity);
        inDialogue = dc.isInDialogue;
    }
    lua_pushboolean(L, inDialogue);
    return 1;
}

static int Lua_InteractionInteract(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    int nextArg = 1;
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    ECS::Entity target = static_cast<ECS::Entity>(luaL_checkinteger(L, nextArg));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    bool ok = false;
    if (registry && registry->IsAlive(entity) && registry->IsAlive(target)) {
        ok = Gameplay::InteractionSystem::Get().Interact(registry, entity, target);
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_TakeDamage(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    double amount = luaL_checknumber(L, nextArg);
    std::string damageType = "Physical";
    if (lua_isstring(L, nextArg + 1)) {
        damageType = lua_tostring(L, nextArg + 1);
    }
    ECS::Entity instigator = ECS::NULL_ENTITY;
    if (lua_isinteger(L, nextArg + 2)) {
        instigator = static_cast<ECS::Entity>(lua_tointeger(L, nextArg + 2));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::HealthComponent>(entity)) {
        auto& hc = registry->GetComponent<Gameplay::HealthComponent>(entity);
        if (!hc.invulnerable) {
            float oldHealth = hc.currentHealth;
            float damageDealt = static_cast<float>(amount);
            if (hc.shield > 0.0f) {
                if (hc.shield >= damageDealt) {
                    hc.shield -= damageDealt;
                    damageDealt = 0.0f;
                } else {
                    damageDealt -= hc.shield;
                    hc.shield = 0.0f;
                }
            }
            hc.currentHealth = (std::max)(0.0f, hc.currentHealth - damageDealt);
            if (hc.onHealthChanged) {
                hc.onHealthChanged(oldHealth, hc.currentHealth);
            }
            
            lua_State* LState = ScriptEngine::Get().GetLuaState();
            lua_newtable(LState);
            lua_pushinteger(LState, entity);
            lua_setfield(LState, -2, "target");
            lua_pushnumber(LState, amount);
            lua_setfield(LState, -2, "amount");
            lua_pushstring(LState, damageType.c_str());
            lua_setfield(LState, -2, "type");
            lua_pushinteger(LState, instigator);
            lua_setfield(LState, -2, "instigator");
            int dataRef = luaL_ref(LState, LUA_REGISTRYINDEX);
            
            Core::LuaCustomEvent damEvent("OnDamaged", dataRef);
            Core::EventManager::Get().DispatchEvent(damEvent);
            
            if (hc.currentHealth <= 0.0f) {
                if (hc.onDeath) {
                    hc.onDeath();
                }
                
                lua_newtable(LState);
                lua_pushinteger(LState, entity);
                lua_setfield(LState, -2, "victim");
                lua_pushinteger(LState, instigator);
                lua_setfield(LState, -2, "killer");
                int deathDataRef = luaL_ref(LState, LUA_REGISTRYINDEX);
                
                Core::LuaCustomEvent dEvent("OnDeath", deathDataRef);
                Core::EventManager::Get().DispatchEvent(dEvent);
            }
        }
    }
    return 0;
}

static int Lua_AddTag(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* tagStr = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity)) {
        if (!registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
            registry->AddComponent<Gameplay::GameplayTagsComponent>(entity);
        }
        auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        gtc.AddTag(tagStr);
    }
    return 0;
}

static int Lua_RemoveTag(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* tagStr = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
        auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        gtc.RemoveTag(tagStr);
    }
    return 0;
}

static int Lua_HasTag(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* tagStr = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
        auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        lua_pushboolean(L, gtc.HasTag(tagStr));
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int Lua_HasTagExact(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    const char* tagStr = luaL_checkstring(L, nextArg);
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::GameplayTagsComponent>(entity)) {
        auto& gtc = registry->GetComponent<Gameplay::GameplayTagsComponent>(entity);
        lua_pushboolean(L, gtc.HasTagExact(tagStr));
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int Lua_GetTeam(lua_State* L) {
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1)) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity) && registry->HasComponent<Gameplay::TeamComponent>(entity)) {
        auto& tc = registry->GetComponent<Gameplay::TeamComponent>(entity);
        lua_pushinteger(L, tc.teamId);
        return 1;
    }
    lua_pushinteger(L, -1);
    return 1;
}

static int Lua_SetTeam(lua_State* L) {
    int nextArg = 1;
    ECS::Entity entity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_isinteger(L, 1) && lua_gettop(L) >= 2) {
        entity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
        nextArg = 2;
    }
    int teamId = static_cast<int>(luaL_checkinteger(L, nextArg));
    
    auto* registry = ScriptEngine::Get().GetRegistry();
    if (registry && registry->IsAlive(entity)) {
        if (!registry->HasComponent<Gameplay::TeamComponent>(entity)) {
            registry->AddComponent<Gameplay::TeamComponent>(entity);
        }
        auto& tc = registry->GetComponent<Gameplay::TeamComponent>(entity);
        tc.teamId = teamId;
    }
    return 0;
}

static int Lua_LoadLevelAsync(lua_State* L) {
    const char* levelName = luaL_checkstring(L, 1);
    Gameplay::LevelManager::Get().LoadLevelAsync(levelName);
    return 0;
}

static int Lua_GetGameInstance(lua_State* L) {
    lua_newtable(L);
    lua_pushstring(L, "KumariGameInstance");
    lua_setfield(L, -2, "type");
    return 1;
}

static int Lua_GetWorld(lua_State* L) {
    lua_newtable(L);
    lua_pushstring(L, "KumariWorld");
    lua_setfield(L, -2, "type");
    return 1;
}

bool ScriptEngine::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    HotReloadManager::Get().Initialize(registry);
    
    // Initialize Lua state with our custom allocator
    m_luaState = lua_newstate(LuaAllocator, this);
    if (!m_luaState) {
        Core::Logger::Error("ScriptEngine", "Failed to create Lua state.");
        return false;
    }

    // Open ONLY safe standard libraries (Phase 5: Sandboxing)
    luaL_requiref(m_luaState, "_G", luaopen_base, 1);
    lua_pop(m_luaState, 1);

    luaL_requiref(m_luaState, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(m_luaState, 1);

    luaL_requiref(m_luaState, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(m_luaState, 1);

    luaL_requiref(m_luaState, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(m_luaState, 1);

    luaL_requiref(m_luaState, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(m_luaState, 1);

    luaL_requiref(m_luaState, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(m_luaState, 1);

    // Override base functions for sandboxing
    const char* disallowed_globals[] = {
        "dofile", "loadfile", "load", "loadstring", "collectgarbage", "require"
    };
    for (const char* name : disallowed_globals) {
        lua_pushnil(m_luaState);
        lua_setglobal(m_luaState, name);
    }

    // Register execution limits debug hook
    lua_sethook(m_luaState, LuaHookFunc, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 100);

    // Override the print function (redirect to KumariEngine Logger)
    lua_register(m_luaState, "print", [](lua_State* L) -> int {
        int nargs = lua_gettop(L);
        std::string out;
        for (int i = 1; i <= nargs; ++i) {
            if (i > 1) out += "\t";
            size_t len = 0;
            const char* s = luaL_tolstring(L, i, &len);
            if (s) {
                out.append(s, len);
            }
            lua_pop(L, 1);
        }
        Core::Logger::Info("LuaScript", "%s", out.c_str());
        return 0;
    });

    RegisterAPI();

    Core::Logger::Info("ScriptEngine", "Script Engine initialized successfully.");
    return true;
}

void ScriptEngine::Shutdown() {
    HotReloadManager::Get().Shutdown();
    if (m_luaState) {
        // Clear all script state queued events first to release Lua registry refs
        for (auto& [ref, info] : m_scripts) {
            ClearQueuedEvents(info, m_luaState);
        }
        lua_close(m_luaState);
        m_luaState = nullptr;
    }
    m_registry = nullptr;
    m_entitySubscriptions.clear();
    m_scripts.clear();
    m_totalMemoryAllocated = 0;
    m_activeScript = nullptr;
    Core::Logger::Info("ScriptEngine", "Script Engine shut down.");
}

int Lua_Subscribe(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    ECS::Entity ent = ScriptEngine::Get().GetCurrentEntity();
    Core::Logger::Info("LuaDebug", "Lua_Subscribe called for event '%s', entity %d", eventName, ent);
    if (ent == ECS::NULL_ENTITY) {
        Core::Logger::Warning("ScriptEngine", "Cannot Subscribe: no current entity scope.");
        return 0;
    }

    lua_pushvalue(L, 2);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
    Core::Logger::Info("LuaDebug", "Lua_Subscribe registered callbackRef %d", callbackRef);

    std::string eventNameStr(eventName);
    uint32_t cppSubId = Core::EventManager::Get().Subscribe(eventNameStr, [ent, callbackRef](const Core::Event& ev) {
        ScriptEngine::Get().DispatchToLua(ent, callbackRef, ev);
    });

    ScriptEngine::Get().m_entitySubscriptions[ent].push_back({eventNameStr, callbackRef, cppSubId});
    return 0;
}

int Lua_Unsubscribe(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);
    ECS::Entity ent = ScriptEngine::Get().GetCurrentEntity();
    if (ent == ECS::NULL_ENTITY) return 0;

    std::string eventNameStr(eventName);
    auto& subs = ScriptEngine::Get().m_entitySubscriptions[ent];
    for (auto it = subs.begin(); it != subs.end(); ) {
        if (it->eventName == eventNameStr) {
            Core::EventManager::Get().Unsubscribe(it->eventName, it->cppSubId);
            luaL_unref(L, LUA_REGISTRYINDEX, it->callbackRef);
            it = subs.erase(it);
        } else {
            ++it;
        }
    }
    return 0;
}

static int Lua_Publish(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);

    lua_pushvalue(L, 2);
    int dataRef = luaL_ref(L, LUA_REGISTRYINDEX);
    Core::Logger::Info("LuaDebug", "Lua_Publish called for event '%s', dataRef %d", eventName, dataRef);

    Core::LuaCustomEvent customEvent(eventName, dataRef);
    Core::EventManager::Get().DispatchEvent(customEvent);

    return 0;
}

static int Lua_SendRPC(lua_State* L) {
    int targetPeerId = static_cast<int>(luaL_checkinteger(L, 1));
    std::string name = luaL_checkstring(L, 2);

    Save::EntityGUID entityGuid = Save::NULL_GUID;
    if (!lua_isnil(L, 3)) {
        if (lua_isstring(L, 3)) {
            std::string guidStr = lua_tostring(L, 3);
            if (guidStr.length() == 32) {
                try {
                    entityGuid.high = std::stoull(guidStr.substr(0, 16), nullptr, 16);
                    entityGuid.low = std::stoull(guidStr.substr(16, 16), nullptr, 16);
                } catch (...) {
                    return luaL_error(L, "Invalid EntityGUID string format");
                }
            }
        } else if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "high");
            if (lua_isinteger(L, -1)) entityGuid.high = static_cast<uint64_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, 3, "low");
            if (lua_isinteger(L, -1)) entityGuid.low = static_cast<uint64_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
        }
    }

    bool reliable = lua_toboolean(L, 4) != 0;

    Networking::PacketWriter argsWriter(Networking::PacketId::Invalid);

    int nargs = lua_gettop(L);
    for (int i = 5; i <= nargs; ++i) {
        int type = lua_type(L, i);
        if (type == LUA_TNUMBER) {
            if (lua_isinteger(L, i)) {
                Networking::RPCManager::WriteInt(argsWriter, static_cast<int32_t>(lua_tointeger(L, i)));
            } else {
                Networking::RPCManager::WriteDouble(argsWriter, static_cast<double>(lua_tonumber(L, i)));
            }
        } else if (type == LUA_TBOOLEAN) {
            Networking::RPCManager::WriteBool(argsWriter, lua_toboolean(L, i) != 0);
        } else if (type == LUA_TSTRING) {
            Networking::RPCManager::WriteString(argsWriter, lua_tostring(L, i));
        } else if (type == LUA_TTABLE) {
            lua_getfield(L, i, "x");
            lua_getfield(L, i, "y");
            lua_getfield(L, i, "z");
            if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                glm::vec3 v(static_cast<float>(lua_tonumber(L, -3)),
                            static_cast<float>(lua_tonumber(L, -2)),
                            static_cast<float>(lua_tonumber(L, -1)));
                Networking::RPCManager::WriteVec3(argsWriter, v);
                lua_pop(L, 3);
            } else {
                lua_pop(L, 3);
                lua_getfield(L, i, "high");
                lua_getfield(L, i, "low");
                if (lua_isinteger(L, -2) && lua_isinteger(L, -1)) {
                    Save::EntityGUID g;
                    g.high = static_cast<uint64_t>(lua_tointeger(L, -2));
                    g.low = static_cast<uint64_t>(lua_tointeger(L, -1));
                    Networking::RPCManager::WriteGUID(argsWriter, g);
                } else {
                    lua_pop(L, 2);
                    return luaL_error(L, "Unsupported table parameter passed to SendRPC. Table must be Vec3 {x,y,z} or GUID {high,low}");
                }
                lua_pop(L, 2);
            }
        } else {
            return luaL_error(L, "Unsupported argument type passed to SendRPC.");
        }
    }

    Networking::RPCManager::Get().SendRPC(targetPeerId, name, entityGuid, argsWriter, reliable);
    return 0;
}

static int Lua_BroadcastRPC(lua_State* L) {
    std::string name = luaL_checkstring(L, 1);

    Save::EntityGUID entityGuid = Save::NULL_GUID;
    if (!lua_isnil(L, 2)) {
        if (lua_isstring(L, 2)) {
            std::string guidStr = lua_tostring(L, 2);
            if (guidStr.length() == 32) {
                try {
                    entityGuid.high = std::stoull(guidStr.substr(0, 16), nullptr, 16);
                    entityGuid.low = std::stoull(guidStr.substr(16, 16), nullptr, 16);
                } catch (...) {
                    return luaL_error(L, "Invalid EntityGUID string format");
                }
            }
        } else if (lua_istable(L, 2)) {
            lua_getfield(L, 2, "high");
            if (lua_isinteger(L, -1)) entityGuid.high = static_cast<uint64_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, 2, "low");
            if (lua_isinteger(L, -1)) entityGuid.low = static_cast<uint64_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
        }
    }

    bool reliable = lua_toboolean(L, 3) != 0;

    Networking::PacketWriter argsWriter(Networking::PacketId::Invalid);

    int nargs = lua_gettop(L);
    for (int i = 4; i <= nargs; ++i) {
        int type = lua_type(L, i);
        if (type == LUA_TNUMBER) {
            if (lua_isinteger(L, i)) {
                Networking::RPCManager::WriteInt(argsWriter, static_cast<int32_t>(lua_tointeger(L, i)));
            } else {
                Networking::RPCManager::WriteDouble(argsWriter, static_cast<double>(lua_tonumber(L, i)));
            }
        } else if (type == LUA_TBOOLEAN) {
            Networking::RPCManager::WriteBool(argsWriter, lua_toboolean(L, i) != 0);
        } else if (type == LUA_TSTRING) {
            Networking::RPCManager::WriteString(argsWriter, lua_tostring(L, i));
        } else if (type == LUA_TTABLE) {
            lua_getfield(L, i, "x");
            lua_getfield(L, i, "y");
            lua_getfield(L, i, "z");
            if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                glm::vec3 v(static_cast<float>(lua_tonumber(L, -3)),
                            static_cast<float>(lua_tonumber(L, -2)),
                            static_cast<float>(lua_tonumber(L, -1)));
                Networking::RPCManager::WriteVec3(argsWriter, v);
                lua_pop(L, 3);
            } else {
                lua_pop(L, 3);
                lua_getfield(L, i, "high");
                lua_getfield(L, i, "low");
                if (lua_isinteger(L, -2) && lua_isinteger(L, -1)) {
                    Save::EntityGUID g;
                    g.high = static_cast<uint64_t>(lua_tointeger(L, -2));
                    g.low = static_cast<uint64_t>(lua_tointeger(L, -1));
                    Networking::RPCManager::WriteGUID(argsWriter, g);
                } else {
                    lua_pop(L, 2);
                    return luaL_error(L, "Unsupported table parameter passed to BroadcastRPC.");
                }
                lua_pop(L, 2);
            }
        } else {
            return luaL_error(L, "Unsupported argument type passed to BroadcastRPC.");
        }
    }

    Networking::RPCManager::Get().BroadcastRPC(name, entityGuid, argsWriter, reliable);
    return 0;
}

static int Lua_CameraSetActive(lua_State* L) {
    const char* cameraName = luaL_checkstring(L, 1);
    Camera::CameraManager::Get().SetActiveCamera(cameraName);
    return 0;
}

static int Lua_CameraBlendTo(lua_State* L) {
    const char* cameraName = luaL_checkstring(L, 1);
    float duration = static_cast<float>(luaL_checknumber(L, 2));
    Camera::CameraManager::Get().BlendToCamera(cameraName, duration);
    return 0;
}

static int Lua_CameraStartShake(lua_State* L) {
    const char* cameraName = luaL_checkstring(L, 1);
    float intensity = static_cast<float>(luaL_checknumber(L, 2));
    float duration = static_cast<float>(luaL_checknumber(L, 3));
    float speed = 25.0f;
    if (lua_gettop(L) >= 4) {
        speed = static_cast<float>(luaL_checknumber(L, 4));
    }
    auto cam = Camera::CameraManager::Get().GetCamera(cameraName);
    if (cam) {
        cam->StartShake(intensity, duration, speed);
    }
    return 0;
}

static int Lua_AudioTriggerEvent(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);
    ECS::Entity sourceEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 2 && lua_isinteger(L, 2)) {
        sourceEntity = static_cast<ECS::Entity>(lua_tointeger(L, 2));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        Audio::AudioSystem::Get().TriggerAudioEvent(reg, sourceEntity, eventName);
    }
    return 0;
}

static int Lua_AudioSetMixerVolume(lua_State* L) {
    int channelVal = static_cast<int>(luaL_checkinteger(L, 1));
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    Audio::AudioSystem::Get().GetMixer().SetVolume(static_cast<Audio::MixerChannel>(channelVal), volume);
    return 0;
}

static int Lua_AudioSetMixerMute(lua_State* L) {
    int channelVal = static_cast<int>(luaL_checkinteger(L, 1));
    bool mute = lua_toboolean(L, 2) != 0;
    Audio::AudioSystem::Get().GetMixer().SetMuted(static_cast<Audio::MixerChannel>(channelVal), mute);
    return 0;
}

static int Lua_CinematicPlay(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 2 && lua_isinteger(L, 2)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 2));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        if (!reg->HasComponent<Timeline::CinematicPlayerComponent>(playerEntity)) {
            reg->AddComponent<Timeline::CinematicPlayerComponent>(playerEntity);
        }
        auto& comp = reg->GetComponent<Timeline::CinematicPlayerComponent>(playerEntity);
        comp.timelineName = name;
        Timeline::CinematicSystem::Get().Play(reg, playerEntity);
    }
    return 0;
}

static int Lua_CinematicPause(lua_State* L) {
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        Timeline::CinematicSystem::Get().Pause(reg, playerEntity);
    }
    return 0;
}

static int Lua_CinematicResume(lua_State* L) {
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        Timeline::CinematicSystem::Get().Resume(reg, playerEntity);
    }
    return 0;
}

static int Lua_CinematicStop(lua_State* L) {
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        Timeline::CinematicSystem::Get().Stop(reg, playerEntity);
    }
    return 0;
}

static int Lua_CinematicSkip(lua_State* L) {
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    if (reg) {
        Timeline::CinematicSystem::Get().Skip(reg, playerEntity);
    }
    return 0;
}

static int Lua_CinematicIsPlaying(lua_State* L) {
    ECS::Entity playerEntity = ScriptEngine::Get().GetCurrentEntity();
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        playerEntity = static_cast<ECS::Entity>(lua_tointeger(L, 1));
    }
    auto* reg = ScriptEngine::Get().GetRegistry();
    bool playing = false;
    if (reg && reg->HasComponent<Timeline::CinematicPlayerComponent>(playerEntity)) {
        playing = reg->GetComponent<Timeline::CinematicPlayerComponent>(playerEntity).isPlaying;
    }
    lua_pushboolean(L, playing);
    return 1;
}

static int Lua_RegisterRPC(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushvalue(L, 2);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    Networking::RPCManager::Get().RegisterLuaHandler(name, callbackRef);
    return 0;
}

void ScriptEngine::RegisterAPI() {
    lua_register(m_luaState, "GetPosition", Lua_GetPosition);
    lua_register(m_luaState, "SetPosition", Lua_SetPosition);
    lua_register(m_luaState, "Subscribe", Lua_Subscribe);
    lua_register(m_luaState, "Unsubscribe", Lua_Unsubscribe);
    lua_register(m_luaState, "Publish", Lua_Publish);
    lua_register(m_luaState, "SendRPC", Lua_SendRPC);
    lua_register(m_luaState, "BroadcastRPC", Lua_BroadcastRPC);
    lua_register(m_luaState, "RegisterRPC", Lua_RegisterRPC);
    lua_register(m_luaState, "GetHealth", Lua_GetHealth);
    lua_register(m_luaState, "SetHealth", Lua_SetHealth);
    lua_register(m_luaState, "GetMaxHealth", Lua_GetMaxHealth);
    lua_register(m_luaState, "TakeDamage", Lua_TakeDamage);
    lua_register(m_luaState, "AddTag", Lua_AddTag);
    lua_register(m_luaState, "RemoveTag", Lua_RemoveTag);
    lua_register(m_luaState, "HasTag", Lua_HasTag);
    lua_register(m_luaState, "HasTagExact", Lua_HasTagExact);
    lua_register(m_luaState, "GetTeam", Lua_GetTeam);
    lua_register(m_luaState, "SetTeam", Lua_SetTeam);
    lua_register(m_luaState, "LoadLevelAsync", Lua_LoadLevelAsync);
    lua_register(m_luaState, "GetGameInstance", Lua_GetGameInstance);
    lua_register(m_luaState, "GetWorld", Lua_GetWorld);

    // Audio system Lua APIs
    lua_register(m_luaState, "AudioTriggerEvent", Lua_AudioTriggerEvent);
    lua_register(m_luaState, "AudioSetMixerVolume", Lua_AudioSetMixerVolume);
    lua_register(m_luaState, "AudioSetMixerMute", Lua_AudioSetMixerMute);

    // Camera framework Lua APIs
    lua_register(m_luaState, "CameraSetActive", Lua_CameraSetActive);
    lua_register(m_luaState, "CameraBlendTo", Lua_CameraBlendTo);
    lua_register(m_luaState, "CameraStartShake", Lua_CameraStartShake);

    // Cinematic & Timeline Lua APIs
    lua_register(m_luaState, "CinematicPlay", Lua_CinematicPlay);
    lua_register(m_luaState, "CinematicPause", Lua_CinematicPause);
    lua_register(m_luaState, "CinematicResume", Lua_CinematicResume);
    lua_register(m_luaState, "CinematicStop", Lua_CinematicStop);
    lua_register(m_luaState, "CinematicSkip", Lua_CinematicSkip);
    lua_register(m_luaState, "CinematicIsPlaying", Lua_CinematicIsPlaying);

    // Register gameplay systems APIs
    lua_register(m_luaState, "InventoryAddItem", Lua_InventoryAddItem);
    lua_register(m_luaState, "InventoryRemoveItem", Lua_InventoryRemoveItem);
    lua_register(m_luaState, "InventoryHasItem", Lua_InventoryHasItem);
    lua_register(m_luaState, "InventoryGetItemQuantity", Lua_InventoryGetItemQuantity);
    lua_register(m_luaState, "CraftingCanCraft", Lua_CraftingCanCraft);
    lua_register(m_luaState, "CraftingCraft", Lua_CraftingCraft);
    lua_register(m_luaState, "QuestAccept", Lua_QuestAccept);
    lua_register(m_luaState, "QuestUpdateObjective", Lua_QuestUpdateObjective);
    lua_register(m_luaState, "QuestIsCompleted", Lua_QuestIsCompleted);
    lua_register(m_luaState, "QuestIsActive", Lua_QuestIsActive);
    lua_register(m_luaState, "DialogueStart", Lua_DialogueStart);
    lua_register(m_luaState, "DialogueSelectChoice", Lua_DialogueSelectChoice);
    lua_register(m_luaState, "DialogueGetCurrentText", Lua_DialogueGetCurrentText);
    lua_register(m_luaState, "DialogueIsInDialogue", Lua_DialogueIsInDialogue);
    lua_register(m_luaState, "InteractionInteract", Lua_InteractionInteract);

    // Register developer utilities to Lua
    lua_register(m_luaState, "ListScripts", [](lua_State* L) -> int {
        auto list = ScriptEngine::Get().GetLoadedScripts();
        lua_newtable(L);
        for (size_t i = 0; i < list.size(); ++i) {
            lua_pushstring(L, list[i].c_str());
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    });

    lua_register(m_luaState, "ReloadScripts", [](lua_State* L) -> int {
        (void)L;
        ScriptEngine::Get().ReloadAllScripts();
        return 0;
    });

    lua_register(m_luaState, "EnableScript", [](lua_State* L) -> int {
        if (lua_isinteger(L, 1)) {
            int envRef = static_cast<int>(lua_tointeger(L, 1));
            bool enabled = lua_toboolean(L, 2) != 0;
            ScriptEngine::Get().SetScriptEnabled(envRef, enabled);
        } else if (lua_isstring(L, 1)) {
            std::string path = lua_tostring(L, 1);
            bool enabled = lua_toboolean(L, 2) != 0;
            ScriptEngine::Get().SetScriptEnabled(path, enabled);
        }
        return 0;
    });

    lua_register(m_luaState, "PrintStats", [](lua_State* L) -> int {
        (void)L;
        std::string stats = ScriptEngine::Get().GetActiveScriptStats();
        Core::Logger::Info("ScriptEngine", "\n%s", stats.c_str());
        return 0;
    });

    lua_register(m_luaState, "DumpProfiler", [](lua_State* L) -> int {
        (void)L;
        std::string prof = ScriptEngine::Get().DumpProfilerInfo();
        Core::Logger::Info("ScriptEngine", "\n%s", prof.c_str());
        return 0;
    });
}

int ScriptEngine::LoadScript(ECS::Entity entity, const std::string& scriptPath) {
    if (!m_luaState) return -1;

    ScriptInfo info;
    info.scriptPath = scriptPath;
    info.entity = entity;
    info.maxMemory = m_defaultMaxMemory;
    info.maxExecutionTime = m_defaultMaxExecutionTime;
    info.maxRecursionDepth = m_defaultMaxRecursionDepth;
    info.maxEventQueueSize = m_defaultMaxEventQueueSize;

    int errIdx = lua_gettop(m_luaState) + 1;
    lua_pushcfunction(m_luaState, Lua_MessageHandler);

    int status = luaL_loadfile(m_luaState, scriptPath.c_str());
    if (status != LUA_OK) {
        std::string err = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 2);
        Core::Logger::Error("ScriptEngine", "Failed to load script file '%s': %s", scriptPath.c_str(), err.c_str());
        return -1;
    }

    lua_newtable(m_luaState);

    lua_pushinteger(m_luaState, entity);
    lua_setfield(m_luaState, -2, "entity");

    lua_newtable(m_luaState);
    lua_pushglobaltable(m_luaState);
    lua_setfield(m_luaState, -2, "__index");
    lua_setmetatable(m_luaState, -2);

    lua_pushvalue(m_luaState, -1);
    lua_setupvalue(m_luaState, errIdx + 1, 1);

    ActiveScriptScope activeScope(&info);
    ScriptProfileScope profileScope(&info);
    m_frameStartTime = std::chrono::high_resolution_clock::now();

    lua_pushvalue(m_luaState, errIdx + 1);

    if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
        std::string err = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 3);
        lua_remove(m_luaState, errIdx);
        Core::Logger::Error("ScriptEngine", "Error executing script init chunk '%s': %s", scriptPath.c_str(), err.c_str());
        return -1;
    }

    lua_getfield(m_luaState, -1, "updateOrder");
    if (lua_isinteger(m_luaState, -1)) {
        info.updateOrder = static_cast<int>(lua_tointeger(m_luaState, -1));
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "paused");
    if (lua_isboolean(m_luaState, -1)) {
        info.paused = lua_toboolean(m_luaState, -1) != 0;
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "oneShot");
    if (lua_isboolean(m_luaState, -1)) {
        info.isOneShot = lua_toboolean(m_luaState, -1) != 0;
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "maxMemory");
    if (lua_isinteger(m_luaState, -1)) {
        info.maxMemory = static_cast<size_t>(lua_tointeger(m_luaState, -1));
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "maxExecutionTime");
    if (lua_isnumber(m_luaState, -1)) {
        info.maxExecutionTime = static_cast<float>(lua_tonumber(m_luaState, -1));
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "maxRecursionDepth");
    if (lua_isinteger(m_luaState, -1)) {
        info.maxRecursionDepth = static_cast<int>(lua_tointeger(m_luaState, -1));
    }
    lua_pop(m_luaState, 1);

    lua_getfield(m_luaState, -1, "maxEventQueueSize");
    if (lua_isinteger(m_luaState, -1)) {
        info.maxEventQueueSize = static_cast<size_t>(lua_tointeger(m_luaState, -1));
    }
    lua_pop(m_luaState, 1);

    int envRef = luaL_ref(m_luaState, LUA_REGISTRYINDEX);
    
    info.envRef = envRef;
    m_scripts[envRef] = info;

    lua_pop(m_luaState, 1); // pop chunk
    lua_remove(m_luaState, errIdx); // remove handler

    HotReloadManager::Get().WatchScript(scriptPath);
    return envRef;
}

void ScriptEngine::UnloadScript(int envRef) {
    if (m_luaState && envRef != -1) {
        lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, envRef);
        ECS::Entity entity = ECS::NULL_ENTITY;
        if (lua_istable(m_luaState, -1)) {
            lua_getfield(m_luaState, -1, "entity");
            if (lua_isinteger(m_luaState, -1)) {
                entity = static_cast<ECS::Entity>(lua_tointeger(m_luaState, -1));
            }
            lua_pop(m_luaState, 1);
        }
        lua_pop(m_luaState, 1);

        if (entity != ECS::NULL_ENTITY) {
            auto it = m_entitySubscriptions.find(entity);
            if (it != m_entitySubscriptions.end()) {
                for (const auto& sub : it->second) {
                    Core::EventManager::Get().Unsubscribe(sub.eventName, sub.cppSubId);
                    luaL_unref(m_luaState, LUA_REGISTRYINDEX, sub.callbackRef);
                }
                m_entitySubscriptions.erase(it);
            }
        }

        auto itScript = m_scripts.find(envRef);
        if (itScript != m_scripts.end()) {
            ClearQueuedEvents(itScript->second, m_luaState);
            m_scripts.erase(itScript);
        }

        luaL_unref(m_luaState, LUA_REGISTRYINDEX, envRef);
    }
}

bool ScriptEngine::PrepareCall(int envRef, const std::string& funcName) {
    if (!m_luaState || envRef == -1) return false;
    
    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, envRef);
    if (!lua_istable(m_luaState, -1)) {
        lua_pop(m_luaState, 1);
        return false;
    }

    lua_getfield(m_luaState, -1, funcName.c_str());
    if (!lua_isfunction(m_luaState, -1)) {
        lua_pop(m_luaState, 2);
        return false;
    }

    lua_remove(m_luaState, -2);
    return true;
}

void ScriptEngine::OnCreateEntity(ECS::Entity entity) {
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.initialized) return;

    if (sc.envRef == -1) {
        sc.envRef = LoadScript(entity, sc.scriptPath);
    }

    if (sc.envRef != -1) {
        auto it = m_scripts.find(sc.envRef);
        if (it != m_scripts.end() && it->second.enabled) {
            auto& info = it->second;
            CurrentEntityScope scope(entity);
            ActiveScriptScope activeScope(&info);
            int errIdx = lua_gettop(m_luaState) + 1;
            lua_pushcfunction(m_luaState, Lua_MessageHandler);

            if (PrepareCall(sc.envRef, "OnCreate")) {
                ScriptProfileScope profileScope(&info);
                m_frameStartTime = std::chrono::high_resolution_clock::now();
                if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
                    std::string err = lua_tostring(m_luaState, -1);
                    lua_pop(m_luaState, 1);
                    HandleScriptError(sc.envRef, entity, "OnCreate", err);
                }
                lua_remove(m_luaState, errIdx);
            } else {
                lua_pop(m_luaState, 1);
            }
        }
        sc.initialized = true;
    }
}

void ScriptEngine::OnUpdateEntity(ECS::Entity entity, float dt) {
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.envRef == -1) return;

    if (!sc.initialized) {
        OnCreateEntity(entity);
    }

    auto it = m_scripts.find(sc.envRef);
    if (it != m_scripts.end() && it->second.enabled && !it->second.paused) {
        auto& info = it->second;
        CurrentEntityScope scope(entity);
        ActiveScriptScope activeScope(&info);
        int errIdx = lua_gettop(m_luaState) + 1;
        lua_pushcfunction(m_luaState, Lua_MessageHandler);

        if (PrepareCall(sc.envRef, "OnUpdate")) {
            lua_pushnumber(m_luaState, dt);
            ScriptProfileScope profileScope(&info);
            m_frameStartTime = std::chrono::high_resolution_clock::now();
            if (lua_pcall(m_luaState, 1, 0, errIdx) != LUA_OK) {
                std::string err = lua_tostring(m_luaState, -1);
                lua_pop(m_luaState, 1);
                HandleScriptError(sc.envRef, entity, "OnUpdate", err);
            }
            lua_remove(m_luaState, errIdx);
        } else {
            lua_pop(m_luaState, 1);
        }
    }
}

void ScriptEngine::OnDestroyEntity(ECS::Entity entity) {
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.envRef == -1) return;

    if (sc.initialized) {
        auto it = m_scripts.find(sc.envRef);
        if (it != m_scripts.end() && it->second.enabled) {
            auto& info = it->second;
            CurrentEntityScope scope(entity);
            ActiveScriptScope activeScope(&info);
            int errIdx = lua_gettop(m_luaState) + 1;
            lua_pushcfunction(m_luaState, Lua_MessageHandler);

            if (PrepareCall(sc.envRef, "OnDestroy")) {
                ScriptProfileScope profileScope(&info);
                m_frameStartTime = std::chrono::high_resolution_clock::now();
                if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
                    std::string err = lua_tostring(m_luaState, -1);
                    lua_pop(m_luaState, 1);
                    HandleScriptError(sc.envRef, entity, "OnDestroy", err);
                }
                lua_remove(m_luaState, errIdx);
            } else {
                lua_pop(m_luaState, 1);
            }
        }
    }

    UnloadScript(sc.envRef);
    sc.envRef = -1;
    sc.initialized = false;
}

void ScriptEngine::TriggerTimelineEvent(ECS::Entity entity, const std::string& eventName, float eventValue) {
    if (!m_luaState) return;
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.envRef == -1) return;

    auto it = m_scripts.find(sc.envRef);
    if (it == m_scripts.end()) return;

    auto& info = it->second;
    if (!info.enabled || info.paused) return;

    CurrentEntityScope scope(entity);
    ActiveScriptScope activeScope(&info);
    int errIdx = lua_gettop(m_luaState) + 1;
    lua_pushcfunction(m_luaState, Lua_MessageHandler);

    if (PrepareCall(sc.envRef, "OnTimelineEvent")) {
        lua_pushstring(m_luaState, eventName.c_str());
        lua_pushnumber(m_luaState, eventValue);
        ScriptProfileScope profileScope(&info);
        m_frameStartTime = std::chrono::high_resolution_clock::now();
        if (lua_pcall(m_luaState, 2, 0, errIdx) != LUA_OK) {
            std::string err = lua_tostring(m_luaState, -1);
            lua_pop(m_luaState, 1);
            HandleScriptError(sc.envRef, entity, "OnTimelineEvent", err);
        }
        lua_remove(m_luaState, errIdx);
    } else {
        lua_pop(m_luaState, 1);
    }
}

void ScriptEngine::DispatchToLua(ECS::Entity entity, int callbackRef, const Core::Event& event) {
    if (!m_luaState || callbackRef == -1) return;

    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.envRef == -1) return;

    auto it = m_scripts.find(sc.envRef);
    if (it == m_scripts.end()) return;

    auto& info = it->second;
    if (!info.enabled) return;

    // Check event queue/recursion limit
    if (info.queuedEvents.size() >= info.maxEventQueueSize) {
        Core::Logger::Warning("ScriptEngine", "Script '%s' exceeded max event queue/dispatch limit of %zu. Suspending script.",
                              info.scriptPath.c_str(), info.maxEventQueueSize);
        info.enabled = false;
        sc.initialized = false;
        ClearQueuedEvents(info, m_luaState);
        return;
    }

    // Push a dummy queued event to track recursion level/depth
    QueuedEvent dummy;
    dummy.callbackRef = callbackRef;
    dummy.eventType = event.GetType();
    info.queuedEvents.push_back(dummy);

    ActiveScriptScope activeScope(&info);
    CurrentEntityScope entityScope(entity);
    ScriptProfileScope eventProfileScope(&info);

    int errIdx = lua_gettop(m_luaState) + 1;
    lua_pushcfunction(m_luaState, Lua_MessageHandler);

    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, callbackRef);
    if (lua_isfunction(m_luaState, -1)) {
        int numArgs = 0;
        std::string type = event.GetType();

        if (type == "OnKeyPressed" || type == "OnKeyReleased") {
            const auto& keyEvent = static_cast<const Core::KeyEvent&>(event);
            lua_pushinteger(m_luaState, keyEvent.GetKey());
            numArgs = 1;
        }
        else if (type == "OnMouseButtonPressed" || type == "OnMouseButtonReleased") {
            const auto& mouseBtnEvent = static_cast<const Core::MouseButtonEvent&>(event);
            lua_pushinteger(m_luaState, mouseBtnEvent.GetButton());
            numArgs = 1;
        }
        else if (type == "OnMouseMoved") {
            const auto& mouseMovedEvent = static_cast<const Core::MouseMovedEvent&>(event);
            lua_pushnumber(m_luaState, mouseMovedEvent.GetX());
            lua_pushnumber(m_luaState, mouseMovedEvent.GetY());
            numArgs = 2;
        }
        else if (type == "OnCollisionEnter" || type == "OnCollisionStay") {
            const auto& colEvent = static_cast<const Core::CollisionEvent&>(event);
            lua_pushinteger(m_luaState, colEvent.GetEntityA());
            lua_pushinteger(m_luaState, colEvent.GetEntityB());
            lua_pushnumber(m_luaState, colEvent.GetNormal().x);
            lua_pushnumber(m_luaState, colEvent.GetNormal().y);
            lua_pushnumber(m_luaState, colEvent.GetNormal().z);
            lua_pushnumber(m_luaState, colEvent.GetPenetration());
            numArgs = 6;
        }
        else if (type == "OnCollisionExit") {
            const auto& colExitEvent = static_cast<const Core::CollisionExitEvent&>(event);
            lua_pushinteger(m_luaState, colExitEvent.GetEntityA());
            lua_pushinteger(m_luaState, colExitEvent.GetEntityB());
            numArgs = 2;
        }
        else if (type == "OnTriggerEnter" || type == "OnTriggerExit") {
            const auto& trigEvent = static_cast<const Core::TriggerEvent&>(event);
            lua_pushinteger(m_luaState, trigEvent.GetTriggerEntity());
            lua_pushinteger(m_luaState, trigEvent.GetOtherEntity());
            numArgs = 2;
        }
        else {
            const auto& customEvent = static_cast<const Core::LuaCustomEvent&>(event);
            lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, customEvent.GetDataRef());
            numArgs = 1;
        }

        m_frameStartTime = std::chrono::high_resolution_clock::now();
        if (lua_pcall(m_luaState, numArgs, 0, errIdx) != LUA_OK) {
            std::string err = lua_tostring(m_luaState, -1);
            lua_pop(m_luaState, 1);
            HandleScriptError(sc.envRef, entity, "Event: " + type, err);
        }
    } else {
        lua_pop(m_luaState, 1);
    }

    lua_remove(m_luaState, errIdx);
}

void ScriptEngine::Update(float dt) {
    if (!m_registry) return;

    Core::EventManager::Get().ProcessQueue();
    HotReloadManager::Get().Update(dt);

    std::vector<std::pair<ECS::Entity, int>> activeScriptRefs;
    m_registry->Each<ECS::ScriptComponent>([&](ECS::Entity entity, ECS::ScriptComponent& sc) {
        if (sc.envRef != -1) {
            auto it = m_scripts.find(sc.envRef);
            if (it != m_scripts.end() && it->second.enabled) {
                activeScriptRefs.push_back({entity, sc.envRef});
            }
        }
    });

    std::sort(activeScriptRefs.begin(), activeScriptRefs.end(), [&](const auto& a, const auto& b) {
        auto itA = m_scripts.find(a.second);
        auto itB = m_scripts.find(b.second);
        int orderA = (itA != m_scripts.end()) ? itA->second.updateOrder : 0;
        int orderB = (itB != m_scripts.end()) ? itB->second.updateOrder : 0;
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return a.first < b.first;
    });

    for (const auto& [entity, envRef] : activeScriptRefs) {
        auto it = m_scripts.find(envRef);
        if (it == m_scripts.end() || !it->second.enabled) continue;

        auto& info = it->second;
        auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);

        ActiveScriptScope activeScope(&info);
        int errIdx = lua_gettop(m_luaState) + 1;
        lua_pushcfunction(m_luaState, Lua_MessageHandler);

        if (info.enabled && !info.paused) {
            if (!sc.initialized) {
                if (PrepareCall(envRef, "OnCreate")) {
                    ScriptProfileScope onCreateProfileScope(&info);
                    m_frameStartTime = std::chrono::high_resolution_clock::now();
                    if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
                        std::string err = lua_tostring(m_luaState, -1);
                        lua_pop(m_luaState, 1);
                        HandleScriptError(envRef, entity, "OnCreate", err);
                    }
                }
                sc.initialized = true;
            }

            if (info.enabled && !info.paused) {
                if (PrepareCall(envRef, "OnUpdate")) {
                    lua_pushnumber(m_luaState, dt);
                    ScriptProfileScope onUpdateProfileScope(&info);
                    m_frameStartTime = std::chrono::high_resolution_clock::now();
                    if (lua_pcall(m_luaState, 1, 0, errIdx) != LUA_OK) {
                        std::string err = lua_tostring(m_luaState, -1);
                        lua_pop(m_luaState, 1);
                        HandleScriptError(envRef, entity, "OnUpdate", err);
                    }
                }
            }

            if (info.enabled && info.isOneShot) {
                info.enabled = false;
                sc.initialized = false;
            }
        }

        lua_remove(m_luaState, errIdx);
    }

    for (auto& [ref, info] : m_scripts) {
        ClearQueuedEvents(info, m_luaState);
    }
}

bool ScriptEngine::SerializeScriptState(ECS::Entity entity, Save::BinaryWriter& writer) const {
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
        return false;
    }
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
    if (sc.envRef == -1 || !m_luaState) {
        writer.WriteUint32(0);
        return true;
    }

    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, sc.envRef);
    if (!lua_istable(m_luaState, -1)) {
        lua_pop(m_luaState, 1);
        writer.WriteUint32(0);
        return true;
    }

    struct VarData {
        std::string key;
        uint8_t type;
        bool boolVal = false;
        int64_t intVal = 0;
        double doubleVal = 0.0;
        std::string strVal;
        Save::EntityGUID guidVal;
    };
    std::vector<VarData> vars;

    lua_pushnil(m_luaState);
    while (lua_next(m_luaState, -2) != 0) {
        if (lua_type(m_luaState, -2) == LUA_TSTRING) {
            std::string key = lua_tostring(m_luaState, -2);
            if (key != "entity" && key != "updateOrder" && key != "paused" && key != "oneShot" &&
                key != "maxMemory" && key != "maxExecutionTime" && key != "maxRecursionDepth" && key != "maxEventQueueSize" &&
                !key.empty() && key[0] != '_') {
                int valType = lua_type(m_luaState, -1);
                VarData var;
                var.key = key;
                bool supported = false;

                if (valType == LUA_TBOOLEAN) {
                    var.type = 1;
                    var.boolVal = (lua_toboolean(m_luaState, -1) != 0);
                    supported = true;
                } else if (valType == LUA_TNUMBER) {
                    if (lua_isinteger(m_luaState, -1)) {
                        int64_t intVal = lua_tointeger(m_luaState, -1);
                        bool isEntityRef = false;
                        if (intVal > 0 && intVal < static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                            ECS::Entity referencedEnt = static_cast<ECS::Entity>(intVal);
                            if (m_registry->IsAlive(referencedEnt)) {
                                Save::EntityGUID guid = m_registry->GetGUID(referencedEnt);
                                if (!guid.IsNull()) {
                                    var.type = 5;
                                    var.guidVal = guid;
                                    isEntityRef = true;
                                    supported = true;
                                }
                            }
                        }
                        if (!isEntityRef) {
                            var.type = 2;
                            var.intVal = intVal;
                            supported = true;
                        }
                    } else {
                        var.type = 3;
                        var.doubleVal = lua_tonumber(m_luaState, -1);
                        supported = true;
                    }
                } else if (valType == LUA_TSTRING) {
                    var.type = 4;
                    var.strVal = lua_tostring(m_luaState, -1);
                    supported = true;
                }

                if (supported) {
                    vars.push_back(var);
                }
            }
        }
        lua_pop(m_luaState, 1);
    }
    lua_pop(m_luaState, 1);

    writer.WriteUint32(static_cast<uint32_t>(vars.size()));
    for (const auto& var : vars) {
        writer.WriteString(var.key);
        writer.WriteUint8(var.type);
        if (var.type == 1) {
            writer.WriteBool(var.boolVal);
        } else if (var.type == 2) {
            writer.WriteInt64(var.intVal);
        } else if (var.type == 3) {
            writer.WriteDouble(var.doubleVal);
        } else if (var.type == 4) {
            writer.WriteString(var.strVal);
        } else if (var.type == 5) {
            writer.WriteUint64(var.guidVal.high);
            writer.WriteUint64(var.guidVal.low);
        }
    }
    return true;
}

bool ScriptEngine::DeserializeScriptState(ECS::Entity entity, Save::BinaryReader& reader) {
    if (!m_registry || !m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
        return false;
    }
    auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);

    if (sc.envRef == -1) {
        sc.envRef = LoadScript(entity, sc.scriptPath);
    }
    if (sc.envRef == -1) {
        Core::Logger::Error("ScriptEngine", "Failed to load script '%s' for entity %d during restore.", sc.scriptPath.c_str(), entity);
        return false;
    }

    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, sc.envRef);
    if (!lua_istable(m_luaState, -1)) {
        lua_pop(m_luaState, 1);
        return false;
    }

    uint32_t numVars = 0;
    if (!reader.ReadUint32(numVars)) {
        lua_pop(m_luaState, 1);
        return false;
    }

    for (uint32_t i = 0; i < numVars; ++i) {
        std::string key;
        uint8_t type = 0;
        if (!reader.ReadString(key) || !reader.ReadUint8(type)) {
            lua_pop(m_luaState, 1);
            return false;
        }

        if (type == 1) {
            bool boolVal = false;
            if (!reader.ReadBool(boolVal)) {
                lua_pop(m_luaState, 1);
                return false;
            }
            lua_pushboolean(m_luaState, boolVal);
            lua_setfield(m_luaState, -2, key.c_str());
        } else if (type == 2) {
            int64_t intVal = 0;
            if (!reader.ReadInt64(intVal)) {
                lua_pop(m_luaState, 1);
                return false;
            }
            lua_pushinteger(m_luaState, intVal);
            lua_setfield(m_luaState, -2, key.c_str());
        } else if (type == 3) {
            double doubleVal = 0.0;
            if (!reader.ReadDouble(doubleVal)) {
                lua_pop(m_luaState, 1);
                return false;
            }
            lua_pushnumber(m_luaState, doubleVal);
            lua_setfield(m_luaState, -2, key.c_str());
        } else if (type == 4) {
            std::string strVal;
            if (!reader.ReadString(strVal)) {
                lua_pop(m_luaState, 1);
                return false;
            }
            lua_pushlstring(m_luaState, strVal.data(), strVal.size());
            lua_setfield(m_luaState, -2, key.c_str());
        } else if (type == 5) {
            Save::EntityGUID guidVal;
            if (!reader.ReadUint64(guidVal.high) || !reader.ReadUint64(guidVal.low)) {
                lua_pop(m_luaState, 1);
                return false;
            }
            ECS::Entity reboundEnt = m_registry->GetEntityByGUID(guidVal);
            lua_pushinteger(m_luaState, reboundEnt);
            lua_setfield(m_luaState, -2, key.c_str());
        } else {
            lua_pop(m_luaState, 1);
            return false;
        }
    }

    lua_pop(m_luaState, 1);
    return true;
}

std::unordered_map<std::string, ScriptEngine::PrimitiveValue> ScriptEngine::SaveEnvironment(int envRef) {
    std::unordered_map<std::string, PrimitiveValue> savedState;
    if (!m_luaState || envRef == -1) return savedState;

    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, envRef);
    if (lua_istable(m_luaState, -1)) {
        lua_pushnil(m_luaState);
        while (lua_next(m_luaState, -2) != 0) {
            if (lua_type(m_luaState, -2) == LUA_TSTRING) {
                std::string key = lua_tostring(m_luaState, -2);
                if (key != "entity" && key != "updateOrder" && key != "paused" && key != "oneShot" &&
                    key != "maxMemory" && key != "maxExecutionTime" && key != "maxRecursionDepth" && key != "maxEventQueueSize" &&
                    !key.empty() && key[0] != '_') {
                    int valType = lua_type(m_luaState, -1);
                    PrimitiveValue var;
                    bool supported = false;

                    if (valType == LUA_TBOOLEAN) {
                        var.type = PrimitiveValue::Type::Bool;
                        var.boolVal = (lua_toboolean(m_luaState, -1) != 0);
                        supported = true;
                    } else if (valType == LUA_TNUMBER) {
                        if (lua_isinteger(m_luaState, -1)) {
                            int64_t intVal = lua_tointeger(m_luaState, -1);
                            bool isEntityRef = false;
                            if (intVal > 0 && intVal < static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                                ECS::Entity referencedEnt = static_cast<ECS::Entity>(intVal);
                                if (m_registry && m_registry->IsAlive(referencedEnt)) {
                                    var.type = PrimitiveValue::Type::Entity;
                                    var.entityVal = referencedEnt;
                                    isEntityRef = true;
                                    supported = true;
                                }
                            }
                            if (!isEntityRef) {
                                var.type = PrimitiveValue::Type::Integer;
                                var.intVal = intVal;
                                supported = true;
                            }
                        } else {
                            var.type = PrimitiveValue::Type::Double;
                            var.doubleVal = lua_tonumber(m_luaState, -1);
                            supported = true;
                        }
                    } else if (valType == LUA_TSTRING) {
                        var.type = PrimitiveValue::Type::String;
                        var.strVal = lua_tostring(m_luaState, -1);
                        supported = true;
                    }

                    if (supported) {
                        savedState[key] = var;
                    }
                }
            }
            lua_pop(m_luaState, 1);
        }
    }
    lua_pop(m_luaState, 1);
    return savedState;
}

void ScriptEngine::RestoreEnvironment(int envRef, const std::unordered_map<std::string, PrimitiveValue>& state) {
    if (!m_luaState || envRef == -1) return;

    lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, envRef);
    if (lua_istable(m_luaState, -1)) {
        for (const auto& [key, var] : state) {
            switch (var.type) {
                case PrimitiveValue::Type::Bool:
                    lua_pushboolean(m_luaState, var.boolVal);
                    break;
                case PrimitiveValue::Type::Integer:
                    lua_pushinteger(m_luaState, var.intVal);
                    break;
                case PrimitiveValue::Type::Double:
                    lua_pushnumber(m_luaState, var.doubleVal);
                    break;
                case PrimitiveValue::Type::String:
                    lua_pushlstring(m_luaState, var.strVal.data(), var.strVal.size());
                    break;
                case PrimitiveValue::Type::Entity:
                    lua_pushinteger(m_luaState, static_cast<lua_Integer>(var.entityVal));
                    break;
            }
            lua_setfield(m_luaState, -2, key.c_str());
        }
    }
    lua_pop(m_luaState, 1);
}

bool ScriptEngine::ReloadScript(const std::string& scriptPath) {
    if (!m_luaState || !m_registry) return false;

    auto startTime = std::chrono::high_resolution_clock::now();

    int errIdx = lua_gettop(m_luaState) + 1;
    lua_pushcfunction(m_luaState, Lua_MessageHandler);

    int status = luaL_loadfile(m_luaState, scriptPath.c_str());
    if (status != LUA_OK) {
        std::string err = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 2);
        
        Core::Logger::Error("ScriptEngine", "Hot reload compilation failed for '%s': %s", scriptPath.c_str(), err.c_str());
        return false;
    }

    int compiledChunkStackIndex = lua_gettop(m_luaState);

    std::vector<ECS::Entity> affectedEntities;
    std::error_code ec;
    auto normPath = std::filesystem::absolute(scriptPath, ec);
    if (!ec) {
        normPath = normPath.lexically_normal();
    }
    std::string normPathStr = ec ? scriptPath : normPath.string();

    m_registry->Each<ECS::ScriptComponent>([&](ECS::Entity entity, ECS::ScriptComponent& sc) {
        std::error_code componentEc;
        auto componentNormPath = std::filesystem::absolute(sc.scriptPath, componentEc);
        if (!componentEc) {
            componentNormPath = componentNormPath.lexically_normal();
        }
        std::string componentPathStr = componentEc ? sc.scriptPath : componentNormPath.string();

        if (componentPathStr == normPathStr) {
            affectedEntities.push_back(entity);
        }
    });

    if (affectedEntities.empty()) {
        lua_pop(m_luaState, 2); // pop chunk and handler
        return true;
    }

    bool overallSuccess = true;

    for (auto entity : affectedEntities) {
        auto& sc = m_registry->GetComponent<ECS::ScriptComponent>(entity);
        
        if (sc.envRef == -1) {
            sc.envRef = LoadScript(entity, sc.scriptPath);
            if (sc.envRef != -1) {
                sc.initialized = true;
            } else {
                overallSuccess = false;
            }
            continue;
        }

        std::unordered_map<std::string, PrimitiveValue> savedState = SaveEnvironment(sc.envRef);

        auto subIt = m_entitySubscriptions.find(entity);
        if (subIt != m_entitySubscriptions.end()) {
            for (const auto& sub : subIt->second) {
                Core::EventManager::Get().Unsubscribe(sub.eventName, sub.cppSubId);
                luaL_unref(m_luaState, LUA_REGISTRYINDEX, sub.callbackRef);
            }
            m_entitySubscriptions.erase(subIt);
        }

        auto itScript = m_scripts.find(sc.envRef);
        if (itScript != m_scripts.end()) {
            ClearQueuedEvents(itScript->second, m_luaState);
        }

        lua_pushvalue(m_luaState, compiledChunkStackIndex);
        lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, sc.envRef);
        lua_pushvalue(m_luaState, -1);
        lua_setupvalue(m_luaState, -3, 1);
        lua_pop(m_luaState, 1);

        ScriptInfo* info = nullptr;
        if (itScript != m_scripts.end()) {
            info = &itScript->second;
        }

        CurrentEntityScope scope(entity);
        ActiveScriptScope activeScope(info);
        ScriptProfileScope profileScope(info);
        m_frameStartTime = std::chrono::high_resolution_clock::now();

        if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
            std::string err = lua_tostring(m_luaState, -1);
            lua_pop(m_luaState, 1);
            
            HandleScriptError(sc.envRef, entity, "HotReload Init", err);
            
            UnloadScript(sc.envRef);
            sc.envRef = -1;
            sc.initialized = false;
            overallSuccess = false;
            continue;
        }

        bool onCreateSuccess = true;
        if (sc.initialized) {
            if (PrepareCall(sc.envRef, "OnCreate")) {
                if (lua_pcall(m_luaState, 0, 0, errIdx) != LUA_OK) {
                    std::string err = lua_tostring(m_luaState, -1);
                    lua_pop(m_luaState, 1);
                    HandleScriptError(sc.envRef, entity, "HotReload OnCreate", err);
                    onCreateSuccess = false;
                }
            }
        }

        if (!onCreateSuccess) {
            UnloadScript(sc.envRef);
            sc.envRef = -1;
            sc.initialized = false;
            overallSuccess = false;
            continue;
        }

        RestoreEnvironment(sc.envRef, savedState);
    }

    lua_pop(m_luaState, 1); // pop chunk
    lua_remove(m_luaState, errIdx); // remove handler

    if (overallSuccess) {
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
        Core::Logger::Info("ScriptEngine", "Successfully reloaded script '%s' in %.3f ms for %d entity instances.",
                           scriptPath.c_str(), elapsed.count(), static_cast<int>(affectedEntities.size()));
    }

    return overallSuccess;
}

void ScriptEngine::HandleScriptError(int envRef, ECS::Entity entity, const std::string& stage, const std::string& rawError) {
    std::string filename;
    int lineNum = 0;
    std::string details;
    ParseLuaError(rawError, filename, lineNum, details);

    auto it = m_scripts.find(envRef);
    if (filename.empty() && it != m_scripts.end()) {
        filename = it->second.scriptPath;
    }

    std::string guidStr = "NULL";
    if (m_registry && entity != ECS::NULL_ENTITY) {
        auto guid = m_registry->GetGUID(entity);
        if (!guid.IsNull()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%016llX%016llX", static_cast<unsigned long long>(guid.high), static_cast<unsigned long long>(guid.low));
            guidStr = buf;
        }
    }

    Core::Logger::Error("ScriptEngine", "Runtime Error in %s | Script: %s | Entity: %u (GUID: %s) | Line: %d\nError: %s\nStack Trace:\n%s",
                        stage.c_str(), filename.c_str(), entity, guidStr.c_str(), lineNum, details.c_str(), rawError.c_str());

    if (it != m_scripts.end()) {
        bool isLimitViolation = (rawError.find("timeout") != std::string::npos ||
                                 rawError.find("recursion") != std::string::npos ||
                                 rawError.find("memory") != std::string::npos);
        if (isLimitViolation) {
            it->second.enabled = false;
            if (m_registry && m_registry->HasComponent<ECS::ScriptComponent>(entity)) {
                m_registry->GetComponent<ECS::ScriptComponent>(entity).initialized = false;
            }
            ClearQueuedEvents(it->second, m_luaState);
        }
    }
}

void* ScriptEngine::Allocate(void* ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        if (ptr) {
            if (m_activeScript) {
                if (osize <= m_activeScript->currentMemory) {
                    m_activeScript->currentMemory -= osize;
                } else {
                    m_activeScript->currentMemory = 0;
                }
            }
            if (osize <= m_totalMemoryAllocated) {
                m_totalMemoryAllocated -= osize;
            } else {
                m_totalMemoryAllocated = 0;
            }
            std::free(ptr);
        }
        return nullptr;
    }

    if (ptr == nullptr) {
        if (m_activeScript && m_activeScript->maxMemory > 0) {
            if (m_activeScript->currentMemory + nsize > m_activeScript->maxMemory) {
                return nullptr;
            }
        }
        void* newPtr = std::malloc(nsize);
        if (newPtr) {
            if (m_activeScript) {
                m_activeScript->currentMemory += nsize;
                m_activeScript->peakMemory = std::max(m_activeScript->peakMemory, m_activeScript->currentMemory);
            }
            m_totalMemoryAllocated += nsize;
        }
        return newPtr;
    } else {
        size_t diff = 0;
        bool shrink = false;
        if (nsize > osize) {
            diff = nsize - osize;
        } else {
            diff = osize - nsize;
            shrink = true;
        }

        if (!shrink && m_activeScript && m_activeScript->maxMemory > 0) {
            if (m_activeScript->currentMemory + diff > m_activeScript->maxMemory) {
                return nullptr;
            }
        }

        void* newPtr = std::realloc(ptr, nsize);
        if (newPtr) {
            if (m_activeScript) {
                if (!shrink) {
                    m_activeScript->currentMemory += diff;
                } else {
                    if (diff <= m_activeScript->currentMemory) {
                        m_activeScript->currentMemory -= diff;
                    } else {
                        m_activeScript->currentMemory = 0;
                    }
                }
                m_activeScript->peakMemory = std::max(m_activeScript->peakMemory, m_activeScript->currentMemory);
            }
            if (!shrink) {
                m_totalMemoryAllocated += diff;
            } else {
                if (diff <= m_totalMemoryAllocated) {
                    m_totalMemoryAllocated -= diff;
                } else {
                    m_totalMemoryAllocated = 0;
                }
            }
        }
        return newPtr;
    }
}

void* ScriptEngine::LuaAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    return static_cast<ScriptEngine*>(ud)->Allocate(ptr, osize, nsize);
}

void ScriptEngine::LuaHookFunc(lua_State* L, lua_Debug* ar) {
    auto& se = ScriptEngine::Get();
    if (!se.m_activeScript) return;

    if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL) {
        if (se.m_activeScript->maxRecursionDepth > 0) {
            lua_Debug tempAr;
            if (lua_getstack(L, se.m_activeScript->maxRecursionDepth, &tempAr)) {
                luaL_error(L, "maximum recursion depth exceeded");
            }
        }
    } else if (ar->event == LUA_HOOKCOUNT) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = now - se.m_frameStartTime;
        if (se.m_activeScript->maxExecutionTime > 0.0f && elapsed.count() > se.m_activeScript->maxExecutionTime) {
            luaL_error(L, "script execution timeout");
        }
    }
}

std::vector<std::string> ScriptEngine::GetLoadedScripts() const {
    std::vector<std::string> list;
    for (const auto& [ref, info] : m_scripts) {
        list.push_back(info.scriptPath);
    }
    return list;
}

void ScriptEngine::ReloadAllScripts() {
    std::vector<std::string> paths;
    for (const auto& [ref, info] : m_scripts) {
        if (std::find(paths.begin(), paths.end(), info.scriptPath) == paths.end()) {
            paths.push_back(info.scriptPath);
        }
    }
    for (const auto& path : paths) {
        ReloadScript(path);
    }
}

void ScriptEngine::SetScriptEnabled(const std::string& scriptPath, bool enabled) {
    for (auto& [ref, info] : m_scripts) {
        if (info.scriptPath == scriptPath) {
            info.enabled = enabled;
            if (m_registry && m_registry->HasComponent<ECS::ScriptComponent>(info.entity)) {
                m_registry->GetComponent<ECS::ScriptComponent>(info.entity).initialized = enabled;
            }
        }
    }
}

void ScriptEngine::SetScriptEnabled(int envRef, bool enabled) {
    auto it = m_scripts.find(envRef);
    if (it != m_scripts.end()) {
        it->second.enabled = enabled;
        if (m_registry && m_registry->HasComponent<ECS::ScriptComponent>(it->second.entity)) {
            m_registry->GetComponent<ECS::ScriptComponent>(it->second.entity).initialized = enabled;
        }
    }
}

void ScriptEngine::SetScriptPaused(int envRef, bool paused) {
    auto it = m_scripts.find(envRef);
    if (it != m_scripts.end()) {
        it->second.paused = paused;
    }
}

std::string ScriptEngine::GetActiveScriptStats() const {
    std::stringstream ss;
    ss << "=== ACTIVE SCRIPT STATISTICS ===\n";
    ss << "Total Memory Allocated by Lua: " << std::fixed << std::setprecision(2)
       << static_cast<double>(m_totalMemoryAllocated) / 1024.0 << " KB\n";
    for (const auto& [ref, info] : m_scripts) {
        ss << "- Script: " << info.scriptPath << " (Entity " << info.entity << ", EnvRef " << ref << ")\n";
        ss << "  Status: " << (info.enabled ? "ENABLED" : "SUSPENDED") << " | " << (info.paused ? "PAUSED" : "RUNNING") << "\n";
        ss << "  Memory: " << static_cast<double>(info.currentMemory) / 1024.0 << " KB (Peak: "
           << static_cast<double>(info.peakMemory) / 1024.0 << " KB)\n";
    }
    ss << "================================\n";
    return ss.str();
}

std::string ScriptEngine::DumpProfilerInfo() const {
    std::stringstream ss;
    ss << "=== SCRIPT PROFILER INFORMATION ===\n";
    ss << std::left << std::setw(30) << "Script Path"
       << std::right << std::setw(10) << "Entity"
       << std::right << std::setw(10) << "Calls"
       << std::right << std::setw(12) << "Avg Time"
       << std::right << std::setw(12) << "Peak Time"
       << std::right << std::setw(12) << "Memory\n";
    ss << "------------------------------------------------------------------------------------\n";
    for (const auto& [ref, info] : m_scripts) {
        std::string filename = std::filesystem::path(info.scriptPath).filename().string();
        ss << std::left << std::setw(30) << (filename.size() > 28 ? filename.substr(0, 25) + "..." : filename)
           << std::right << std::setw(10) << info.entity
           << std::right << std::setw(10) << info.callCount
           << std::right << std::fixed << std::setprecision(4) << std::setw(10) << info.avgExecutionTime << " ms"
           << std::right << std::fixed << std::setprecision(4) << std::setw(10) << info.peakExecutionTime << " ms"
           << std::right << std::setw(10) << static_cast<double>(info.currentMemory) / 1024.0 << " KB\n";
    }
    ss << "===================================\n";
    return ss.str();
}

void ScriptEngine::SetDefaultLimits(size_t maxMem, float maxTimeMs, int maxRecursion, size_t maxQueue) {
    m_defaultMaxMemory = maxMem;
    m_defaultMaxExecutionTime = maxTimeMs;
    m_defaultMaxRecursionDepth = maxRecursion;
    m_defaultMaxEventQueueSize = maxQueue;
}

void ScriptEngine::SetScriptLimits(int envRef, size_t maxMem, float maxTimeMs, int maxRecursion, size_t maxQueue) {
    auto it = m_scripts.find(envRef);
    if (it != m_scripts.end()) {
        it->second.maxMemory = maxMem;
        it->second.maxExecutionTime = maxTimeMs;
        it->second.maxRecursionDepth = maxRecursion;
        it->second.maxEventQueueSize = maxQueue;
    }
}

} // namespace KumariEngine::Scripting
