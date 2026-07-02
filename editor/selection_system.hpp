#pragma once
#include "ecs/ecs.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <vector>

namespace KumariEngine::Editor {

class SelectionSystem {
public:
    static SelectionSystem& Get() {
        static SelectionSystem instance;
        return instance;
    }

    SelectionSystem(const SelectionSystem&) = delete;
    SelectionSystem& operator=(const SelectionSystem&) = delete;

    void Select(ECS::Entity entity);
    void AddToSelection(ECS::Entity entity);
    void RemoveFromSelection(ECS::Entity entity);
    void ToggleSelection(ECS::Entity entity);
    bool IsSelected(ECS::Entity entity) const;
    const std::vector<ECS::Entity>& GetSelection() const { return m_selectedEntities; }

    ECS::Entity GetSelected() const { return m_selectedEntities.empty() ? ECS::NULL_ENTITY : m_selectedEntities.back(); }
    bool HasSelection() const { return !m_selectedEntities.empty(); }
    void ClearSelection();

    void HighlightSelection(ECS::Registry* registry);
    void SelectRectangle(ECS::Registry* registry, const glm::vec2& mouseMin, const glm::vec2& mouseMax, const glm::mat4& viewProjMatrix, const glm::vec2& viewportSize);

    using SelectionCallback = std::function<void(ECS::Entity)>;
    void AddSelectionListener(SelectionCallback callback) { m_listeners.push_back(callback); }
    void ClearListeners() { m_listeners.clear(); }

private:
    SelectionSystem() = default;
    ~SelectionSystem() = default;

    void NotifyListeners();

    std::vector<ECS::Entity> m_selectedEntities;
    std::vector<SelectionCallback> m_listeners;
};

} // namespace KumariEngine::Editor
