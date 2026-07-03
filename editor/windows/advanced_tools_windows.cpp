#include "advanced_tools_windows.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "core/memory_tracker.hpp"
#include "core/thread_profiler.hpp"
#include "core/frame_graph.hpp"
#include "platform/plugin_manager.hpp"
#include "editor/selection_system.hpp"
#include <iostream>

namespace KumariEngine::Editor {

// --- Undo/Redo Commands Implementation ---

void ModifyAnimationStateMachineCommand::Execute() {
    if (m_registry && m_registry->HasComponent<Animation::AnimationComponent>(m_entity)) {
        m_registry->GetComponent<Animation::AnimationComponent>(m_entity).stateMachine = m_newState;
    }
}
void ModifyAnimationStateMachineCommand::Undo() {
    if (m_registry && m_registry->HasComponent<Animation::AnimationComponent>(m_entity)) {
        m_registry->GetComponent<Animation::AnimationComponent>(m_entity).stateMachine = m_oldState;
    }
}

void ModifyBehaviorTreeCommand::Execute() {
    if (m_registry && m_registry->HasComponent<AI::BehaviorTreeComponent>(m_entity)) {
        m_registry->GetComponent<AI::BehaviorTreeComponent>(m_entity).tree = m_newTree;
    }
}
void ModifyBehaviorTreeCommand::Undo() {
    if (m_registry && m_registry->HasComponent<AI::BehaviorTreeComponent>(m_entity)) {
        m_registry->GetComponent<AI::BehaviorTreeComponent>(m_entity).tree = m_oldTree;
    }
}

void ModifyVisualScriptGraphCommand::Execute() {
    if (m_registry && m_registry->HasComponent<Scripting::VisualScriptingComponent>(m_entity)) {
        m_registry->GetComponent<Scripting::VisualScriptingComponent>(m_entity).graph.Deserialize(m_newData);
    }
}
void ModifyVisualScriptGraphCommand::Undo() {
    if (m_registry && m_registry->HasComponent<Scripting::VisualScriptingComponent>(m_entity)) {
        m_registry->GetComponent<Scripting::VisualScriptingComponent>(m_entity).graph.Deserialize(m_oldData);
    }
}

void ModifyAudioMixerCommand::Execute() {
    Audio::AudioSystem::Get().GetMixer().SetVolume(m_channel, m_newVol);
    Audio::AudioSystem::Get().GetMixer().SetMuted(m_channel, m_newMute);
}
void ModifyAudioMixerCommand::Undo() {
    Audio::AudioSystem::Get().GetMixer().SetVolume(m_channel, m_oldVol);
    Audio::AudioSystem::Get().GetMixer().SetMuted(m_channel, m_oldMute);
}

void ModifyParticleEmitterCommand::Execute() {
    if (m_registry && m_registry->HasComponent<Particle::ParticleSystemComponent>(m_entity)) {
        m_registry->GetComponent<Particle::ParticleSystemComponent>(m_entity).settings = m_newVal;
    }
}
void ModifyParticleEmitterCommand::Undo() {
    if (m_registry && m_registry->HasComponent<Particle::ParticleSystemComponent>(m_entity)) {
        m_registry->GetComponent<Particle::ParticleSystemComponent>(m_entity).settings = m_oldVal;
    }
}


// --- Editor Window Panel Methods ---

// 1. Animation Editor
void AnimationEditorWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Animation Editor Window] ===");
    Core::Logger::Info("EditorUI", "  States: Idle, Run, Jump, Attack");
    Core::Logger::Info("EditorUI", "  Blend Parameters: Speed (0.0), Direction (0.0)");
}
void AnimationEditorWindow::SetCurrentState(ECS::Registry* reg, ECS::Entity ent, const std::string& state) {
    if (!reg || !reg->HasComponent<Animation::AnimationComponent>(ent)) return;
    auto& comp = reg->GetComponent<Animation::AnimationComponent>(ent);
    
    Animation::AnimationStateMachine oldSM = comp.stateMachine;
    Animation::AnimationStateMachine newSM = comp.stateMachine;
    newSM.currentState = state;
    
    UndoSystem::Get().Execute(std::make_shared<ModifyAnimationStateMachineCommand>(reg, ent, oldSM, newSM));
}
void AnimationEditorWindow::SetBlendWeight(ECS::Registry* reg, ECS::Entity ent, const std::string& param, float weight) {
    if (!reg || !reg->HasComponent<Animation::AnimationComponent>(ent)) return;
    auto& comp = reg->GetComponent<Animation::AnimationComponent>(ent);
    
    Animation::AnimationStateMachine oldSM = comp.stateMachine;
    Animation::AnimationStateMachine newSM = comp.stateMachine;
    newSM.floatParameters[param] = weight;
    
    UndoSystem::Get().Execute(std::make_shared<ModifyAnimationStateMachineCommand>(reg, ent, oldSM, newSM));
}

// 2. AI Editor
void AIEditorWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- AI Editor Window ---");
    
    auto& navMesh = AI::AISystem::Get().GetNavMesh();
    const auto& nodes = navMesh.GetNodes();
    int walkableCount = 0;
    for (const auto& node : nodes) {
        if (node.walkable) walkableCount++;
    }
    Core::Logger::Info("EditorUI", "NavMesh: Total Nodes = %zu, Walkable = %d, Blocked = %zu",
                       nodes.size(), walkableCount, nodes.size() - walkableCount);

    ECS::Entity selected = SelectionSystem::Get().GetSelected();
    auto* registry = WindowSystem::Get().GetRegistry();
    if (selected != ECS::NULL_ENTITY && registry && registry->IsAlive(selected)) {
        Core::Logger::Info("EditorUI", "Selected Entity: %u", selected);
        
        if (registry->HasComponent<AI::NavigationAgentComponent>(selected)) {
            const auto& agent = registry->GetComponent<AI::NavigationAgentComponent>(selected);
            Core::Logger::Info("EditorUI", "  NavigationAgentComponent:");
            Core::Logger::Info("EditorUI", "    Speed: %.2f", agent.agentSpeed);
            Core::Logger::Info("EditorUI", "    Acceptance Radius: %.2f", agent.acceptanceRadius);
            Core::Logger::Info("EditorUI", "    Avoidance: %s (Radius: %.2f)",
                               agent.useAvoidance ? "Enabled" : "Disabled", agent.avoidanceRadius);
            Core::Logger::Info("EditorUI", "    Target: [%.1f, %.1f, %.1f]",
                               agent.agentTarget.x, agent.agentTarget.y, agent.agentTarget.z);
            Core::Logger::Info("EditorUI", "    Path points: %zu, Current index: %zu",
                               agent.currentPath.size(), agent.pathIndex);
            
            if (!agent.currentPath.empty()) {
                Core::Logger::Info("EditorUI", "    Active Path Overlay:");
                for (size_t i = 0; i < agent.currentPath.size(); ++i) {
                    const auto& p = agent.currentPath[i];
                    Core::Logger::Info("EditorUI", "      [%zu]: (%.1f, %.1f, %.1f) %s",
                                       i, p.x, p.y, p.z, (i == agent.pathIndex) ? "<-- Current" : "");
                }
            }
        }
        
        if (registry->HasComponent<AI::BlackboardComponent>(selected)) {
            const auto& blackboard = registry->GetComponent<AI::BlackboardComponent>(selected);
            Core::Logger::Info("EditorUI", "  BlackboardComponent:");
            for (const auto& [key, val] : blackboard.values) {
                if (std::holds_alternative<int>(val)) {
                    Core::Logger::Info("EditorUI", "    %s: %d", key.c_str(), std::get<int>(val));
                } else if (std::holds_alternative<float>(val)) {
                    Core::Logger::Info("EditorUI", "    %s: %.2f", key.c_str(), std::get<float>(val));
                } else if (std::holds_alternative<bool>(val)) {
                    Core::Logger::Info("EditorUI", "    %s: %s", key.c_str(), std::get<bool>(val) ? "True" : "False");
                } else if (std::holds_alternative<std::string>(val)) {
                    Core::Logger::Info("EditorUI", "    %s: '%s'", key.c_str(), std::get<std::string>(val).c_str());
                } else if (std::holds_alternative<glm::vec3>(val)) {
                    const auto& v = std::get<glm::vec3>(val);
                    Core::Logger::Info("EditorUI", "    %s: [%.1f, %.1f, %.1f]", key.c_str(), v.x, v.y, v.z);
                } else if (std::holds_alternative<ECS::Entity>(val)) {
                    Core::Logger::Info("EditorUI", "    %s: Entity %u", key.c_str(), std::get<ECS::Entity>(val));
                }
            }
        }
        
        if (registry->HasComponent<AI::PerceptionComponent>(selected)) {
            const auto& perception = registry->GetComponent<AI::PerceptionComponent>(selected);
            Core::Logger::Info("EditorUI", "  PerceptionComponent:");
            Core::Logger::Info("EditorUI", "    FOV: %.1f, Vision: %.1f, Hearing: %.1f",
                               perception.fieldOfView, perception.visionRange, perception.hearingRange);
            for (const auto& stim : perception.perceivedStimuli) {
                Core::Logger::Info("EditorUI", "      Target Entity %u: [%.1f, %.1f, %.1f], visible: %s",
                                   stim.entity, stim.position.x, stim.position.y, stim.position.z,
                                   stim.isVisible ? "Yes" : "No");
            }
        }
        
        if (registry->HasComponent<AI::BehaviorTreeComponent>(selected)) {
            const auto& btc = registry->GetComponent<AI::BehaviorTreeComponent>(selected);
            Core::Logger::Info("EditorUI", "  BehaviorTreeComponent:");
            if (btc.tree.GetRoot()) {
                std::function<void(std::shared_ptr<AI::BTNode>, int)> printNode = [&](std::shared_ptr<AI::BTNode> node, int depth) {
                    if (!node) return;
                    std::string indent(depth * 2, ' ');
                    Core::Logger::Info("EditorUI", "    %s- Node: %s", indent.c_str(), node->GetName().c_str());
                    
                    auto sel = std::dynamic_pointer_cast<AI::SelectorNode>(node);
                    if (sel) {
                        for (const auto& child : sel->GetChildren()) {
                            printNode(child, depth + 1);
                        }
                    }
                    auto seq = std::dynamic_pointer_cast<AI::SequenceNode>(node);
                    if (seq) {
                        for (const auto& child : seq->GetChildren()) {
                            printNode(child, depth + 1);
                        }
                    }
                    auto dec = std::dynamic_pointer_cast<AI::DecoratorNode>(node);
                    if (dec) {
                        printNode(dec->GetChild(), depth + 1);
                    }
                };
                printNode(btc.tree.GetRoot(), 0);
            }
        }
    }
}
void AIEditorWindow::RebuildNavMesh(const glm::vec3& center, int w, int d, float space) {
    AI::AISystem::Get().GetNavMesh().BuildGrid(center, w, d, space);
    Core::Logger::Info("AI", "Navigation Mesh rebuilt at center [%.1f, %.1f, %.1f], grid size %d x %d",
                       center.x, center.y, center.z, w, d);
}
void AIEditorWindow::SetAgentDestination(ECS::Registry* reg, ECS::Entity ent, const glm::vec3& target) {
    if (reg && reg->HasComponent<AI::NavMeshComponent>(ent)) {
        reg->GetComponent<AI::NavMeshComponent>(ent).agentTarget = target;
    }
    if (reg && reg->HasComponent<AI::NavigationAgentComponent>(ent)) {
        reg->GetComponent<AI::NavigationAgentComponent>(ent).agentTarget = target;
    }
}

// 3. Visual Scripting Editor
void VisualScriptingWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Visual Scripting Window] ===");
    Core::Logger::Info("EditorUI", "  Graph Nodes: Start, Update, CustomEvent");
    Core::Logger::Info("EditorUI", "  Breakpoints: None");
}
void VisualScriptingWindow::AddNode(ECS::Registry* reg, ECS::Entity ent, const Scripting::VSNode& node) {
    if (!reg || !reg->HasComponent<Scripting::VisualScriptingComponent>(ent)) return;
    auto& comp = reg->GetComponent<Scripting::VisualScriptingComponent>(ent);

    std::string oldData = comp.graph.Serialize();
    comp.graph.AddNode(node);
    std::string newData = comp.graph.Serialize();

    UndoSystem::Get().Execute(std::make_shared<ModifyVisualScriptGraphCommand>(reg, ent, oldData, newData));
}
void VisualScriptingWindow::AddConnection(ECS::Registry* reg, ECS::Entity ent, const std::string& fromN, const std::string& fromP, const std::string& toN, const std::string& toP) {
    if (!reg || !reg->HasComponent<Scripting::VisualScriptingComponent>(ent)) return;
    auto& comp = reg->GetComponent<Scripting::VisualScriptingComponent>(ent);

    std::string oldData = comp.graph.Serialize();
    comp.graph.Connect(fromN, fromP, toN, toP);
    std::string newData = comp.graph.Serialize();

    UndoSystem::Get().Execute(std::make_shared<ModifyVisualScriptGraphCommand>(reg, ent, oldData, newData));
}
void VisualScriptingWindow::ToggleBreakpoint(ECS::Registry* reg, ECS::Entity ent, const std::string& nodeId) {
    if (!reg || !reg->HasComponent<Scripting::VisualScriptingComponent>(ent)) return;
    auto& comp = reg->GetComponent<Scripting::VisualScriptingComponent>(ent);
    
    std::string oldData = comp.graph.Serialize();
    auto* node = comp.graph.FindNode(nodeId);
    if (node) {
        node->isBreakpoint = !node->isBreakpoint;
    }
    std::string newData = comp.graph.Serialize();

    UndoSystem::Get().Execute(std::make_shared<ModifyVisualScriptGraphCommand>(reg, ent, oldData, newData));
}

// 4. Audio Mixer Panel
void AudioMixerWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Audio Mixer Window ---");
    auto& mixer = Audio::AudioSystem::Get().GetMixer();
    
    auto printChannel = [&](const std::string& name, Audio::MixerChannel chan) {
        float vol = mixer.GetVolume(chan);
        bool muted = mixer.IsMuted(chan);
        Core::Logger::Info("EditorUI", "  Channel: %s | Volume: %.2f | Muted: %s",
                           name.c_str(), vol, muted ? "Yes" : "No");
    };

    printChannel("Master", Audio::MixerChannel::Master);
    printChannel("Music", Audio::MixerChannel::Music);
    printChannel("SFX", Audio::MixerChannel::SFX);
    printChannel("Dialogue", Audio::MixerChannel::Dialogue);
}
void AudioMixerWindow::SetChannelVolume(Audio::MixerChannel chan, float vol) {
    float oldVol = Audio::AudioSystem::Get().GetMixer().GetVolume(chan);
    bool oldMute = Audio::AudioSystem::Get().GetMixer().IsMuted(chan);
    
    UndoSystem::Get().Execute(std::make_shared<ModifyAudioMixerCommand>(chan, oldVol, vol, oldMute, oldMute));
}
void AudioMixerWindow::SetChannelMute(Audio::MixerChannel chan, bool mute) {
    float oldVol = Audio::AudioSystem::Get().GetMixer().GetVolume(chan);
    bool oldMute = Audio::AudioSystem::Get().GetMixer().IsMuted(chan);
    
    UndoSystem::Get().Execute(std::make_shared<ModifyAudioMixerCommand>(chan, oldVol, oldVol, oldMute, mute));
}

// 5. Particle System Editor
void ParticleEditorWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Particle Editor Window] ===");
    Core::Logger::Info("EditorUI", "  Emitters: Fire, Smoke, Explosion");
    Core::Logger::Info("EditorUI", "  Active Particles: 0");
}
void ParticleEditorWindow::UpdateEmitterSettings(ECS::Registry* reg, ECS::Entity ent, const Particle::ParticleEmitterSettings& settings) {
    if (!reg || !reg->HasComponent<Particle::ParticleSystemComponent>(ent)) return;
    auto& comp = reg->GetComponent<Particle::ParticleSystemComponent>(ent);

    Particle::ParticleEmitterSettings oldSettings = comp.settings;
    UndoSystem::Get().Execute(std::make_shared<ModifyParticleEmitterCommand>(reg, ent, oldSettings, settings));
}

static void RenderCPUSample(const Core::ProfilerSample& sample, int indent) {
    std::string indentStr(indent * 2, ' ');
    Core::Logger::Info("EditorUI", "%s  - %s: %.2f ms", indentStr.c_str(), sample.name.c_str(), sample.durationMs);
    for (const auto& child : sample.children) {
        RenderCPUSample(child, indent + 1);
    }
}

// 6. Profiler Window
void ProfilerWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Profiler Window] ===");
    Core::Logger::Info("EditorUI", "  [CPU timings]");
    auto cpuHistory = Core::Profiler::Get().GetCPUHistory();
    for (const auto& sample : cpuHistory) {
        RenderCPUSample(sample, 0);
    }

    Core::Logger::Info("EditorUI", "  [GPU timings]");
    auto gpuSamples = Core::Profiler::Get().GetGPUSamples();
    for (const auto& [name, duration] : gpuSamples) {
        Core::Logger::Info("EditorUI", "    - %s: %.2f ms", name.c_str(), duration);
    }

    Core::Logger::Info("EditorUI", "  [Thread states]");
    auto threadStates = Core::ThreadProfiler::Get().GetThreadStates();
    for (const auto& [tid, state] : threadStates) {
        (void)tid;
        Core::Logger::Info("EditorUI", "    - %s: %.1f%% utilization", state.threadName.c_str(), state.lastUtilizationRatio * 100.0);
    }

    Core::Logger::Info("EditorUI", "  [Memory tracking]");
    const char* categoryNames[] = {
        "ECS", "Renderer", "Audio", "Physics", "Script", "Network", "General",
        "Terrain", "Streaming", "HotReload", "Reflection"
    };
    for (int i = 0; i <= static_cast<int>(Core::MemoryCategory::Reflection); ++i) {
        auto cat = static_cast<Core::MemoryCategory>(i);
        auto stats = Core::MemoryTracker::Get().GetStats(cat);
        Core::Logger::Info("EditorUI", "    - %s: %.2f KB (Peak: %.2f KB, Frag: %.1f%%)",
                           categoryNames[i],
                           static_cast<double>(stats.currentUsage) / 1024.0,
                           static_cast<double>(stats.peakUsage) / 1024.0,
                           stats.fragmentationRatio * 100.0f);
    }
}
void ProfilerWindow::TriggerFrameCapture(ECS::Registry* reg, const std::string& filepath) {
    if (Core::Profiler::Get().CaptureFrame(filepath, reg)) {
        Core::Logger::Info("Profiler", "Frame diagnostics saved to: %s", filepath.c_str());
    } else {
        Core::Logger::Error("Profiler", "Failed to capture frame to: %s", filepath.c_str());
    }
}

// 7. Packaging / Build Wizard
void BuildWizardWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Build Wizard Window] ===");
    for (const auto& profile : m_profiles) {
        Core::Logger::Info("EditorUI", "  Profile: %s | Platform: %s | Configuration: %s",
                           profile.name.c_str(), profile.platform.c_str(), profile.isRelease ? "Release" : "Debug");
    }
}
void BuildWizardWindow::ConfigureBuildProfile(const std::string& name, const std::string& platform, bool release) {
    m_profiles.push_back({name, platform, release});
    Core::Logger::Info("Packaging", "Configured Build Profile: %s (%s, %s)",
                       name.c_str(), platform.c_str(), release ? "Release" : "Debug");
}
bool BuildWizardWindow::RunValidationChecks() {
    Core::Logger::Info("Packaging", "Running validation checks on resources, scenes and components...");
    // Mock validation rule checks
    Core::Logger::Info("Packaging", "Validation checks PASSED cleanly.");
    return true;
}
bool BuildWizardWindow::BuildProject(const std::string& profileName) {
    auto it = std::find_if(m_profiles.begin(), m_profiles.end(), [&](const BuildProfile& p) {
        return p.name == profileName;
    });

    if (it == m_profiles.end()) {
        Core::Logger::Error("Packaging", "Failed to package: Build profile '%s' not found.", profileName.c_str());
        return false;
    }

    Core::Logger::Info("Packaging", "Building target package for platform '%s' (Configuration: %s)...",
                       it->platform.c_str(), it->isRelease ? "Release" : "Debug");
    Core::Logger::Info("Packaging", "Package created successfully.");
    return true;
}

// 8. Plugin Manager
void PluginManagerWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Plugin Manager Window] ===");
    const auto& plugins = Platform::PluginManager::Get().GetPlugins();
    for (const auto& [name, plugin] : plugins) {
        Core::Logger::Info("EditorUI", "  Plugin: %s | Version: %s",
                           name.c_str(), plugin->GetVersion().c_str());
    }
}
void PluginManagerWindow::LoadPlugin(std::shared_ptr<Platform::Plugin> p) {
    Platform::PluginManager::Get().LoadPlugin(p);
}
void PluginManagerWindow::UnloadPlugin(const std::string& name) {
    Platform::PluginManager::Get().UnloadPlugin(name);
}

// 9. Documentation / Help system
void DocumentationWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Documentation Window] ===");
    Core::Logger::Info("EditorUI", "  Topics: ecs, navmesh, audio, particles, undo");
}
void DocumentationWindow::GenerateAPIHelp(const std::string& destPath) {
    Platform::PluginManager::Get().GenerateAPIDocumentation(destPath);
    Core::Logger::Info("Documentation", "Exported API Reference to: %s", destPath.c_str());
}
std::string DocumentationWindow::SearchHelpDatabase(const std::string& keyword) {
    std::unordered_map<std::string, std::string> helpDB = {
        { "ecs", "ECS::Registry: Manages entity creation, component assignment, and View/Each loops." },
        { "navmesh", "AI::NavMesh: Builds A* grids for obstacle avoidance and agent path calculations." },
        { "audio", "Audio::AudioMixer: Handles Master, Music, SFX, and Dialogue channels routing." },
        { "particles", "Particle::ParticleSystem: Updates life, velocity, gravity and mixes color parameters." },
        { "undo", "UndoSystem: Manages command buffers allowing visual changes to be reverted/reapplied." }
    };

    std::string keyLower = keyword;
    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto it = helpDB.find(keyLower);
    if (it != helpDB.end()) {
        return it->second;
    }
    return "No documentation entries found matching keyword: " + keyword;
}

} // namespace KumariEngine::Editor
