#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.hpp"
#include "save/EntityGUID.hpp"
#include <unordered_map>
#include <mutex>

namespace KumariEngine::Networking {

struct SavedEntityState {
    glm::vec3 position;
    glm::quat rotation;
};

class LagCompensator {
public:
    static LagCompensator& Get() {
        static LagCompensator instance;
        return instance;
    }

    LagCompensator(const LagCompensator&) = delete;
    LagCompensator& operator=(const LagCompensator&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Rewinds all replicated entities to their interpolated states at the specified target time
    void Rewind(double targetTimeMs);

    // Restores all entities to their state prior to the rewind operation
    void Restore();

    // Verification helper to see if we are currently rewound
    bool IsRewound() const { return !m_originalStates.empty(); }
    const std::unordered_map<Save::EntityGUID, SavedEntityState>& GetOriginalStates() const { return m_originalStates; }

private:
    LagCompensator() = default;
    ~LagCompensator() = default;

    ECS::Registry* m_registry = nullptr;
    mutable std::mutex m_mutex;

    std::unordered_map<Save::EntityGUID, SavedEntityState> m_originalStates;
};

} // namespace KumariEngine::Networking
