#pragma once

namespace Kumari {

enum class EngineState {
    Init,
    Loading,
    Running,
    Paused,
    Debug,
    Shutdown
};

class EngineStateManager {
public:
    static EngineStateManager& Get();

    void SetState(EngineState newState);
    EngineState GetState() const;

    bool IsRunning() const;
    bool IsPaused() const;
    bool IsDebug() const;

    void TogglePause();
    void ToggleDebug();

private:
    EngineState currentState = EngineState::Init;
};

}
