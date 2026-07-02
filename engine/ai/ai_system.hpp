#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"



#include "ai_components.hpp"
#include "behavior_tree.hpp"
#include "ai_controller.hpp"

namespace KumariEngine::AI {

// --- Navigation Mesh ---
struct NavNode {
    glm::vec3 position;
    bool walkable = true;
    std::vector<int> neighbors; // neighbor indices in NavMesh list
};

class NavMesh {
public:
    void BuildGrid(const glm::vec3& center, int width, int depth, float spacing);
    void BuildFromWorld(ECS::Registry* registry, const glm::vec3& center, int width, int depth, float spacing, float agentRadius = 0.5f, float agentHeight = 2.0f);
    void SetWalkable(int index, bool walkable);
    std::vector<glm::vec3> FindPath(const glm::vec3& start, const glm::vec3& end) const;
    
    // Runtime loading & saving
    bool Save(const std::string& filepath) const;
    bool Load(const std::string& filepath);
    
    // Path validation & smoothing
    bool ValidatePath(const std::vector<glm::vec3>& path) const;
    std::vector<glm::vec3> SmoothPath(const std::vector<glm::vec3>& path) const;
    bool HasLineOfSight(const glm::vec3& start, const glm::vec3& end) const;
    
    const std::vector<NavNode>& GetNodes() const { return m_nodes; }
    void Clear() { m_nodes.clear(); m_pathCache.clear(); }
    void ClearCache() { m_pathCache.clear(); }
    
    int FindNearestNodeIndex(const glm::vec3& pos) const;
    
private:
    std::vector<NavNode> m_nodes;
    glm::vec3 m_center{0.0f};
    int m_width = 0;
    int m_depth = 0;
    float m_spacing = 0.0f;

    struct PathCacheKey {
        glm::vec3 start;
        glm::vec3 end;
        bool operator==(const PathCacheKey& other) const {
            auto q = [](const glm::vec3& p) {
                return glm::ivec3(std::round(p.x * 10.0f), std::round(p.y * 10.0f), std::round(p.z * 10.0f));
            };
            return q(start) == q(other.start) && q(end) == q(other.end);
        }
    };
    struct PathCacheKeyHash {
        size_t operator()(const PathCacheKey& key) const {
            auto q = [](const glm::vec3& p) {
                return glm::ivec3(std::round(p.x * 10.0f), std::round(p.y * 10.0f), std::round(p.z * 10.0f));
            };
            auto q1 = q(key.start);
            auto q2 = q(key.end);
            return (static_cast<size_t>(q1.x) ^ static_cast<size_t>(q2.x)) ^
                   ((static_cast<size_t>(q1.z) ^ static_cast<size_t>(q2.z)) << 16);
        }
    };
    mutable std::unordered_map<PathCacheKey, std::vector<glm::vec3>, PathCacheKeyHash> m_pathCache;
};

struct NavMeshComponent {
    glm::vec3 agentTarget;
    float agentSpeed = 3.5f;
    std::vector<glm::vec3> currentPath;
    size_t pathIndex = 0;
};

// --- Debug Visualizer & System ---
class AISystem {
public:
    static AISystem& Get() {
        static AISystem instance;
        return instance;
    }
    
    void Update(ECS::Registry* registry, float deltaTime);
    
    NavMesh& GetNavMesh() { return m_navMesh; }
    
    // Debug line submissions (accumulates paths & nav mesh connections)
    struct DebugLine {
        glm::vec3 start;
        glm::vec3 end;
        glm::vec3 color;
    };
    const std::vector<DebugLine>& GetDebugLines() const { return m_debugLines; }
    void ClearDebugLines() { m_debugLines.clear(); }
    void AddDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) {
        m_debugLines.push_back({start, end, color});
    }

private:
    AISystem() = default;
    ~AISystem() = default;
    
    NavMesh m_navMesh;
    std::vector<DebugLine> m_debugLines;
};

} // namespace KumariEngine::AI
