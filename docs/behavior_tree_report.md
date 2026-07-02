# Behavior Tree Report — Kumari Engine

This report details the Behavior Tree (BT) foundation and execution system in Kumari Engine.

## Node Hierarchy

The behavior tree system is built around `BTNode` subclasses, returning a `BTState` of `Success`, `Failure`, or `Running` on tick:

### Composites
- **`SelectorNode`**: Ticks children sequentially. If any child returns `Success` or `Running`, the selector returns that state. It fails only when all children fail.
- **`SequenceNode`**: Ticks children sequentially. If any child returns `Failure` or `Running`, the sequence returns that state. It succeeds only when all children succeed.

### Decorators
- **`InverterDecorator`**: Negates child outcomes (swaps `Success` and `Failure`).
- **`SucceederDecorator`**: Forces child outcome to `Success`.
- **`BlackboardConditionDecorator`**: Evaluates conditions on the entity's blackboard before ticking the child. Queries include value presence (`Exists`), bools (`IsTrue`, `IsFalse`), and equality checks (`EqualInt`, `EqualFloat`).

### Services
- **`PerceptionServiceNode`**: Periodically (based on tick interval) updates perceived targets and variables, keeping the blackboard updated for tasks.

### Tasks
- **`WaitTaskNode`**: Pauses execution for a specified duration using the blackboard's `DeltaTime`.
- **`MoveToTaskNode`**: Instructs the `AIController` to move toward a blackboard target position, returning `Running` until the agent reaches the destination.
