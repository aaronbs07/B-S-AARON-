#include "core/event_manager.hpp"
#include "scripting/script_engine.hpp"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "core/logger.hpp"
#include <algorithm>

namespace KumariEngine::Core {

// LuaCustomEvent implementation
LuaCustomEvent::LuaCustomEvent(const std::string& type, int dataRef)
    : m_type(type), m_dataRef(dataRef) {}

LuaCustomEvent::~LuaCustomEvent() {
    if (m_dataRef != -1) {
        auto* luaState = Scripting::ScriptEngine::Get().GetLuaState();
        if (luaState) {
            luaL_unref(luaState, LUA_REGISTRYINDEX, m_dataRef);
        }
    }
}

LuaCustomEvent::LuaCustomEvent(LuaCustomEvent&& other) noexcept
    : m_type(std::move(other.m_type)), m_dataRef(other.m_dataRef) {
    other.m_dataRef = -1;
}

LuaCustomEvent& LuaCustomEvent::operator=(LuaCustomEvent&& other) noexcept {
    if (this != &other) {
        if (m_dataRef != -1) {
            auto* luaState = Scripting::ScriptEngine::Get().GetLuaState();
            if (luaState) {
                luaL_unref(luaState, LUA_REGISTRYINDEX, m_dataRef);
            }
        }
        m_type = std::move(other.m_type);
        m_dataRef = other.m_dataRef;
        other.m_dataRef = -1;
    }
    return *this;
}

// EventManager implementation
EventManager& EventManager::Get() {
    static EventManager instance;
    return instance;
}

SubscriptionId EventManager::Subscribe(const std::string& eventType, const EventCallback& callback) {
    SubscriptionId id = m_nextSubscriptionId++;
    m_listeners[eventType].push_back({id, callback});
    return id;
}

void EventManager::Unsubscribe(const std::string& eventType, SubscriptionId id) {
    auto it = m_listeners.find(eventType);
    if (it != m_listeners.end()) {
        auto& list = it->second;
        list.erase(
            std::remove_if(list.begin(), list.end(), [id](const auto& pair) { return pair.first == id; }),
            list.end()
        );
    }
}

void EventManager::DispatchEvent(const Event& event) {
    auto it = m_listeners.find(event.GetType());
    if (it != m_listeners.end()) {
        // Copy the list to prevent modifications to m_listeners during iteration
        auto callbacks = it->second;
        for (const auto& pair : callbacks) {
            pair.second(event);
        }
    }
}

void EventManager::QueueEvent(std::unique_ptr<Event> event) {
    m_eventQueue.push_back(std::move(event));
}

void EventManager::ProcessQueue() {
    if (m_eventQueue.empty()) return;

    // Swap queues to allow pushing events during event processing
    std::vector<std::unique_ptr<Event>> queueToProcess;
    queueToProcess.swap(m_eventQueue);

    for (const auto& event : queueToProcess) {
        DispatchEvent(*event);
    }
}

void EventManager::Clear() {
    m_listeners.clear();
    m_eventQueue.clear();
}

} // namespace KumariEngine::Core
