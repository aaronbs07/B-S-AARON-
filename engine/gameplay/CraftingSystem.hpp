#pragma once
#include "ecs/ecs.hpp"
#include "gameplay/InventorySystem.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace KumariEngine::Gameplay {

struct RecipeIngredient {
    std::string itemId;
    uint32_t quantity = 0;
};

struct CraftingRecipe {
    std::string recipeId;
    std::vector<RecipeIngredient> ingredients;
    std::string outputItemId;
    uint32_t outputQuantity = 1;
    std::string requiredStation;
};

struct CraftingStationComponent {
    std::string stationType;
};

class RecipeDatabase {
public:
    static RecipeDatabase& Get() {
        static RecipeDatabase instance;
        return instance;
    }

    RecipeDatabase(const RecipeDatabase&) = delete;
    RecipeDatabase& operator=(const RecipeDatabase&) = delete;

    void RegisterRecipe(const CraftingRecipe& recipe);
    const CraftingRecipe* GetRecipe(const std::string& recipeId) const;
    void Clear();

private:
    RecipeDatabase() = default;
    ~RecipeDatabase() = default;
    std::unordered_map<std::string, CraftingRecipe> m_recipes;
};

bool CanCraft(const std::string& recipeId, const InventoryComponent& inventory, const std::string& nearStationType);
bool ExecuteCraft(const std::string& recipeId, InventoryComponent& inventory, const std::string& nearStationType);

} // namespace KumariEngine::Gameplay
