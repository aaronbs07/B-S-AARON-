#pragma once
#include "ecs/ecs.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace KumariEngine::Gameplay {

struct DialogueChoice {
    std::string text;
    std::string nextNodeId;
    std::string conditionLua; // Lua condition code/function returning bool
};

struct DialogueNode {
    std::string nodeId;
    std::string speaker;
    std::string text;
    std::vector<DialogueChoice> choices;
    std::string nextNodeId; // Default next if no choices
    std::string callbackLua; // Lua function/code to run when this node is active
};

struct DialogueTree {
    std::string dialogueId;
    std::string startNodeId;
    std::unordered_map<std::string, DialogueNode> nodes;
};

struct DialogueComponent {
    std::string currentDialogueId;
    std::string currentNodeId;
    bool isInDialogue = false;
};

class DialogueDatabase {
public:
    static DialogueDatabase& Get() {
        static DialogueDatabase instance;
        return instance;
    }

    DialogueDatabase(const DialogueDatabase&) = delete;
    DialogueDatabase& operator=(const DialogueDatabase&) = delete;

    void RegisterDialogue(const DialogueTree& tree);
    const DialogueTree* GetDialogue(const std::string& dialogueId) const;
    void Clear();

    bool StartDialogue(ECS::Registry* registry, ECS::Entity player, const std::string& dialogueId);
    bool ChooseOption(ECS::Registry* registry, ECS::Entity player, size_t choiceIndex);
    const DialogueNode* GetCurrentNode(ECS::Registry* registry, ECS::Entity player) const;

private:
    DialogueDatabase() = default;
    ~DialogueDatabase() = default;
    std::unordered_map<std::string, DialogueTree> m_dialogues;
};

} // namespace KumariEngine::Gameplay
