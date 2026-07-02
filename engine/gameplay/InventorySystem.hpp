#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace KumariEngine::Gameplay {

struct ItemDefinition {
    std::string itemId;
    std::string name;
    std::string description;
    bool isStackable = false;
    uint32_t maxStackSize = 1;
    std::vector<std::string> tags;
};

class ItemDatabase {
public:
    static ItemDatabase& Get() {
        static ItemDatabase instance;
        return instance;
    }

    ItemDatabase(const ItemDatabase&) = delete;
    ItemDatabase& operator=(const ItemDatabase&) = delete;

    void RegisterItem(const ItemDefinition& def);
    const ItemDefinition* GetItem(const std::string& itemId) const;
    void Clear();

private:
    ItemDatabase() = default;
    ~ItemDatabase() = default;
    std::unordered_map<std::string, ItemDefinition> m_items;
};

struct InventorySlot {
    std::string itemId;
    uint32_t quantity = 0;

    bool IsEmpty() const { return itemId.empty() || quantity == 0; }
    void Clear() { itemId.clear(); quantity = 0; }
};

struct InventoryComponent {
    std::vector<InventorySlot> slots;
    uint32_t maxSlots = 20;

    InventoryComponent() = default;
    explicit InventoryComponent(uint32_t maxSlots);

    bool AddItem(const std::string& itemId, uint32_t quantity);
    bool RemoveItem(const std::string& itemId, uint32_t quantity);
    bool HasItem(const std::string& itemId, uint32_t quantity) const;
    void Clear();
};

struct ItemComponent {
    std::string itemId;
    uint32_t quantity = 1;
};

bool TransferItem(InventoryComponent& from, uint32_t fromSlotIndex, InventoryComponent& to, uint32_t toSlotIndex, uint32_t quantity);

} // namespace KumariEngine::Gameplay
