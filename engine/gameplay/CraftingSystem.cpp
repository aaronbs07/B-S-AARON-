#include "gameplay/CraftingSystem.hpp"

namespace KumariEngine::Gameplay {

// RecipeDatabase Implementation
void RecipeDatabase::RegisterRecipe(const CraftingRecipe& recipe) {
    m_recipes[recipe.recipeId] = recipe;
}

const CraftingRecipe* RecipeDatabase::GetRecipe(const std::string& recipeId) const {
    auto it = m_recipes.find(recipeId);
    if (it != m_recipes.end()) {
        return &it->second;
    }
    return nullptr;
}

void RecipeDatabase::Clear() {
    m_recipes.clear();
}

// Validation & Execution
bool CanCraft(const std::string& recipeId, const InventoryComponent& inventory, const std::string& nearStationType) {
    const auto* recipe = RecipeDatabase::Get().GetRecipe(recipeId);
    if (!recipe) return false;

    // Check station requirement
    if (!recipe->requiredStation.empty() && recipe->requiredStation != nearStationType) {
        return false;
    }

    // Check ingredient quantities
    for (const auto& ing : recipe->ingredients) {
        if (!inventory.HasItem(ing.itemId, ing.quantity)) {
            return false;
        }
    }

    return true;
}

bool ExecuteCraft(const std::string& recipeId, InventoryComponent& inventory, const std::string& nearStationType) {
    if (!CanCraft(recipeId, inventory, nearStationType)) {
        return false;
    }

    const auto* recipe = RecipeDatabase::Get().GetRecipe(recipeId);
    if (!recipe) return false;

    // Consume ingredients
    for (const auto& ing : recipe->ingredients) {
        inventory.RemoveItem(ing.itemId, ing.quantity);
    }

    // Produce output item
    inventory.AddItem(recipe->outputItemId, recipe->outputQuantity);
    return true;
}

} // namespace KumariEngine::Gameplay
