#include "gameplay/EquipmentSystem.hpp"
#include <algorithm>

namespace KumariEngine::Gameplay {

EquipmentComponent::EquipmentComponent() {
    slots.fill("");
}

bool EquipmentComponent::ValidateItemForSlot(EquipmentSlot slot, const std::string& itemId) const {
    if (itemId.empty()) return true;

    const auto* def = ItemDatabase::Get().GetItem(itemId);
    if (!def) return false;

    // Check tags to validate slot compatibility
    std::string requiredTag;
    switch (slot) {
        case EquipmentSlot::Head:   requiredTag = "Helmet"; break;
        case EquipmentSlot::Chest:  requiredTag = "Armor";  break;
        case EquipmentSlot::Weapon: requiredTag = "Weapon"; break;
        case EquipmentSlot::Shield: requiredTag = "Shield"; break;
        case EquipmentSlot::Boots:  requiredTag = "Boots";  break;
        default: return false;
    }

    auto it = std::find(def->tags.begin(), def->tags.end(), requiredTag);
    if (it != def->tags.end()) {
        return true;
    }

    // Fallback search: check for lower-case or alt tags
    if (slot == EquipmentSlot::Head) {
        if (std::find(def->tags.begin(), def->tags.end(), "Head") != def->tags.end()) return true;
    }
    if (slot == EquipmentSlot::Chest) {
        if (std::find(def->tags.begin(), def->tags.end(), "Chest") != def->tags.end()) return true;
    }

    return false;
}

bool EquipmentComponent::EquipItem(EquipmentSlot slot, const std::string& itemId, InventoryComponent* inventory) {
    size_t slotIdx = static_cast<size_t>(slot);
    if (slotIdx >= slots.size()) return false;

    if (!ValidateItemForSlot(slot, itemId)) {
        return false;
    }

    // If we have an inventory, make sure we have the item in it (or ignore if inventory is null)
    if (inventory && !itemId.empty()) {
        if (!inventory->HasItem(itemId, 1)) {
            return false;
        }
    }

    // Handle swap: unequip currently equipped item first
    std::string previouslyEquipped = slots[slotIdx];
    if (!previouslyEquipped.empty()) {
        if (!UnequipItem(slot, inventory)) {
            return false; // Could not unequip (e.g. inventory full)
        }
    }

    // Consume from inventory
    if (inventory && !itemId.empty()) {
        inventory->RemoveItem(itemId, 1);
    }

    slots[slotIdx] = itemId;
    return true;
}

bool EquipmentComponent::UnequipItem(EquipmentSlot slot, InventoryComponent* inventory) {
    size_t slotIdx = static_cast<size_t>(slot);
    if (slotIdx >= slots.size()) return false;

    std::string itemId = slots[slotIdx];
    if (itemId.empty()) return true; // Already empty

    // Put item back into inventory if provided
    if (inventory) {
        if (!inventory->AddItem(itemId, 1)) {
            return false; // Inventory full
        }
    }

    slots[slotIdx] = "";
    return true;
}

} // namespace KumariEngine::Gameplay
