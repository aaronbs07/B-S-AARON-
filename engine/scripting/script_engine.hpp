#pragma once
#include "ecs/ecs.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

struct lua_State;
struct lua_Debug;

namespace KumariEngine::Core {
class Event;
}

namespace KumariEngine::Save {
class BinaryWriter;
class BinaryReader;
}

namespace KumariEngine::Scripting {

struct LuaSubscription {
    std::string eventName;
    int callbackRef;
    uint32_t cppSubId;
};

class ScriptEngine {
public:
    static ScriptEngine& Get() {
        static ScriptEngine instance;
        return instance;
    }

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    bool Initialize(ECS::Registry* registry);
    void Shutdown();

    // ECS Lifecycle integration
    void Update(float dt);
    void OnCreateEntity(ECS::Entity entity);
    void OnUpdateEntity(ECS::Entity entity, float dt);
    void OnDestroyEntity(ECS::Entity entity);

    bool SerializeScriptState(ECS::Entity entity, Save::BinaryWriter& writer) const;
    bool DeserializeScriptState(ECS::Entity entity, Save::BinaryReader& reader);

    // Helpers
    int LoadScript(ECS::Entity entity, const std::string& scriptPath);
    void UnloadScript(int envRef);
    bool ReloadScript(const std::string& scriptPath);
    lua_State* GetLuaState() const { return m_luaState; }
    ECS::Entity GetCurrentEntity() const { return m_currentEntity; }
    void DispatchToLua(ECS::Entity entity, int callbackRef, const Core::Event& event);
    void TriggerTimelineEvent(ECS::Entity entity, const std::string& eventName, float eventValue);
    ECS::Registry* GetRegistry() const { return m_registry; }

    // Developer Utilities
    std::vector<std::string> GetLoadedScripts() const;
    void ReloadAllScripts();
    void SetScriptEnabled(const std::string& scriptPath, bool enabled);
    void SetScriptEnabled(int envRef, bool enabled);
    void SetScriptPaused(int envRef, bool paused);
    std::string GetActiveScriptStats() const;
    std::string DumpProfilerInfo() const;

    // Limits config
    void SetDefaultLimits(size_t maxMem, float maxTimeMs, int maxRecursion, size_t maxQueue);
    void SetScriptLimits(int envRef, size_t maxMem, float maxTimeMs, int maxRecursion, size_t maxQueue);
    size_t GetTotalMemoryAllocated() const { return m_totalMemoryAllocated; }

    struct QueuedEvent {
        int callbackRef;
        std::string eventType;
        int intVal1 = 0;
        int intVal2 = 0;
        double doubleVal1 = 0.0;
        double doubleVal2 = 0.0;
        glm::vec3 vecVal = glm::vec3(0.0f);
        float floatVal1 = 0.0f;
        int customDataRef = -1;
    };

    struct ScriptInfo {
        std::string scriptPath;
        ECS::Entity entity = ECS::NULL_ENTITY;
        int envRef = -1;
        bool enabled = true;
        bool paused = false;
        bool isOneShot = false;
        int updateOrder = 0;

        // Resource limits
        size_t maxMemory = 0;
        size_t currentMemory = 0;
        float maxExecutionTime = 0.0f; // in milliseconds
        int maxRecursionDepth = 0;
        size_t maxEventQueueSize = 100;

        // Profiler metrics
        double avgExecutionTime = 0.0;
        double peakExecutionTime = 0.0;
        double totalExecutionTime = 0.0;
        uint64_t callCount = 0;
        size_t peakMemory = 0;
        int warningCount = 0;

        std::vector<QueuedEvent> queuedEvents;
    };

private:
    struct PrimitiveValue {
        enum class Type { Bool, Integer, Double, String, Entity } type;
        bool boolVal = false;
        int64_t intVal = 0;
        double doubleVal = 0.0;
        std::string strVal;
        ECS::Entity entityVal = ECS::NULL_ENTITY;
    };

    std::unordered_map<std::string, PrimitiveValue> SaveEnvironment(int envRef);
    void RestoreEnvironment(int envRef, const std::unordered_map<std::string, PrimitiveValue>& state);

    ScriptEngine() = default;
    ~ScriptEngine() = default;

    void RegisterAPI();
    bool PrepareCall(int envRef, const std::string& funcName);
    void HandleScriptError(int envRef, ECS::Entity entity, const std::string& stage, const std::string& rawError);

    // Allocator and hook helpers
    void* Allocate(void* ptr, size_t osize, size_t nsize);
    static void* LuaAllocator(void* ud, void* ptr, size_t osize, size_t nsize);
    static void LuaHookFunc(lua_State* L, lua_Debug* ar);

    lua_State* m_luaState = nullptr;
    ECS::Registry* m_registry = nullptr;
    ECS::Entity m_currentEntity = ECS::NULL_ENTITY;
    std::unordered_map<ECS::Entity, std::vector<LuaSubscription>> m_entitySubscriptions;

    // Scheduler and sandboxing state
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    ScriptInfo* m_activeScript = nullptr;
    size_t m_totalMemoryAllocated = 0;
    
    // Default fallback limits
    size_t m_defaultMaxMemory = 16 * 1024 * 1024; // 16 MB default
    float m_defaultMaxExecutionTime = 10.0f;       // 10 ms default
    int m_defaultMaxRecursionDepth = 50;          // 50 depth default
    size_t m_defaultMaxEventQueueSize = 100;

    std::unordered_map<int, ScriptInfo> m_scripts;

    friend class CurrentEntityScope;
    friend class ActiveScriptScope;
    friend class ScriptProfileScope;
    friend int Lua_Subscribe(lua_State* L);
    friend int Lua_Unsubscribe(lua_State* L);
};

class ActiveScriptScope {
public:
    ActiveScriptScope(ScriptEngine::ScriptInfo* info) {
        m_prev = ScriptEngine::Get().m_activeScript;
        ScriptEngine::Get().m_activeScript = info;
    }
    ~ActiveScriptScope() {
        ScriptEngine::Get().m_activeScript = m_prev;
    }
private:
    ScriptEngine::ScriptInfo* m_prev;
};

class ScriptProfileScope {
public:
    ScriptProfileScope(ScriptEngine::ScriptInfo* info) : m_info(info) {
        m_startTime = std::chrono::high_resolution_clock::now();
    }
    ~ScriptProfileScope() {
        if (m_info) {
            auto endTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = endTime - m_startTime;
            double ms = elapsed.count();
            m_info->callCount++;
            m_info->totalExecutionTime += ms;
            m_info->avgExecutionTime = m_info->totalExecutionTime / m_info->callCount;
            m_info->peakExecutionTime = std::max(m_info->peakExecutionTime, ms);
            m_info->peakMemory = std::max(m_info->peakMemory, m_info->currentMemory);
        }
    }
private:
    ScriptEngine::ScriptInfo* m_info;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

class CurrentEntityScope {
public:
    CurrentEntityScope(ECS::Entity entity) {
        m_prev = ScriptEngine::Get().m_currentEntity;
        ScriptEngine::Get().m_currentEntity = entity;
    }
    ~CurrentEntityScope() {
        ScriptEngine::Get().m_currentEntity = m_prev;
    }
private:
    ECS::Entity m_prev;
};

} // namespace KumariEngine::Scripting
