#include "GameplayTags.hpp"
#include <sstream>
#include <algorithm>

namespace KumariEngine::Gameplay {

uint32_t GameplayTag::CalculateHash(const std::string& tag) {
    // FNV-1a 32-bit hash
    uint32_t hash = 2166136261u;
    for (char c : tag) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

GameplayTag::GameplayTag(const std::string& tag)
    : rawTag(tag), hash(CalculateHash(tag)) {}

bool GameplayTag::Matches(const GameplayTag& other) const {
    if (hash == other.hash) {
        return true;
    }
    // Check hierarchical: other must be a parent prefix of this tag
    // e.g. this is "A.B.C" and other is "A.B" -> other.rawTag + "." is "A.B."
    // and "A.B.C" starts with "A.B."
    if (rawTag.size() > other.rawTag.size()) {
        if (rawTag.compare(0, other.rawTag.size(), other.rawTag) == 0) {
            if (rawTag[other.rawTag.size()] == '.') {
                return true;
            }
        }
    }
    return false;
}

void GameplayTagsComponent::AddTag(const std::string& tagStr) {
    if (tagStr.empty()) return;
    GameplayTag tag(tagStr);
    auto it = std::find_if(tags.begin(), tags.end(), [&](const GameplayTag& t) {
        return t.hash == tag.hash;
    });
    if (it == tags.end()) {
        tags.push_back(tag);
    }
}

void GameplayTagsComponent::RemoveTag(const std::string& tagStr) {
    uint32_t hash = GameplayTag::CalculateHash(tagStr);
    tags.erase(std::remove_if(tags.begin(), tags.end(), [&](const GameplayTag& t) {
        return t.hash == hash;
    }), tags.end());
}

bool GameplayTagsComponent::HasTag(const std::string& tagStr) const {
    GameplayTag queryTag(tagStr);
    return HasTag(queryTag);
}

bool GameplayTagsComponent::HasTagExact(const std::string& tagStr) const {
    GameplayTag queryTag(tagStr);
    return HasTagExact(queryTag);
}

bool GameplayTagsComponent::HasTag(const GameplayTag& tag) const {
    for (const auto& t : tags) {
        if (t.Matches(tag)) {
            return true;
        }
    }
    return false;
}

bool GameplayTagsComponent::HasTagExact(const GameplayTag& tag) const {
    for (const auto& t : tags) {
        if (t.hash == tag.hash) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> GameplayTagsComponent::GetRawTags() const {
    std::vector<std::string> raw;
    raw.reserve(tags.size());
    for (const auto& t : tags) {
        raw.push_back(t.rawTag);
    }
    return raw;
}

} // namespace KumariEngine::Gameplay
