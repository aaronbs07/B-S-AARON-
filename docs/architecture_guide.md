# Kumari Engine Architecture Guide

Welcome to the Kumari Engine Architecture Guide. This document provides a high-level overview of the engine's core subsystems, their design principles, and their interactions.

## 1. Subsystem Architecture Overview

Kumari Engine is structured as a collection of decoupled, modular subsystems sharing a unified interface design.

```
       [ Kumari Kandam Editor ]
                  |
                  v
         [ Kumari Engine ]
  ________________|________________
  |               |               |
[ECS]        [Renderer]      [Resource]
  |               |               |
[Physics]    [Animation]     [Scripting]
```

## 2. Core Subsystems

### 2.1 Entity Component System (ECS)
The ECS module uses a sparse-set entity component registry designed for cache-friendly iterations and flat memory storage:
- **Registry**: Coordinates entity lifetimes and component pool lookup.
- **ComponentPool**: Contiguous arrays containing raw component data, allowing systems to traverse components in a cache-coherent manner.

### 2.2 Vulkan Renderer
The graphics pipeline uses a modular frame layout:
- **RenderGraph**: Directs render passes, managing automatic resource transitions and barriers.
- **Post-Process Pipeline**: Implements HDR tonemapping, SSR (Screen-Space Reflections), and IB (Image-Based) lighting.
- **GPU Profiler**: Uses Vulkan timestamp query pools to resolve GPU stage timings down to nanosecond precision.

### 2.3 Resource System
The `ResourceManager` implements a thread-safe caching system with LRU (Least Recently Used) eviction:
- Strong reference LRU list keeps active assets loaded up to capacity limit.
- Automatic eviction of unused resources avoids memory bloat and leaks.

### 2.4 Performance Profiler Subsystem
A multi-threaded performance monitoring pipeline:
- **CPU Profiler**: Tracks nested call stacks hierarchically per thread.
- **Thread Profiler**: Tracks CPU utilization on active threads (Main, Physics, Renderer, Streaming, AI, Network).
- **Memory Tracker**: Estimates allocation counts, peak usage, and fragmentation ratio.
- **Frame Graph**: Accumulates metrics into a 256-frame ring buffer for export to JSON.

## 3. Play Mode and State Management
The `EditorManager` coordinates transitions between editing and play state:
- Captures serializable scene pre-play snapshots.
- Restores original world state on simulation stop, preventing scene leakage.
- Scales delta time to support step-by-step frame debugging.
