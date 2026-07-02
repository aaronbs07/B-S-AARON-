#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace KumariEngine::Gameplay {

struct QuestObjective {
    std::string objectiveId;
    std::string description;
    std::string type; // "Kill", "Collect", "Interact", "Location"
    std::string targetId;
    int requiredCount = 1;
    int currentCount = 0;
};

struct QuestReward {
    std::string rewardType; // "Item", "Gold", "XP"
    std::string targetId;   // e.g. itemId
    int quantity = 1;
};

struct QuestStage {
    int stageIndex = 0;
    std::string stageDescription;
    std::vector<QuestObjective> objectives;
};

struct QuestDefinition {
    std::string questId;
    std::string name;
    std::string description;
    std::vector<QuestStage> stages;
    std::vector<QuestReward> rewards;
};

struct QuestState {
    std::string questId;
    int currentStageIndex = 0;
    bool isCompleted = false;
    std::unordered_map<std::string, int> objectiveProgress; // objectiveId -> currentCount
};

struct QuestComponent {
    std::unordered_map<std::string, QuestState> activeQuests;
    std::vector<std::string> completedQuests;
};

class QuestManager {
public:
    static QuestManager& Get() {
        static QuestManager instance;
        return instance;
    }

    QuestManager(const QuestManager&) = delete;
    QuestManager& operator=(const QuestManager&) = delete;

    void RegisterQuest(const QuestDefinition& def);
    const QuestDefinition* GetQuest(const std::string& questId) const;
    void Clear();

    bool AcceptQuest(ECS::Registry* registry, ECS::Entity player, const std::string& questId);
    void ProgressObjective(ECS::Registry* registry, ECS::Entity player, const std::string& type, const std::string& targetId, int amount);
    void CompleteQuest(ECS::Registry* registry, ECS::Entity player, const std::string& questId);

private:
    QuestManager() = default;
    ~QuestManager() = default;
    std::unordered_map<std::string, QuestDefinition> m_quests;
};

} // namespace KumariEngine::Gameplay
