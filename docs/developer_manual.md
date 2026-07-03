# Kumari Engine Developer Manual

This manual explains how to write code, define custom components, register types, and utilize the profiler framework.

## 1. Creating Custom Components

Components must be registered in the reflection type registry to enable inspector visibility and property editing.

### Step 1: Define the Component Structure
```cpp
#pragma once
#include <string>
#include "reflection/reflection.hpp"

namespace KumariEngine::Gameplay {

struct PlayerStatsComponent {
    int level = 1;
    float health = 100.0f;
    std::string name = "Player";
};

} // namespace KumariEngine::Gameplay
```

### Step 2: Register Reflection Metadata
In your source file (e.g. `PlayerStatsComponent.cpp`):
```cpp
#include "PlayerStatsComponent.hpp"
#include "reflection/type_registry.hpp"

REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::PlayerStatsComponent)
    REFLECT_MEMBER(level)
    REFLECT_MEMBER(health)
    REFLECT_MEMBER(name)
REFLECT_COMPONENT_END()
```

## 2. Timing Custom Workloads
Use the `PROFILE_SCOPE` macro to profile functions:
```cpp
#include "core/profiler.hpp"

void ProcessPhysics() {
    PROFILE_SCOPE("ProcessPhysics");
    // Workload here...
}
```

For custom threads, register with `ThreadProfiler` first:
```cpp
#include "core/thread_profiler.hpp"

void MyWorkerThread() {
    auto tid = std::this_thread::get_id();
    Core::ThreadProfiler::Get().RegisterThread(tid, "MyWorker");
    
    while (running) {
        Core::ThreadProfiler::Get().BeginWork(tid, "Processing");
        // Do work
        Core::ThreadProfiler::Get().EndWork(tid);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

## 3. Building the Project
Run the automated build script:
```powershell
.\build_project.ps1
```
This script automatically:
- Compiles both Debug and Release configurations.
- Packages assets using RLE compression and rotating XOR encryption.
- Runs all unit, smoke, regression, and benchmark tests.
