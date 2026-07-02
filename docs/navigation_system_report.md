# Navigation System Report — Kumari Engine

This report details the Navigation Mesh (NavMesh) and pathfinding subsystem in Kumari Engine.

## NavMesh Structure & Generation

The **NavMesh** class represents a walkable grid overlaid on the world terrain:
- **`BuildGrid()`** generates a regular 2D grid of nodes centered at a given world position.
- **`BuildFromWorld()`** updates these nodes by querying the `TerrainManager` height at each coordinate. Nodes are checked against a slope limit (45 degrees) based on terrain normals.
- **Obstacle Check**: Dynamic static physics colliders (such as `Sphere`, `AABB`, and `Capsule`) are checked using the `agentRadius` and `agentHeight` settings. Nodes overlapping static obstacles are marked as non-walkable.

## Pathfinding & Path Smoothing

1. **A* Search**: Performs grid searching using an 8-way traversal system. Corner-cutting is dynamically prevented by verifying that the adjacent orthogonal nodes are walkable before traversing diagonally.
2. **Line-of-Sight Smoothing**: Minimizes zig-zag behaviors by checking if direct line-of-sight is available between non-adjacent path coordinates, skipping redundant intermediate points.
3. **Serialization**: Supports binary saving and loading of generated meshes via `Save()` and `Load()` interfaces.

## Caching Strategy
- The system includes a path cache mapping hashed quantized coordinate key pairs (`startX, startZ, endX, endZ`) to pre-computed vector paths.
- Active paths are validated every tick using `ValidatePath()`. If any node along the path becomes non-walkable (e.g., due to dynamic modifications), the cache is invalidated, and a new search is triggered.
