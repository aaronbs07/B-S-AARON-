#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"

namespace KumariEngine::Core {

// Base class for all events
class Event {
public:
    virtual ~Event() = default;
    virtual std::string GetType() const = 0;
};

// Key input events (OnKeyPressed, OnKeyReleased)
class KeyEvent : public Event {
public:
    KeyEvent(const std::string& type, int key) : m_type(type), m_key(key) {}
    std::string GetType() const override { return m_type; }
    int GetKey() const { return m_key; }
private:
    std::string m_type;
    int m_key;
};

// Mouse button events (OnMouseButtonPressed, OnMouseButtonReleased)
class MouseButtonEvent : public Event {
public:
    MouseButtonEvent(const std::string& type, int button) : m_type(type), m_button(button) {}
    std::string GetType() const override { return m_type; }
    int GetButton() const { return m_button; }
private:
    std::string m_type;
    int m_button;
};

// Mouse movement events (OnMouseMoved)
class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(double x, double y) : m_x(x), m_y(y) {}
    std::string GetType() const override { return "OnMouseMoved"; }
    double GetX() const { return m_x; }
    double GetY() const { return m_y; }
private:
    double m_x;
    double m_y;
};

// Physics collision events (OnCollisionEnter, OnCollisionStay)
class CollisionEvent : public Event {
public:
    CollisionEvent(const std::string& type, ECS::Entity entityA, ECS::Entity entityB, const glm::vec3& normal, float penetration)
        : m_type(type), m_entityA(entityA), m_entityB(entityB), m_normal(normal), m_penetration(penetration) {}
    std::string GetType() const override { return m_type; }
    ECS::Entity GetEntityA() const { return m_entityA; }
    ECS::Entity GetEntityB() const { return m_entityB; }
    const glm::vec3& GetNormal() const { return m_normal; }
    float GetPenetration() const { return m_penetration; }
private:
    std::string m_type;
    ECS::Entity m_entityA;
    ECS::Entity m_entityB;
    glm::vec3 m_normal;
    float m_penetration;
};

// Physics collision exit events (OnCollisionExit)
class CollisionExitEvent : public Event {
public:
    CollisionExitEvent(ECS::Entity entityA, ECS::Entity entityB)
        : m_entityA(entityA), m_entityB(entityB) {}
    std::string GetType() const override { return "OnCollisionExit"; }
    ECS::Entity GetEntityA() const { return m_entityA; }
    ECS::Entity GetEntityB() const { return m_entityB; }
private:
    ECS::Entity m_entityA;
    ECS::Entity m_entityB;
};

// Physics trigger events (OnTriggerEnter, OnTriggerExit)
class TriggerEvent : public Event {
public:
    TriggerEvent(const std::string& type, ECS::Entity triggerEntity, ECS::Entity otherEntity)
        : m_type(type), m_triggerEntity(triggerEntity), m_otherEntity(otherEntity) {}
    std::string GetType() const override { return m_type; }
    ECS::Entity GetTriggerEntity() const { return m_triggerEntity; }
    ECS::Entity GetOtherEntity() const { return m_otherEntity; }
private:
    std::string m_type;
    ECS::Entity m_triggerEntity;
    ECS::Entity m_otherEntity;
};

// Custom events published from Lua (with reference to Lua data table/value)
class LuaCustomEvent : public Event {
public:
    LuaCustomEvent(const std::string& type, int dataRef);
    ~LuaCustomEvent() override;

    // Prevent copying to avoid multiple reference releases
    LuaCustomEvent(const LuaCustomEvent&) = delete;
    LuaCustomEvent& operator=(const LuaCustomEvent&) = delete;

    // Allow moving
    LuaCustomEvent(LuaCustomEvent&& other) noexcept;
    LuaCustomEvent& operator=(LuaCustomEvent&& other) noexcept;

    std::string GetType() const override { return m_type; }
    int GetDataRef() const { return m_dataRef; }
private:
    std::string m_type;
    int m_dataRef;
};

using SubscriptionId = uint32_t;
using EventCallback = std::function<void(const Event&)>;

class EventManager {
public:
    static EventManager& Get();

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // Event Registration
    SubscriptionId Subscribe(const std::string& eventType, const EventCallback& callback);
    void Unsubscribe(const std::string& eventType, SubscriptionId id);

    // Event Dispatch
    void DispatchEvent(const Event& event);
    void QueueEvent(std::unique_ptr<Event> event);
    void ProcessQueue();

    // Clear all subscriptions and queue on shutdown
    void Clear();

private:
    EventManager() = default;
    ~EventManager() = default;

    std::unordered_map<std::string, std::vector<std::pair<SubscriptionId, EventCallback>>> m_listeners;
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    SubscriptionId m_nextSubscriptionId = 1;
};

} // namespace KumariEngine::Core
