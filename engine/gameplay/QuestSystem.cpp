#include "gameplay/QuestSystem.hpp"
#include "gameplay/InventorySystem.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Gameplay {

// QuestManager Implementation
void QuestManager::RegisterQuest(const QuestDefinition& def) {
    m_quests[def.questId] = def;
}

const QuestDefinition* QuestManager::GetQuest(const std::string& questId) const {
    auto it = m_quests.find(questId);
    if (it != m_quests.end()) {
        return &it->second;
    }
    return nullptr;
}

void QuestManager::Clear() {
    m_quests.clear();
}

bool QuestManager::AcceptQuest(ECS::Registry* registry, ECS::Entity player, const std::string& questId) {
    if (!registry || !registry->IsAlive(player)) return false;

    const auto* def = GetQuest(questId);
    if (!def) return false;

    if (!registry->HasComponent<QuestComponent>(player)) {
        registry->AddComponent<QuestComponent>(player);
    }

    auto& qc = registry->GetComponent<QuestComponent>(player);
    
    // Check if already active or completed
    if (qc.activeQuests.find(questId) != qc.activeQuests.end()) return false;
    if (std::find(qc.completedQuests.begin(), qc.completedQuests.end(), questId) != qc.completedQuests.end()) return false;

    QuestState state;
    state.questId = questId;
    state.currentStageIndex = 0;
    state.isCompleted = false;

    // Initialize objective progress for the first stage
    if (!def->stages.empty()) {
        for (const auto& obj : def->stages[0].objectives) {
            state.objectiveProgress[obj.objectiveId] = 0;
        }
    }

    qc.activeQuests[questId] = state;
    Core::Logger::Info("QuestSystem", "Player %u accepted quest: %s", player, def->name.c_str());
    return true;
}

void QuestManager::ProgressObjective(ECS::Registry* registry, ECS::Entity player, const std::string& type, const std::string& targetId, int amount) {
    if (!registry || !registry->IsAlive(player) || !registry->HasComponent<QuestComponent>(player)) return;

    auto& qc = registry->GetComponent<QuestComponent>(player);
    std::vector<std::string> completedQuestIds;

    for (auto& [questId, state] : qc.activeQuests) {
        const auto* def = GetQuest(questId);
        if (!def) continue;

        if (state.currentStageIndex >= static_cast<int>(def->stages.size())) continue;

        const auto& stage = def->stages[state.currentStageIndex];
        bool stageChanged = false;

        for (const auto& obj : stage.objectives) {
            if (obj.type == type && obj.targetId == targetId) {
                int current = state.objectiveProgress[obj.objectiveId];
                state.objectiveProgress[obj.objectiveId] = (std::min)(obj.requiredCount, current + amount);
                Core::Logger::Info("QuestSystem", "Quest %s, stage %d, objective %s progressed: %d/%d", 
                                   questId.c_str(), state.currentStageIndex, obj.objectiveId.c_str(), 
                                   state.objectiveProgress[obj.objectiveId], obj.requiredCount);
            }
        }

        // Check if all stage objectives are met
        bool stageCompleted = true;
        for (const auto& obj : stage.objectives) {
            if (state.objectiveProgress[obj.objectiveId] < obj.requiredCount) {
                stageCompleted = false;
                break;
            }
        }

        if (stageCompleted) {
            state.currentStageIndex++;
            stageChanged = true;
            
            if (state.currentStageIndex >= static_cast<int>(def->stages.size())) {
                completedQuestIds.push_back(questId);
            } else {
                // Initialize next stage objectives
                state.objectiveProgress.clear();
                const auto& nextStage = def->stages[state.currentStageIndex];
                for (const auto& obj : nextStage.objectives) {
                    state.objectiveProgress[obj.objectiveId] = 0;
                }
                Core::Logger::Info("QuestSystem", "Quest %s advanced to stage %d", questId.c_str(), state.currentStageIndex);
            }
        }
    }

    // Complete quests
    for (const auto& qId : completedQuestIds) {
        CompleteQuest(registry, player, qId);
    }
}

void QuestManager::CompleteQuest(ECS::Registry* registry, ECS::Entity player, const std::string& questId) {
    if (!registry || !registry->IsAlive(player) || !registry->HasComponent<QuestComponent>(player)) return;

    auto& qc = registry->GetComponent<QuestComponent>(player);
    auto it = qc.activeQuests.find(questId);
    if (it == qc.activeQuests.end()) return;

    const auto* def = GetQuest(questId);
    if (!def) return;

    // Disburse rewards
    for (const auto& reward : def->rewards) {
        if (reward.rewardType == "Item") {
            if (registry->HasComponent<InventoryComponent>(player)) {
                auto& inv = registry->GetComponent<InventoryComponent>(player);
                inv.AddItem(reward.targetId, reward.quantity);
            }
        }
        // Gold / XP rewards can be handled by other systems or events if needed
    }

    qc.completedQuests.push_back(questId);
    qc.activeQuests.erase(it);
    Core::Logger::Info("QuestSystem", "Player %u completed quest: %s", player, def->name.c_str());
}

} // namespace KumariEngine::Gameplay
