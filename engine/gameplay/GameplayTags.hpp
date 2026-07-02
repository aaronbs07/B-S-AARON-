#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include "ecs/ecs.hpp"

namespace KumariEngine::Gameplay {

struct GameplayTag {
    std::string rawTag;
    uint32_t hash = 0;

    GameplayTag() = default;
    explicit GameplayTag(const std::string& tag);

    bool operator==(const GameplayTag& other) const { return hash == other.hash; }
    bool operator!=(const GameplayTag& other) const { return hash != other.hash; }
    bool operator<(const GameplayTag& other) const { return hash < other.hash; }

    // Matches: returns true if this tag matches the query tag hierarchically
    // e.g. "Character.Player.Mage".Matches("Character.Player") is true
    bool Matches(const GameplayTag& other) const;

    static uint32_t CalculateHash(const std::string& tag);
};

struct GameplayTagsComponent {
    std::vector<GameplayTag> tags;

    void AddTag(const std::string& tagStr);
    void RemoveTag(const std::string& tagStr);
    bool HasTag(const std::string& tagStr) const;
    bool HasTagExact(const std::string& tagStr) const;
    bool HasTag(const GameplayTag& tag) const;
    bool HasTagExact(const GameplayTag& tag) const;

    std::vector<std::string> GetRawTags() const;
};

} // namespace KumariEngine::Gameplay
