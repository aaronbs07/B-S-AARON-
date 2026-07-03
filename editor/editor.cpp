#include "editor.hpp"
#include "window_system.hpp"
#include "selection_system.hpp"
#include "editor_manager.hpp"
#include "docking_system.hpp"
#include "windows/scene_view_window.hpp"
#include "windows/game_view_window.hpp"
#include "windows/hierarchy_window.hpp"
#include "windows/inspector_window.hpp"
#include "windows/console_window.hpp"
#include "windows/asset_browser_window.hpp"
#include "windows/statistics_window.hpp"
#include "windows/terrain_panels.hpp"
#include "windows/world_streaming_window.hpp"
#include "windows/advanced_tools_windows.hpp"
#include "windows/camera_preview_window.hpp"
#include "windows/timeline_editor_window.hpp"
#include "windows/cinematic_preview_window.hpp"
#include "windows/gameplay_inspector_window.hpp"
#include "windows/reflection_viewer.hpp"
#include "windows/toolbar_window.hpp"
#include "windows/settings_window.hpp"
#include "windows/project_window.hpp"
#include "camera/camera_component.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "animation/animation_system.hpp"
#include "ai/ai_system.hpp"
#include "scripting/visual_scripting.hpp"
#include "audio/audio_system.hpp"
#include "particle/particle_system.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "gameplay/GameInstance.hpp"
#include "physics/debug_renderer.hpp"
#include "scene/transform_component.hpp"
#include "core/logger.hpp"
#include "save/SaveManager.hpp"
#include "scene/scene_manager.hpp"
#include "undo_redo.hpp"

namespace KumariEngine::Editor {

Editor::Editor() = default;

Editor::~Editor() {
    Shutdown();
}

bool Editor::Initialize(ECS::Registry* registry) {
    if (m_initialized) return true;

    Core::Logger::Info("Editor", "Initializing Kumari Editor...");
    m_registry = registry;

    // Set GameInstance editor mode to true
    Gameplay::GameInstance::Get().SetEditorMode(true);

    // Reset undo system history and dirty flag on editor initialization
    UndoSystem::Get().Clear();

    // Register editor components in ECS registry
    if (m_registry) {
        m_registry->RegisterComponent<Camera::CameraComponent>();
        m_registry->RegisterComponent<Lighting::LightComponent>();
        m_registry->RegisterComponent<Renderer::MeshRendererComponent>();
        m_registry->RegisterComponent<Animation::AnimationComponent>();
        m_registry->RegisterComponent<AI::NavMeshComponent>();
        m_registry->RegisterComponent<AI::BehaviorTreeComponent>();
        m_registry->RegisterComponent<AI::BlackboardComponent>();
        m_registry->RegisterComponent<AI::PerceptionComponent>();
        m_registry->RegisterComponent<AI::NavigationAgentComponent>();
        m_registry->RegisterComponent<AI::AIComponent>();
        m_registry->RegisterComponent<Scripting::VisualScriptingComponent>();
        m_registry->RegisterComponent<Audio::AudioSourceComponent>();
        m_registry->RegisterComponent<Audio::AudioListenerComponent>();
        m_registry->RegisterComponent<Particle::ParticleSystemComponent>();
        m_registry->RegisterComponent<Gameplay::HealthComponent>();
        m_registry->RegisterComponent<Gameplay::DamageComponent>();
        m_registry->RegisterComponent<Gameplay::TeamComponent>();
        m_registry->RegisterComponent<Gameplay::InteractionComponent>();
        m_registry->RegisterComponent<Gameplay::GameplayTagsComponent>();
        m_registry->RegisterComponent<Gameplay::SpawnPointComponent>();
        m_registry->RegisterComponent<Gameplay::PlayerControllerComponent>();
        m_registry->RegisterComponent<Gameplay::PlayerStateComponent>();
        m_registry->RegisterComponent<Gameplay::GameStateComponent>();
    }

    // Initialize EditorManager
    EditorManager::Get().Initialize(m_registry);

    // Load configuration
    m_config.Load("editor_config.ini");
    DockingSystem::Get().LoadLayout("editor_config.ini");

    // Instantiating and adding editor windows
    auto& ws = WindowSystem::Get();
    ws.AddWindow(std::make_shared<SceneViewWindow>());
    ws.AddWindow(std::make_shared<GameViewWindow>());
    ws.AddWindow(std::make_shared<SceneHierarchyWindow>());
    ws.AddWindow(std::make_shared<InspectorWindow>());
    ws.AddWindow(std::make_shared<ConsoleWindow>());
    ws.AddWindow(std::make_shared<AssetBrowserWindow>());
    ws.AddWindow(std::make_shared<StatisticsWindow>());
    ws.AddWindow(std::make_shared<TerrainInspectorWindow>());
    ws.AddWindow(std::make_shared<TerrainBrushPanelWindow>());
    ws.AddWindow(std::make_shared<WorldSettingsPanelWindow>());
    ws.AddWindow(std::make_shared<EnvironmentPanelWindow>());
    ws.AddWindow(std::make_shared<WorldStreamingWindow>());
    ws.AddWindow(std::make_shared<GameplayInspectorWindow>());
    
    // Phase 5 Windows
    ws.AddWindow(std::make_shared<AnimationEditorWindow>());
    ws.AddWindow(std::make_shared<AIEditorWindow>());
    ws.AddWindow(std::make_shared<VisualScriptingWindow>());
    ws.AddWindow(std::make_shared<AudioMixerWindow>());
    ws.AddWindow(std::make_shared<ParticleEditorWindow>());
    ws.AddWindow(std::make_shared<ProfilerWindow>());
    ws.AddWindow(std::make_shared<BuildWizardWindow>());
    ws.AddWindow(std::make_shared<PluginManagerWindow>());
    ws.AddWindow(std::make_shared<DocumentationWindow>());
    ws.AddWindow(std::make_shared<CameraPreviewWindow>());
    ws.AddWindow(std::make_shared<TimelineEditorWindow>());
    ws.AddWindow(std::make_shared<CinematicPreviewWindow>());
    ws.AddWindow(std::make_shared<ToolbarWindow>());
    ws.AddWindow(std::make_shared<SettingsWindow>());
    ws.AddWindow(std::make_shared<ProjectWindow>());

    // Milestone 10 Phase 4: Reflection Viewer
    ws.AddWindow(std::make_shared<ReflectionViewerWindow>());

    ws.Initialize(m_registry);

    m_initialized = true;
    Core::Logger::Info("Editor", "Kumari Editor initialized successfully.");
    return true;
}

void Editor::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update window system
    WindowSystem::Get().Update(deltaTime);

    // Apply selection visual highlighting via broad-phase lines submission
    SelectionSystem::Get().HighlightSelection(m_registry);

    // Draw spawn points debug visualizers
    if (m_registry) {
        m_registry->Each<Gameplay::SpawnPointComponent, Scene::TransformComponent>([&](auto, const Gameplay::SpawnPointComponent& sp, const Scene::TransformComponent& tc) {
            if (sp.isEnabled) {
                // Draw a blue sphere representing spawn location
                Physics::PhysicsDebugRenderer::Get().DrawSphere(tc.position, 1.0f, glm::vec3(0.0f, 0.5f, 1.0f));
                // Draw a forward line showing rotation direction
                glm::vec3 forward = tc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                Physics::PhysicsDebugRenderer::Get().DrawLine(tc.position, tc.position + forward * 1.5f, glm::vec3(0.0f, 0.8f, 1.0f));
            }
        });
    }
}

void Editor::Render() {
    if (!m_initialized) return;

    // Dispatch window UI draw calls
    WindowSystem::Get().RenderUI();
}

void Editor::Shutdown() {
    if (!m_initialized) return;

    Core::Logger::Info("Editor", "Shutting down Kumari Editor...");

    // Warn about unsaved changes but do not block shutdown —
    // RequestClose() is for interactive cancel paths (OpenScene, NewScene).
    // Shutdown is always unconditional; the caller decides whether to prompt
    // the user before reaching this point.
    if (IsDirty()) {
        Core::Logger::Warning("Editor", "[Warning] Shutting down with unsaved scene changes!");
    }

    // Save configuration
    m_config.Save("editor_config.ini");
    DockingSystem::Get().SaveLayout("editor_config.ini");

    // Shutdown EditorManager
    EditorManager::Get().Shutdown();

    // Shutdown window system
    WindowSystem::Get().Shutdown();

    m_registry = nullptr;
    m_initialized = false;
    Core::Logger::Info("Editor", "Kumari Editor shut down cleanly.");
}

bool Editor::SaveScene(const std::string& filepath) {
    if (!m_initialized || !m_registry) return false;
    Core::Logger::Info("Editor", "Saving Scene to '%s'...", filepath.c_str());
    if (Save::SaveManager::Get().SaveGame(filepath, m_registry)) {
        m_currentScenePath = filepath;
        UndoSystem::Get().SetDirty(false);
        m_isDirty = false;
        Core::Logger::Info("Editor", "Scene saved successfully.");
        return true;
    }
    Core::Logger::Error("Editor", "Failed to save scene.");
    return false;
}

bool Editor::SaveScene() {
    if (m_currentScenePath.empty()) {
        Core::Logger::Warning("Editor", "Cannot save scene: no scene file path set.");
        return false;
    }
    return SaveScene(m_currentScenePath);
}

bool Editor::SaveAs(const std::string& filepath) {
    return SaveScene(filepath);
}

bool Editor::OpenScene(const std::string& filepath) {
    if (!m_initialized || !m_registry) return false;

    if (!RequestClose()) {
        Core::Logger::Warning("Editor", "Open Scene cancelled due to unsaved changes.");
        return false;
    }

    Core::Logger::Info("Editor", "Opening Scene from '%s'...", filepath.c_str());
    if (Save::SaveManager::Get().LoadGame(filepath, m_registry)) {
        m_currentScenePath = filepath;
        UndoSystem::Get().Clear();
        m_isDirty = false;
        Core::Logger::Info("Editor", "Scene loaded successfully.");
        return true;
    }
    Core::Logger::Error("Editor", "Failed to load scene.");
    return false;
}

void Editor::NewScene() {
    if (!m_initialized || !m_registry) return;
    if (!RequestClose()) {
        Core::Logger::Warning("Editor", "New Scene cancelled due to unsaved changes.");
        return;
    }

    Core::Logger::Info("Editor", "Creating New Scene...");
    Scene::SceneManager::Get().Reset();

    auto aliveEntities = m_registry->GetAliveEntities();
    for (auto entity : aliveEntities) {
        m_registry->DestroyEntity(entity);
    }

    m_currentScenePath.clear();
    UndoSystem::Get().Clear();
    m_isDirty = false;
    Core::Logger::Info("Editor", "New Scene initialized.");
}

bool Editor::IsDirty() const {
    return UndoSystem::Get().IsDirty() || m_isDirty;
}

void Editor::SetDirty(bool dirty) {
    m_isDirty = dirty;
    UndoSystem::Get().SetDirty(dirty);
}

bool Editor::RequestClose() {
    if (IsDirty()) {
        // Scene has unsaved changes. Return false so callers (OpenScene, NewScene)
        // cancel their operation. The UI layer is expected to have already prompted
        // the user before calling those functions, but this acts as a hard guard.
        Core::Logger::Warning("Editor", "[Warning] Scene has unsaved changes! Operation cancelled.");
        return false;
    }
    return true;
}

} // namespace KumariEngine::Editor
