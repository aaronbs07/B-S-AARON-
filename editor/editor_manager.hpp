#pragma once
#include <string>
#include <memory>
#include "ecs/ecs.hpp"

namespace KumariEngine::Editor {

enum class PlayModeState {
    Edit,
    Play,
    Pause
};

class EditorManager {
public:
    static EditorManager& Get() {
        static EditorManager instance;
        return instance;
    }

    EditorManager(const EditorManager&) = delete;
    EditorManager& operator=(const EditorManager&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();
    void Update(float deltaTime);

    void EnterPlayMode();
    void ExitPlayMode();
    void PausePlayMode();
    void StepFrame();

    PlayModeState GetPlayModeState() const { return m_state; }
    float GetTimeScale() const { return m_timeScale; }
    void SetTimeScale(float scale) { m_timeScale = scale; }

    bool IsPlaying() const { return m_state == PlayModeState::Play; }
    bool IsPaused() const { return m_state == PlayModeState::Pause; }
    bool IsEditing() const { return m_state == PlayModeState::Edit; }

private:
    EditorManager() = default;
    ~EditorManager() = default;

    ECS::Registry* m_registry = nullptr;
    PlayModeState m_state = PlayModeState::Edit;
    float m_timeScale = 1.0f;
    std::string m_tempSnapshotPath = "editor_play_snapshot.sav";
    bool m_hasSnapshot = false;
    bool m_pendingRePause = false; // Set by StepFrame(); cleared by Update() after re-pausing.
};

} // namespace KumariEngine::Editor
