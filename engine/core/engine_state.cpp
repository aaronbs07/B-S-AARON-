#include "engine_state.hpp"

namespace Kumari {

EngineStateManager& EngineStateManager::Get() {
    static EngineStateManager instance;
    return instance;
}

void EngineStateManager::SetState(EngineState newState) {
    currentState = newState;
}

EngineState EngineStateManager::GetState() const {
    return currentState;
}

bool EngineStateManager::IsRunning() const {
    return currentState == EngineState::Running;
}

bool EngineStateManager::IsPaused() const {
    return currentState == EngineState::Paused;
}

bool EngineStateManager::IsDebug() const {
    return currentState == EngineState::Debug;
}

void EngineStateManager::TogglePause() {
    if (currentState == EngineState::Paused)
        currentState = EngineState::Running;
    else if (currentState == EngineState::Running)
        currentState = EngineState::Paused;
}

void EngineStateManager::ToggleDebug() {
    if (currentState == EngineState::Debug)
        currentState = EngineState::Running;
    else if (currentState == EngineState::Running)
        currentState = EngineState::Debug;
}

}
