#include "ai_system.hpp"
#include "core/logger.hpp"
#include "core/vfs.hpp"
#include "scene/transform_component.hpp"
#include "physics/physics_components.hpp"
#include "terrain/terrain_manager.hpp"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace KumariEngine::AI {

// --- Navigation Mesh implementation ---

void NavMesh::BuildGrid(const glm::vec3& center, int width, int depth, float spacing) {
    m_center = center;
    m_width = width;
    m_depth = depth;
    m_spacing = spacing;
    
    m_nodes.clear();
    m_nodes.resize(width * depth);
    m_pathCache.clear();

    float startX = center.x - (width - 1) * spacing * 0.5f;
    float startZ = center.z - (depth - 1) * spacing * 0.5f;

    // 1. Create nodes
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int idx = z * width + x;
            m_nodes[idx].position = glm::vec3(startX + x * spacing, center.y, startZ + z * spacing);
            m_nodes[idx].walkable = true;
        }
    }

    // 2. Link neighbors (8-way grid connectivity with corner-cutting prevention)
    auto isNodeWalkable = [&](int gx, int gz) {
        if (gx < 0 || gx >= width || gz < 0 || gz >= depth) return false;
        return m_nodes[gz * width + gx].walkable;
    };

    int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    int dz[] = {0, 0, -1, 1, -1, -1, 1, 1};

    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int idx = z * width + x;
            auto& node = m_nodes[idx];

            for (int i = 0; i < 8; ++i) {
                int nx = x + dx[i];
                int nz = z + dz[i];
                if (nx >= 0 && nx < width && nz >= 0 && nz < depth) {
                    if (i >= 4) {
                        if (!isNodeWalkable(x + dx[i], z) || !isNodeWalkable(x, z + dz[i])) {
                            continue;
                        }
                    }
                    node.neighbors.push_back(nz * width + nx);
                }
            }
        }
    }
}

void NavMesh::BuildFromWorld(ECS::Registry* registry, const glm::vec3& center, int width, int depth, float spacing, float agentRadius, float agentHeight) {
    m_center = center;
    m_width = width;
    m_depth = depth;
    m_spacing = spacing;
    
    m_nodes.clear();
    m_nodes.resize(width * depth);
    m_pathCache.clear();

    float startX = center.x - (width - 1) * spacing * 0.5f;
    float startZ = center.z - (depth - 1) * spacing * 0.5f;

    // 1. Create nodes and check walkability
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int idx = z * width + x;
            float worldX = startX + x * spacing;
            float worldZ = startZ + z * spacing;
            
            float height = Terrain::TerrainManager::Get().GetHeightAt(worldX, worldZ);
            m_nodes[idx].position = glm::vec3(worldX, height, worldZ);
            m_nodes[idx].walkable = true;

            // Check slope
            float eps = 0.1f;
            float hL = Terrain::TerrainManager::Get().GetHeightAt(worldX - eps, worldZ);
            float hR = Terrain::TerrainManager::Get().GetHeightAt(worldX + eps, worldZ);
            float hD = Terrain::TerrainManager::Get().GetHeightAt(worldX, worldZ - eps);
            float hU = Terrain::TerrainManager::Get().GetHeightAt(worldX, worldZ + eps);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
            
            float angle = glm::acos(glm::clamp(glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f));
            if (angle > glm::radians(45.0f)) {
                m_nodes[idx].walkable = false;
            }
            
            // Check static obstacles in the registry
            if (m_nodes[idx].walkable && registry) {
                auto view = registry->View<Physics::PhysicsComponent, Scene::TransformComponent>();
                for (auto entity : view) {
                    const auto& pc = registry->GetComponent<Physics::PhysicsComponent>(entity);
                    if (pc.bodyType != Physics::BodyType::Static) continue;
                    const auto& tc = registry->GetComponent<Scene::TransformComponent>(entity);
                    
                    if (pc.collider.type == Physics::ColliderType::Sphere) {
                        const auto& sphere = std::get<Physics::Sphere>(pc.collider.shape);
                        glm::vec3 sphereCenter = tc.position + sphere.center;
                        float dist = glm::distance(m_nodes[idx].position + glm::vec3(0, agentHeight*0.5f, 0), sphereCenter);
                        if (dist < (sphere.radius + agentRadius)) {
                            m_nodes[idx].walkable = false;
                            break;
                        }
                    } else if (pc.collider.type == Physics::ColliderType::AABB) {
                        const auto& aabb = std::get<Physics::AABB>(pc.collider.shape);
                        glm::vec3 boxMin = tc.position + aabb.min;
                        glm::vec3 boxMax = tc.position + aabb.max;
                        if (worldX + agentRadius >= boxMin.x && worldX - agentRadius <= boxMax.x &&
                            worldZ + agentRadius >= boxMin.z && worldZ - agentRadius <= boxMax.z &&
                            height + agentHeight >= boxMin.y && height <= boxMax.y) {
                            m_nodes[idx].walkable = false;
                            break;
                        }
                    } else if (pc.collider.type == Physics::ColliderType::Capsule) {
                        const auto& cap = std::get<Physics::Capsule>(pc.collider.shape);
                        glm::vec3 capCenter = tc.position + cap.center;
                        float dist = glm::distance(m_nodes[idx].position + glm::vec3(0, agentHeight*0.5f, 0), capCenter);
                        if (dist < (cap.radius + cap.halfHeight + agentRadius)) {
                            m_nodes[idx].walkable = false;
                            break;
                        }
                    }
                }
            }
        }
    }

    // 2. Link neighbors (8-way with corner-cutting prevention)
    auto isNodeWalkable = [&](int gx, int gz) {
        if (gx < 0 || gx >= width || gz < 0 || gz >= depth) return false;
        return m_nodes[gz * width + gx].walkable;
    };

    int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    int dz[] = {0, 0, -1, 1, -1, -1, 1, 1};

    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int idx = z * width + x;
            auto& node = m_nodes[idx];
            if (!node.walkable) continue;

            for (int i = 0; i < 8; ++i) {
                int nx = x + dx[i];
                int nz = z + dz[i];
                if (nx >= 0 && nx < width && nz >= 0 && nz < depth) {
                    if (!m_nodes[nz * width + nx].walkable) continue;
                    
                    if (i >= 4) {
                        if (!isNodeWalkable(x + dx[i], z) || !isNodeWalkable(x, z + dz[i])) {
                            continue;
                        }
                    }
                    
                    float hDiff = std::abs(m_nodes[idx].position.y - m_nodes[nz * width + nx].position.y);
                    if (hDiff > 1.5f) continue;

                    node.neighbors.push_back(nz * width + nx);
                }
            }
        }
    }
}

void NavMesh::SetWalkable(int index, bool walkable) {
    if (index >= 0 && index < static_cast<int>(m_nodes.size())) {
        m_nodes[index].walkable = walkable;
        ClearCache();
    }
}

int NavMesh::FindNearestNodeIndex(const glm::vec3& pos) const {
    if (m_nodes.empty()) return -1;
    int nearest = 0;
    float minDist = glm::distance(m_nodes[0].position, pos);
    for (size_t i = 1; i < m_nodes.size(); ++i) {
        float dist = glm::distance(m_nodes[i].position, pos);
        if (dist < minDist) {
            minDist = dist;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

std::vector<glm::vec3> NavMesh::FindPath(const glm::vec3& start, const glm::vec3& end) const {
    PathCacheKey key{start, end};
    auto cachedIt = m_pathCache.find(key);
    if (cachedIt != m_pathCache.end()) {
        return cachedIt->second;
    }

    std::vector<glm::vec3> path;
    int startIndex = FindNearestNodeIndex(start);
    int endIndex = FindNearestNodeIndex(end);

    if (startIndex == -1 || endIndex == -1) {
        return path;
    }
    if (startIndex == endIndex) {
        path.push_back(m_nodes[startIndex].position);
        return path;
    }

    // A* Pathfinding
    struct PathNode {
        int index;
        float gCost;
        float hCost;
        int parentIndex;
        float fCost() const { return gCost + hCost; }
        bool operator>(const PathNode& other) const { return fCost() > other.fCost(); }
    };

    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>> openSet;
    std::unordered_map<int, float> gCosts;
    std::unordered_map<int, int> camelParents;

    openSet.push({startIndex, 0.0f, glm::distance(m_nodes[startIndex].position, m_nodes[endIndex].position), -1});
    gCosts[startIndex] = 0.0f;

    bool found = false;
    while (!openSet.empty()) {
        auto current = openSet.top();
        openSet.pop();

        if (current.index == endIndex) {
            found = true;
            break;
        }

        for (int neighborIdx : m_nodes[current.index].neighbors) {
            if (!m_nodes[neighborIdx].walkable) continue;

            float tentativeG = current.gCost + glm::distance(m_nodes[current.index].position, m_nodes[neighborIdx].position);
            if (gCosts.find(neighborIdx) == gCosts.end() || tentativeG < gCosts[neighborIdx]) {
                gCosts[neighborIdx] = tentativeG;
                camelParents[neighborIdx] = current.index;
                float h = glm::distance(m_nodes[neighborIdx].position, m_nodes[endIndex].position);
                openSet.push({neighborIdx, tentativeG, h, current.index});
            }
        }
    }

    if (found) {
        int curr = endIndex;
        while (curr != startIndex) {
            path.push_back(m_nodes[curr].position);
            curr = camelParents[curr];
        }
        path.push_back(m_nodes[startIndex].position);
        std::reverse(path.begin(), path.end());
        
        // Path smoothing
        path = SmoothPath(path);
        
        // Cache path
        m_pathCache[key] = path;
    }

    return path;
}

bool NavMesh::Save(const std::string& filepath) const {
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;
    
    out.write(reinterpret_cast<const char*>(&m_center), sizeof(m_center));
    out.write(reinterpret_cast<const char*>(&m_width), sizeof(m_width));
    out.write(reinterpret_cast<const char*>(&m_depth), sizeof(m_depth));
    out.write(reinterpret_cast<const char*>(&m_spacing), sizeof(m_spacing));
    
    size_t nodeCount = m_nodes.size();
    out.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
    for (const auto& node : m_nodes) {
        out.write(reinterpret_cast<const char*>(&node.position), sizeof(node.position));
        out.write(reinterpret_cast<const char*>(&node.walkable), sizeof(node.walkable));
        size_t neighborCount = node.neighbors.size();
        out.write(reinterpret_cast<const char*>(&neighborCount), sizeof(neighborCount));
        if (neighborCount > 0) {
            out.write(reinterpret_cast<const char*>(node.neighbors.data()), neighborCount * sizeof(int));
        }
    }
    return true;
}

bool NavMesh::Load(const std::string& filepath) {
    std::vector<uint8_t> buffer = Core::VFS::Get().Read(filepath);
    if (buffer.empty()) return false;
    
    Clear();
    
    size_t offset = 0;
    auto readBytes = [&](void* dest, size_t size) {
        if (offset + size > buffer.size()) return false;
        std::memcpy(dest, &buffer[offset], size);
        offset += size;
        return true;
    };
    
    if (!readBytes(&m_center, sizeof(m_center))) return false;
    if (!readBytes(&m_width, sizeof(m_width))) return false;
    if (!readBytes(&m_depth, sizeof(m_depth))) return false;
    if (!readBytes(&m_spacing, sizeof(m_spacing))) return false;
    
    size_t nodeCount = 0;
    if (!readBytes(&nodeCount, sizeof(nodeCount))) return false;
    m_nodes.resize(nodeCount);
    for (auto& node : m_nodes) {
        if (!readBytes(&node.position, sizeof(node.position))) return false;
        if (!readBytes(&node.walkable, sizeof(node.walkable))) return false;
        size_t neighborCount = 0;
        if (!readBytes(&neighborCount, sizeof(neighborCount))) return false;
        node.neighbors.resize(neighborCount);
        if (neighborCount > 0) {
            if (!readBytes(node.neighbors.data(), neighborCount * sizeof(int))) return false;
        }
    }
    return true;
}

bool NavMesh::ValidatePath(const std::vector<glm::vec3>& path) const {
    if (path.empty()) return false;
    for (const auto& pos : path) {
        int idx = FindNearestNodeIndex(pos);
        if (idx == -1 || !m_nodes[idx].walkable) {
            return false;
        }
    }
    return true;
}

std::vector<glm::vec3> NavMesh::SmoothPath(const std::vector<glm::vec3>& path) const {
    if (path.size() <= 2) return path;
    
    std::vector<glm::vec3> smoothed;
    smoothed.push_back(path.front());
    
    size_t current = 0;
    while (current < path.size() - 1) {
        size_t next = current + 1;
        for (size_t i = path.size() - 1; i > current + 1; --i) {
            if (HasLineOfSight(path[current], path[i])) {
                next = i;
                break;
            }
        }
        smoothed.push_back(path[next]);
        current = next;
    }
    return smoothed;
}

bool NavMesh::HasLineOfSight(const glm::vec3& start, const glm::vec3& end) const {
    float dist = glm::distance(start, end);
    if (dist < 1e-4f) return true;
    
    float stepSize = m_spacing * 0.5f;
    if (stepSize < 0.1f) stepSize = 0.5f;
    
    int steps = static_cast<int>(dist / stepSize) + 1;
    for (int i = 1; i < steps; ++i) {
        float t = static_cast<float>(i) / steps;
        glm::vec3 p = glm::mix(start, end, t);
        int idx = FindNearestNodeIndex(p);
        if (idx == -1 || !m_nodes[idx].walkable) {
            return false;
        }
    }
    return true;
}

// --- AISystem implementation ---

void AISystem::Update(ECS::Registry* registry, float deltaTime) {
    if (!registry) return;

    // 1. Tick Behavior Trees
    registry->Each<BehaviorTreeComponent>([&](ECS::Entity entity, BehaviorTreeComponent& btc) {
        if (registry->HasComponent<BlackboardComponent>(entity)) {
            registry->GetComponent<BlackboardComponent>(entity).SetValue("DeltaTime", deltaTime);
        }
        btc.lastResult = btc.tree.Tick(registry, entity);
    });

    registry->Each<AIComponent>([&](ECS::Entity entity, AIComponent& aic) {
        if (aic.behaviorTree) {
            if (registry->HasComponent<BlackboardComponent>(entity)) {
                registry->GetComponent<BlackboardComponent>(entity).SetValue("DeltaTime", deltaTime);
            }
            BTState state = aic.behaviorTree->Tick(registry, entity);
            if (state == BTState::Success) {
                aic.currentState = "Success";
            } else if (state == BTState::Failure) {
                aic.currentState = "Failure";
            } else {
                aic.currentState = "Running";
            }
        }
    });

    // 2. Process path movements for old NavMesh agents
    registry->Each<NavMeshComponent, Scene::TransformComponent>([&](ECS::Entity entity, NavMeshComponent& agent, Scene::TransformComponent& tc) {
        (void)entity;
        
        if (agent.currentPath.empty() || glm::distance(agent.currentPath.back(), agent.agentTarget) > 1.0f) {
            agent.currentPath = m_navMesh.FindPath(tc.position, agent.agentTarget);
            agent.pathIndex = 0;
        }

        if (agent.pathIndex < agent.currentPath.size()) {
            glm::vec3 targetPos = agent.currentPath[agent.pathIndex];
            targetPos.y = tc.position.y;
            
            float dist = glm::distance(tc.position, targetPos);
            if (dist < 0.2f) {
                agent.pathIndex++;
            } else {
                glm::vec3 dir = glm::normalize(targetPos - tc.position);
                tc.position += dir * agent.agentSpeed * deltaTime;
            }
        }
    });

    // 3. Process path movements for new Navigation Agents (with avoidance and rotation)
    registry->Each<NavigationAgentComponent, Scene::TransformComponent>([&](ECS::Entity entity, NavigationAgentComponent& agent, Scene::TransformComponent& tc) {
        if (agent.currentPath.empty() || glm::distance(agent.currentPath.back(), agent.agentTarget) > 0.5f) {
            agent.currentPath = m_navMesh.FindPath(tc.position, agent.agentTarget);
            agent.pathIndex = 0;
        }

        if (agent.pathIndex < agent.currentPath.size()) {
            glm::vec3 targetPos = agent.currentPath[agent.pathIndex];
            targetPos.y = tc.position.y;
            
            float dist = glm::distance(tc.position, targetPos);
            if (dist < agent.acceptanceRadius) {
                agent.pathIndex++;
            } else {
                glm::vec3 dir = glm::normalize(targetPos - tc.position);
                
                // Avoidance logic
                if (agent.useAvoidance) {
                    glm::vec3 avoidanceForce{0.0f};
                    registry->Each<NavigationAgentComponent, Scene::TransformComponent>([&](ECS::Entity otherEntity, NavigationAgentComponent&, Scene::TransformComponent& otherTc) {
                        if (otherEntity == entity) return;
                        float d = glm::distance(tc.position, otherTc.position);
                        if (d > 0.01f && d < agent.avoidanceRadius) {
                            avoidanceForce += glm::normalize(tc.position - otherTc.position) * (1.0f - (d / agent.avoidanceRadius));
                        }
                    });
                    if (glm::length(avoidanceForce) > 0.01f) {
                        dir = glm::normalize(dir + avoidanceForce * 0.5f);
                    }
                }
                
                tc.position += dir * agent.agentSpeed * deltaTime;
                AIController::RotateTowardsTarget(registry, entity, targetPos, deltaTime);
            }
        }
    });

    // 4. Populate debug draw lines (navmesh mesh + agent paths)
    ClearDebugLines();
    const auto& nodes = m_navMesh.GetNodes();
    for (const auto& node : nodes) {
        if (!node.walkable) continue;
        for (int neighborIdx : node.neighbors) {
            if (neighborIdx < static_cast<int>(nodes.size()) && nodes[neighborIdx].walkable) {
                AddDebugLine(node.position, nodes[neighborIdx].position, glm::vec3(0.0f, 0.7f, 0.0f)); // green navmesh edges
            }
        }
    }

    registry->Each<NavMeshComponent>([&](ECS::Entity entity, NavMeshComponent& agent) {
        (void)entity;
        if (agent.currentPath.size() > 1) {
            for (size_t i = 0; i < agent.currentPath.size() - 1; ++i) {
                AddDebugLine(agent.currentPath[i], agent.currentPath[i+1], glm::vec3(0.0f, 0.0f, 1.0f)); // blue paths
            }
        }
    });

    registry->Each<NavigationAgentComponent>([&](ECS::Entity entity, NavigationAgentComponent& agent) {
        (void)entity;
        if (agent.currentPath.size() > 1) {
            for (size_t i = 0; i < agent.currentPath.size() - 1; ++i) {
                AddDebugLine(agent.currentPath[i], agent.currentPath[i+1], glm::vec3(0.0f, 0.5f, 1.0f)); // cyan paths
            }
        }
    });
}

} // namespace KumariEngine::AI
