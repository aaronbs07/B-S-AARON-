#include "gameplay_inspector_window.hpp"
#include "gameplay/GameInstance.hpp"
#include "gameplay/World.hpp"
#include "gameplay/Level.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/InventorySystem.hpp"
#include "gameplay/EquipmentSystem.hpp"
#include "gameplay/CraftingSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/DialogueSystem.hpp"
#include "gameplay/InteractionSystem.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

void GameplayInspectorWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Gameplay Framework Inspector ---");

    auto* registry = WindowSystem::Get().GetRegistry();
    if (!registry) {
        Core::Logger::Info("EditorUI", "  Registry is null.");
        return;
    }

    auto& gi = Gameplay::GameInstance::Get();
    auto world = gi.GetWorld();

    if (!world) {
        Core::Logger::Info("EditorUI", "  No active game world.");
        return;
    }

    // 1. World & Levels
    Core::Logger::Info("EditorUI", "  Game World active levels:");
    auto persistent = world->GetPersistentLevel();
    if (persistent) {
        Core::Logger::Info("EditorUI", "    Persistent Level: '%s' (Active: %s, Visible: %s, Entities: %zu)",
                           persistent->GetName().c_str(),
                           persistent->IsActive() ? "Yes" : "No",
                           persistent->IsVisible() ? "Yes" : "No",
                           persistent->GetEntities().size());
    }

    const auto& subLevels = world->GetSubLevels();
    Core::Logger::Info("EditorUI", "    Sublevels loaded: %zu", subLevels.size());
    for (const auto& level : subLevels) {
        Core::Logger::Info("EditorUI", "      Sublevel: '%s' (Active: %s, Visible: %s, Entities: %zu)",
                           level->GetName().c_str(),
                           level->IsActive() ? "Yes" : "No",
                           level->IsVisible() ? "Yes" : "No",
                           level->GetEntities().size());
    }

    // 2. GameState
    registry->Each<Gameplay::GameStateComponent>([&](auto entity, const Gameplay::GameStateComponent& gs) {
        Core::Logger::Info("EditorUI", "  GameState (Entity %u):", entity);
        Core::Logger::Info("EditorUI", "    Match running: %s", gs.isMatchRunning ? "Yes" : "No");
        Core::Logger::Info("EditorUI", "    Match over: %s", gs.isMatchOver ? "Yes" : "No");
        Core::Logger::Info("EditorUI", "    Elapsed time: %.2fs", gs.elapsedTime);
        Core::Logger::Info("EditorUI", "    Winner team ID: %d", gs.winnerTeamId);
    });

    // 3. PlayerControllers and PlayerStates
    Core::Logger::Info("EditorUI", "  Active Player Controllers:");
    registry->Each<Gameplay::PlayerControllerComponent>([&](auto entity, const Gameplay::PlayerControllerComponent& pc) {
        std::string controllerType = pc.isLocal ? "Local" : "Remote";
        Core::Logger::Info("EditorUI", "    Controller (Entity %u) [Peer %u, %s]:", entity, pc.peerId, controllerType.c_str());
        Core::Logger::Info("EditorUI", "      Possessed pawn: Entity %u", pc.possessedPawn);

        if (registry->IsAlive(pc.playerStateEntity) && registry->HasComponent<Gameplay::PlayerStateComponent>(pc.playerStateEntity)) {
            const auto& ps = registry->GetComponent<Gameplay::PlayerStateComponent>(pc.playerStateEntity);
            Core::Logger::Info("EditorUI", "      PlayerState: '%s' (Team: %d, Score: %.1f, Ping: %.1fms)",
                               ps.playerName.c_str(), ps.teamId, ps.score, ps.ping);
        }
    });

    // 4. Inventories
    Core::Logger::Info("EditorUI", "  Inventories:");
    registry->Each<Gameplay::InventoryComponent>([&](auto entity, const Gameplay::InventoryComponent& ic) {
        Core::Logger::Info("EditorUI", "    Inventory (Entity %u): Max Slots: %u", entity, ic.maxSlots);
        for (size_t i = 0; i < ic.slots.size(); ++i) {
            const auto& slot = ic.slots[i];
            if (!slot.itemId.empty()) {
                Core::Logger::Info("EditorUI", "      Slot %zu: Item '%s' x%u", i, slot.itemId.c_str(), slot.quantity);
            }
        }
    });

    // 5. Equipment
    Core::Logger::Info("EditorUI", "  Equipment:");
    registry->Each<Gameplay::EquipmentComponent>([&](auto entity, const Gameplay::EquipmentComponent& ec) {
        Core::Logger::Info("EditorUI", "    Equipment (Entity %u):", entity);
        for (size_t i = 0; i < ec.slots.size(); ++i) {
            const auto& item = ec.slots[i];
            if (!item.empty()) {
                Core::Logger::Info("EditorUI", "      Slot %zu: '%s'", i, item.c_str());
            }
        }
    });

    // 6. Quests
    Core::Logger::Info("EditorUI", "  Active Quests status:");
    registry->Each<Gameplay::QuestComponent>([&](auto entity, const Gameplay::QuestComponent& qc) {
        Core::Logger::Info("EditorUI", "    Quest Journal (Entity %u): Completed Quests: %zu, Active Quests: %zu",
                           entity, qc.completedQuests.size(), qc.activeQuests.size());
        for (const auto& [questId, state] : qc.activeQuests) {
            Core::Logger::Info("EditorUI", "      Quest: '%s' (Stage index: %d, Completed: %s)",
                               questId.c_str(), state.currentStageIndex, state.isCompleted ? "Yes" : "No");
            for (const auto& [objId, progressCount] : state.objectiveProgress) {
                Core::Logger::Info("EditorUI", "        Objective '%s': progress %d", objId.c_str(), progressCount);
            }
        }
    });

    // 7. Dialogue Component State
    registry->Each<Gameplay::DialogueComponent>([&](auto entity, const Gameplay::DialogueComponent& dc) {
        Core::Logger::Info("EditorUI", "  Dialogue state (Entity %u):", entity);
        Core::Logger::Info("EditorUI", "    In dialogue: %s", dc.isInDialogue ? "Yes" : "No");
        Core::Logger::Info("EditorUI", "    Current Dialogue: '%s', Node: '%s'", dc.currentDialogueId.c_str(), dc.currentNodeId.c_str());
    });

    // 8. Interaction System Components
    registry->Each<Gameplay::InteractableComponent>([&](auto entity, const Gameplay::InteractableComponent& ic) {
        Core::Logger::Info("EditorUI", "  Interactable (Entity %u):", entity);
        Core::Logger::Info("EditorUI", "    Prompt: '%s' (Distance: %.1fm)", ic.prompt.c_str(), ic.distance);
        Core::Logger::Info("EditorUI", "    Active: %s (Type: '%s', Target: '%s')",
                           ic.isInteractable ? "Yes" : "No", ic.interactionType.c_str(), ic.targetData.c_str());
    });
}

} // namespace KumariEngine::Editor
