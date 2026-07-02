#include "gameplay/InventorySystem.hpp"
#include <algorithm>

namespace KumariEngine::Gameplay {

// ItemDatabase Implementation
void ItemDatabase::RegisterItem(const ItemDefinition& def) {
    m_items[def.itemId] = def;
}

const ItemDefinition* ItemDatabase::GetItem(const std::string& itemId) const {
    auto it = m_items.find(itemId);
    if (it != m_items.end()) {
        return &it->second;
    }
    return nullptr;
}

void ItemDatabase::Clear() {
    m_items.clear();
}

// InventoryComponent Implementation
InventoryComponent::InventoryComponent(uint32_t maxSlots) : maxSlots(maxSlots) {
    slots.resize(maxSlots);
}

bool InventoryComponent::AddItem(const std::string& itemId, uint32_t quantity) {
    if (quantity == 0) return true;
    const auto* def = ItemDatabase::Get().GetItem(itemId);
    if (!def) return false;

    uint32_t remaining = quantity;

    // 1. Try to stack onto existing items if stackable
    if (def->isStackable) {
        for (auto& slot : slots) {
            if (slot.itemId == itemId) {
                uint32_t space = (def->maxStackSize > slot.quantity) ? (def->maxStackSize - slot.quantity) : 0;
                if (space > 0) {
                    uint32_t toAdd = (std::min)(remaining, space);
                    slot.quantity += toAdd;
                    remaining -= toAdd;
                    if (remaining == 0) return true;
                }
            }
        }
    }

    // 2. Try to fill empty slots
    for (auto& slot : slots) {
        if (slot.IsEmpty()) {
            uint32_t maxStack = def->isStackable ? def->maxStackSize : 1;
            uint32_t toAdd = (std::min)(remaining, maxStack);
            slot.itemId = itemId;
            slot.quantity = toAdd;
            remaining -= toAdd;
            
            // Loop in case we need multiple new slots (e.g. adding 100 stackables with max stack 64)
            if (remaining == 0) return true;
        }
    }

    return remaining == 0;
}

bool InventoryComponent::RemoveItem(const std::string& itemId, uint32_t quantity) {
    if (quantity == 0) return true;
    if (!HasItem(itemId, quantity)) return false;

    uint32_t remaining = quantity;
    for (auto& slot : slots) {
        if (slot.itemId == itemId) {
            if (slot.quantity >= remaining) {
                slot.quantity -= remaining;
                if (slot.quantity == 0) {
                    slot.Clear();
                }
                remaining = 0;
                break;
            } else {
                remaining -= slot.quantity;
                slot.Clear();
            }
        }
    }
    return remaining == 0;
}

bool InventoryComponent::HasItem(const std::string& itemId, uint32_t quantity) const {
    if (quantity == 0) return true;
    uint32_t count = 0;
    for (const auto& slot : slots) {
        if (slot.itemId == itemId) {
            count += slot.quantity;
            if (count >= quantity) return true;
        }
    }
    return false;
}

void InventoryComponent::Clear() {
    for (auto& slot : slots) {
        slot.Clear();
    }
}

// Global helper: TransferItem
bool TransferItem(InventoryComponent& from, uint32_t fromSlotIndex, InventoryComponent& to, uint32_t toSlotIndex, uint32_t quantity) {
    if (fromSlotIndex >= from.slots.size() || toSlotIndex >= to.slots.size() || quantity == 0) {
        return false;
    }

    auto& fromSlot = from.slots[fromSlotIndex];
    auto& toSlot = to.slots[toSlotIndex];

    if (fromSlot.IsEmpty() || fromSlot.quantity < quantity) {
        return false;
    }

    std::string itemId = fromSlot.itemId;
    const auto* def = ItemDatabase::Get().GetItem(itemId);
    if (!def) return false;

    // Validate target slot compatibility
    if (!toSlot.IsEmpty() && toSlot.itemId != itemId) {
        // Target slot occupied by a different item type
        return false;
    }

    uint32_t targetMax = def->isStackable ? def->maxStackSize : 1;
    uint32_t currentTargetQty = toSlot.IsEmpty() ? 0 : toSlot.quantity;
    uint32_t space = (targetMax > currentTargetQty) ? (targetMax - currentTargetQty) : 0;

    if (space < quantity) {
        return false; // Not enough space in the target slot
    }

    // Execute transfer
    if (toSlot.IsEmpty()) {
        toSlot.itemId = itemId;
        toSlot.quantity = quantity;
    } else {
        toSlot.quantity += quantity;
    }

    fromSlot.quantity -= quantity;
    if (fromSlot.quantity == 0) {
        fromSlot.Clear();
    }

    return true;
}

} // namespace KumariEngine::Gameplay
