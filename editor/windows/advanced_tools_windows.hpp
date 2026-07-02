#pragma once
#include "editor/window_system.hpp"
#include "editor/undo_redo.hpp"
#include "animation/animation_system.hpp"
#include "ai/ai_system.hpp"
#include "scripting/visual_scripting.hpp"
#include "audio/audio_system.hpp"
#include "particle/particle_system.hpp"
#include "platform/plugin_manager.hpp"
#include <string>
#include <vector>

namespace KumariEngine::Editor {

// --- Undo/Redo Commands for Advanced Tools ---

class ModifyAnimationStateMachineCommand : public Command {
public:
    ModifyAnimationStateMachineCommand(ECS::Registry* reg, ECS::Entity entity, const Animation::AnimationStateMachine& oldState, const Animation::AnimationStateMachine& newState)
        : m_registry(reg), m_entity(entity), m_oldState(oldState), m_newState(newState) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Animation State Machine"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Animation::AnimationStateMachine m_oldState, m_newState;
};

class ModifyBehaviorTreeCommand : public Command {
public:
    ModifyBehaviorTreeCommand(ECS::Registry* reg, ECS::Entity entity, const AI::BehaviorTree& oldTree, const AI::BehaviorTree& newTree)
        : m_registry(reg), m_entity(entity), m_oldTree(oldTree), m_newTree(newTree) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Behavior Tree"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    AI::BehaviorTree m_oldTree, m_newTree;
};

class ModifyVisualScriptGraphCommand : public Command {
public:
    ModifyVisualScriptGraphCommand(ECS::Registry* reg, ECS::Entity entity, const std::string& oldData, const std::string& newData)
        : m_registry(reg), m_entity(entity), m_oldData(oldData), m_newData(newData) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Visual Script Graph"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    std::string m_oldData, m_newData;
};

class ModifyAudioMixerCommand : public Command {
public:
    ModifyAudioMixerCommand(Audio::MixerChannel channel, float oldVol, float newVol, bool oldMute, bool newMute)
        : m_channel(channel), m_oldVol(oldVol), m_newVol(newVol), m_oldMute(oldMute), m_newMute(newMute) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Audio Mixer settings"; }
private:
    Audio::MixerChannel m_channel;
    float m_oldVol, m_newVol;
    bool m_oldMute, m_newMute;
};

class ModifyParticleEmitterCommand : public Command {
public:
    ModifyParticleEmitterCommand(ECS::Registry* reg, ECS::Entity entity, const Particle::ParticleEmitterSettings& oldVal, const Particle::ParticleEmitterSettings& newVal)
        : m_registry(reg), m_entity(entity), m_oldVal(oldVal), m_newVal(newVal) {}
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override { return "Modify Particle Emitter Settings"; }
private:
    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    Particle::ParticleEmitterSettings m_oldVal, m_newVal;
};


// --- Advanced Editor Panel Windows ---

class AnimationEditorWindow : public EditorWindow {
public:
    AnimationEditorWindow() : EditorWindow("Animation Editor") {}
    void RenderUI() override;
    
    void SetCurrentState(ECS::Registry* reg, ECS::Entity ent, const std::string& state);
    void SetBlendWeight(ECS::Registry* reg, ECS::Entity ent, const std::string& param, float weight);
};

class AIEditorWindow : public EditorWindow {
public:
    AIEditorWindow() : EditorWindow("AI Editor") {}
    void RenderUI() override;
    
    void RebuildNavMesh(const glm::vec3& center, int w, int d, float space);
    void SetAgentDestination(ECS::Registry* reg, ECS::Entity ent, const glm::vec3& target);
};

class VisualScriptingWindow : public EditorWindow {
public:
    VisualScriptingWindow() : EditorWindow("Visual Scripting") {}
    void RenderUI() override;
    
    void AddNode(ECS::Registry* reg, ECS::Entity ent, const Scripting::VSNode& node);
    void AddConnection(ECS::Registry* reg, ECS::Entity ent, const std::string& fromN, const std::string& fromP, const std::string& toN, const std::string& toP);
    void ToggleBreakpoint(ECS::Registry* reg, ECS::Entity ent, const std::string& nodeId);
};

class AudioMixerWindow : public EditorWindow {
public:
    AudioMixerWindow() : EditorWindow("Audio Mixer") {}
    void RenderUI() override;
    
    void SetChannelVolume(Audio::MixerChannel chan, float vol);
    void SetChannelMute(Audio::MixerChannel chan, bool mute);
};

class ParticleEditorWindow : public EditorWindow {
public:
    ParticleEditorWindow() : EditorWindow("Particle Editor") {}
    void RenderUI() override;
    
    void UpdateEmitterSettings(ECS::Registry* reg, ECS::Entity ent, const Particle::ParticleEmitterSettings& settings);
};

class ProfilerWindow : public EditorWindow {
public:
    ProfilerWindow() : EditorWindow("Profiler") {}
    void RenderUI() override;
    
    void TriggerFrameCapture(ECS::Registry* reg, const std::string& filepath);
};

struct BuildProfile {
    std::string name;
    std::string platform;
    bool isRelease = false;
};

class BuildWizardWindow : public EditorWindow {
public:
    BuildWizardWindow() : EditorWindow("Build Wizard") {}
    void RenderUI() override;
    
    void ConfigureBuildProfile(const std::string& name, const std::string& platform, bool release);
    bool RunValidationChecks();
    bool BuildProject(const std::string& profileName);
    
    const std::vector<BuildProfile>& GetProfiles() const { return m_profiles; }

private:
    std::vector<BuildProfile> m_profiles;
};

class PluginManagerWindow : public EditorWindow {
public:
    PluginManagerWindow() : EditorWindow("Plugin Manager") {}
    void RenderUI() override;
    
    void LoadPlugin(std::shared_ptr<Platform::Plugin> p);
    void UnloadPlugin(const std::string& name);
};

class DocumentationWindow : public EditorWindow {
public:
    DocumentationWindow() : EditorWindow("Help & Documentation") {}
    void RenderUI() override;
    
    void GenerateAPIHelp(const std::string& destPath);
    std::string SearchHelpDatabase(const std::string& keyword);
};

} // namespace KumariEngine::Editor
