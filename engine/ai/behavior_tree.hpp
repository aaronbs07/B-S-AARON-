#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "ecs/ecs.hpp"
#include "ai_components.hpp"

namespace KumariEngine::AI {

enum class BTState {
    Success,
    Failure,
    Running
};

class BTNode {
public:
    virtual ~BTNode() = default;
    virtual BTState Tick(ECS::Registry* registry, ECS::Entity entity) = 0;
    virtual std::string GetName() const = 0;
    virtual void Reset() {}
};

// --- Composite Nodes ---
class SelectorNode : public BTNode {
public:
    void AddChild(std::shared_ptr<BTNode> child) { m_children.push_back(child); }
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "Selector"; }
    void Reset() override {
        m_currentChildIndex = 0;
        for (auto& child : m_children) child->Reset();
    }
    const std::vector<std::shared_ptr<BTNode>>& GetChildren() const { return m_children; }
private:
    std::vector<std::shared_ptr<BTNode>> m_children;
    size_t m_currentChildIndex = 0;
};

class SequenceNode : public BTNode {
public:
    void AddChild(std::shared_ptr<BTNode> child) { m_children.push_back(child); }
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "Sequence"; }
    void Reset() override {
        m_currentChildIndex = 0;
        for (auto& child : m_children) child->Reset();
    }
    const std::vector<std::shared_ptr<BTNode>>& GetChildren() const { return m_children; }
private:
    std::vector<std::shared_ptr<BTNode>> m_children;
    size_t m_currentChildIndex = 0;
};

// --- Decorator Nodes ---
class DecoratorNode : public BTNode {
public:
    DecoratorNode(std::shared_ptr<BTNode> child) : m_child(child) {}
    void Reset() override { if (m_child) m_child->Reset(); }
    std::shared_ptr<BTNode> GetChild() const { return m_child; }
protected:
    std::shared_ptr<BTNode> m_child;
};

class InverterDecorator : public DecoratorNode {
public:
    using DecoratorNode::DecoratorNode;
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "Inverter"; }
};

class SucceederDecorator : public DecoratorNode {
public:
    using DecoratorNode::DecoratorNode;
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "Succeeder"; }
};

enum class BlackboardQueryType {
    Exists,
    IsTrue,
    IsFalse,
    EqualInt,
    EqualFloat
};

class BlackboardConditionDecorator : public DecoratorNode {
public:
    BlackboardConditionDecorator(std::shared_ptr<BTNode> child, const std::string& key, BlackboardQueryType queryType, BlackboardValue compareValue = 0)
        : DecoratorNode(child), m_key(key), m_queryType(queryType), m_compareValue(compareValue) {}
    
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "BlackboardCondition(" + m_key + ")"; }

private:
    std::string m_key;
    BlackboardQueryType m_queryType;
    BlackboardValue m_compareValue;
};

// --- Service Nodes ---
class ServiceNode : public DecoratorNode {
public:
    ServiceNode(std::shared_ptr<BTNode> child, float tickInterval)
        : DecoratorNode(child), m_interval(tickInterval), m_timeSinceLastTick(tickInterval) {}
    
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    virtual void TickService(ECS::Registry* registry, ECS::Entity entity) = 0;

protected:
    float m_interval;
    float m_timeSinceLastTick;
};

class PerceptionServiceNode : public ServiceNode {
public:
    PerceptionServiceNode(std::shared_ptr<BTNode> child, float tickInterval, const std::string& targetBBKey = "Target")
        : ServiceNode(child, tickInterval), m_targetBBKey(targetBBKey) {}
    
    std::string GetName() const override { return "PerceptionService"; }
    void TickService(ECS::Registry* registry, ECS::Entity entity) override;

private:
    std::string m_targetBBKey;
};

// --- Task Nodes ---
class WaitTaskNode : public BTNode {
public:
    WaitTaskNode(float duration) : m_duration(duration), m_elapsedTime(0.0f) {}
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "WaitTask"; }
    void Reset() override { m_elapsedTime = 0.0f; }

private:
    float m_duration;
    float m_elapsedTime;
};

class MoveToTaskNode : public BTNode {
public:
    MoveToTaskNode(const std::string& targetPositionBBKey) : m_targetPositionBBKey(targetPositionBBKey) {}
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override;
    std::string GetName() const override { return "MoveToTask(" + m_targetPositionBBKey + ")"; }
    void Reset() override {}

private:
    std::string m_targetPositionBBKey;
};

class ActionNode : public BTNode {
public:
    using ActionFunc = std::function<BTState(ECS::Registry*, ECS::Entity)>;
    ActionNode(const std::string& name, ActionFunc func) : m_name(name), m_func(func) {}
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override { return m_func(registry, entity); }
    std::string GetName() const override { return m_name; }
private:
    std::string m_name;
    ActionFunc m_func;
};

class ConditionNode : public BTNode {
public:
    using ConditionFunc = std::function<bool(ECS::Registry*, ECS::Entity)>;
    ConditionNode(const std::string& name, ConditionFunc func) : m_name(name), m_func(func) {}
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) override {
        return m_func(registry, entity) ? BTState::Success : BTState::Failure;
    }
    std::string GetName() const override { return m_name; }
private:
    std::string m_name;
    ConditionFunc m_func;
};

// --- BehaviorTree Core ---
class BehaviorTree {
public:
    void SetRoot(std::shared_ptr<BTNode> root) { m_root = root; }
    BTState Tick(ECS::Registry* registry, ECS::Entity entity) {
        if (m_root) return m_root->Tick(registry, entity);
        return BTState::Failure; // BTState::Failure matches what the component expects
    }
    std::shared_ptr<BTNode> GetRoot() const { return m_root; }
    void Reset() { if (m_root) m_root->Reset(); }
private:
    std::shared_ptr<BTNode> m_root;
};

struct BehaviorTreeComponent {
    BehaviorTree tree;
    BTState lastResult = static_cast<BTState>(1); // matching existing initial value
};

} // namespace KumariEngine::AI
