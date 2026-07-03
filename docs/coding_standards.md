# Kumari Engine Coding Standards

This guide outlines code formatting, design patterns, naming conventions, and resource management policies.

## 1. Naming Conventions

- **Namespaces**: `PascalCase` (e.g. `KumariEngine::Core`).
- **Classes & Structs**: `PascalCase` (e.g. `ResourceManager`).
- **Functions & Methods**: `PascalCase` (e.g. `LoadResource`).
- **Variables & Parameters**: `camelCase` (e.g. `elapsedTime`).
- **Private Member Variables**: Prefix with `m_` (e.g. `m_resources`).
- **Constants & Enums**: `UPPER_CASE` (e.g. `MAX_ENTITIES`).

## 2. Resource Management & RAII

- **Raw Pointers**: Avoid using raw pointers for resource ownership. Always prefer `std::unique_ptr` or `std::shared_ptr`.
- **RAII**: Acquire resources in constructor, release in destructor. Utilize RAII helpers for locks (`std::lock_guard`, `std::unique_lock`) and profiler scopes (`PROFILE_SCOPE`).
- **Vulkan Handles**: Always wrap Vulkan handles in smart wrappers or manage them explicitly inside Vulkan cleanup functions.

## 3. Multithreading & Synchronization

- Always protect shared data accessed by multiple threads using `std::mutex` or `std::shared_mutex`.
- Prefer read-write locks (`std::shared_lock` vs `std::unique_lock`) to maximize throughput for read-heavy operations.
- Avoid locks in hot performance paths by utilizing thread-local accumulators (e.g., CPU Profiler stack).

## 4. Error Handling & Logging

- Utilize the structured `Core::Logger` categories (e.g. `Core::Logger::Info("Physics", "Message")`).
- Avoid standard `std::cout` in production code. Prefer logging with explicit severity levels (`Info`, `Warning`, `Error`).
- Use assertions (`assert`) to validate pre-conditions and invariants in Debug builds.
