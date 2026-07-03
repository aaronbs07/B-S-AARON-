#include "editor_manager.hpp"
#include "core/engine_state.hpp"
#include "core/logger.hpp"
#include "save/SaveManager.hpp"
#include "gameplay/GameInstance.hpp"
#include "undo_redo.hpp"
#include <filesystem>

namespace KumariEngine::Editor {

void EditorManager::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    m_state = PlayModeState::Edit;
    m_timeScale = 1.0f;
    m_hasSnapshot = false;
    Core::Logger::Info("EditorManager", "EditorManager initialized.");
}

void EditorManager::Shutdown() {
    if (m_hasSnapshot) {
        std::error_code ec;
        std::filesystem::remove(m_tempSnapshotPath, ec);
    }
    m_registry = nullptr;
    Core::Logger::Info("EditorManager", "EditorManager shut down.");
}

void EditorManager::Update(float deltaTime) {
    (void)deltaTime;

    // If StepFrame() was called on the previous tick, the engine ran one frame
    // while in Running state. Re-enter Pause now so it stops after exactly one step.
    if (m_pendingRePause) {
        m_pendingRePause = false;
        m_state = PlayModeState::Pause;
        Kumari::EngineStateManager::Get().SetState(Kumari::EngineState::Paused);
        Core::Logger::Info("EditorManager", "Step frame complete. Engine re-paused.");
    }
}

void EditorManager::EnterPlayMode() {
    if (m_state != PlayModeState::Edit) return;

    Core::Logger::Info("EditorManager", "Entering Play Mode... Saving scene snapshot.");
    if (m_registry) {
        if (Save::SaveManager::Get().SaveGame(m_tempSnapshotPath, m_registry)) {
            m_hasSnapshot = true;
        } else {
            Core::Logger::Error("EditorManager", "Failed to save scene snapshot before playing.");
        }
    }

    m_state = PlayModeState::Play;
    Gameplay::GameInstance::Get().SetEditorMode(false);
    Kumari::EngineStateManager::Get().SetState(Kumari::EngineState::Running);
    Core::Logger::Info("EditorManager", "Play Mode started.");
}

void EditorManager::ExitPlayMode() {
    if (m_state == PlayModeState::Edit) return;

    Core::Logger::Info("EditorManager", "Stopping Play Mode... Restoring scene snapshot.");
    m_state = PlayModeState::Edit;
    Gameplay::GameInstance::Get().SetEditorMode(true);
    Kumari::EngineStateManager::Get().SetState(Kumari::EngineState::Running); // Editor runs under Running state too

    if (m_hasSnapshot && m_registry) {
        // Clear current entities
        auto aliveEntities = m_registry->GetAliveEntities();
        for (auto entity : aliveEntities) {
            m_registry->DestroyEntity(entity);
        }
        // Load the snapshot
        if (Save::SaveManager::Get().LoadGame(m_tempSnapshotPath, m_registry)) {
            Core::Logger::Info("EditorManager", "Scene snapshot restored successfully.");
        } else {
            Core::Logger::Error("EditorManager", "Failed to restore scene snapshot.");
        }
        std::error_code ec;
        std::filesystem::remove(m_tempSnapshotPath, ec);
        m_hasSnapshot = false;
    }

    // Reset undo history on play mode exit
    UndoSystem::Get().Clear();
    Core::Logger::Info("EditorManager", "Play Mode stopped.");
}

void EditorManager::PausePlayMode() {
    if (m_state != PlayModeState::Play) return;

    m_state = PlayModeState::Pause;
    Kumari::EngineStateManager::Get().SetState(Kumari::EngineState::Paused);
    Core::Logger::Info("EditorManager", "Play Mode paused.");
}

void EditorManager::StepFrame() {
    if (m_state != PlayModeState::Pause) {
        PausePlayMode();
    }
    Core::Logger::Info("EditorManager", "Stepping one frame.");

    // Allow the engine to run one update tick by temporarily entering Running.
    // m_pendingRePause tells Update() to re-enter Pause on the very next call,
    // i.e. after that single tick has been dispatched by the engine loop.
    m_pendingRePause = true;
    Kumari::EngineStateManager::Get().SetState(Kumari::EngineState::Running);
}

} // namespace KumariEngine::Editor
