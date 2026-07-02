#pragma once
#include "ecs/ecs.hpp"
#include "gameplay/InventorySystem.hpp"
#include <string>
#include <array>

namespace KumariEngine::Gameplay {

enum class EquipmentSlot : uint32_t {
    Head = 0,
    Chest,
    Weapon,
    Shield,
    Boots,
    Count
};

struct EquipmentComponent {
    std::array<std::string, static_cast<size_t>(EquipmentSlot::Count)> slots;

    EquipmentComponent();

    bool EquipItem(EquipmentSlot slot, const std::string& itemId, InventoryComponent* inventory);
    bool UnequipItem(EquipmentSlot slot, InventoryComponent* inventory);
    bool ValidateItemForSlot(EquipmentSlot slot, const std::string& itemId) const;
};

} // namespace KumariEngine::Gameplay
