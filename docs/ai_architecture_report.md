# AI Architecture Report — Kumari Engine

This report details the architectural design and ECS integration of the AI subsystem in Kumari Engine.

## Modular Component Design

The AI framework follows a strictly decoupled Entity Component System (ECS) architecture, splitting AI logic and state into individual, clean data components:

1. **`BlackboardComponent`**: Acts as the dynamic data hub for each entity. It stores blackboard parameters utilizing a standard `std::unordered_map` mapping string keys to `BlackboardValue` variants. Supported types include `int`, `float`, `bool`, `std::string`, `glm::vec3`, and `ECS::Entity`.
2. **`PerceptionComponent`**: Manages sensor stimulus tracking. It holds vision settings (FOV angle, range) and a list of perceived stimuli containing target locations, entity references, and direct visibility flags.
3. **`NavigationAgentComponent`**: Manages the agent's steering, speed, and pathing state. It stores current targets, active paths, path indexes, acceptance limits, and dynamic avoidance options.
4. **`AIComponent`**: Host component that links the entity's behavior tree state machine (`BehaviorTree`) and active controller.

## ECS Registration & Integration
All components are fully registered within the ECS system inside `editor/editor.cpp` and processed in the system update loop:
- **`AISystem::Update()`** scans the `BehaviorTreeComponent` and `AIComponent` entities. It updates the Blackboard component with the frame `DeltaTime` and ticks the associated Behavior Tree.
- It then processes path traversal updates for entities containing both `NavigationAgentComponent` and `Scene::TransformComponent`, driving physics rotation and translation.
