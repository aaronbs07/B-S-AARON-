#include "engine.hpp"
#include "core/logger.hpp"
#include "window/window.hpp"
#include "input/input.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
#include "terrain/terrain_manager.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera.hpp"
#include "camera/camera_system.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "timeline/cinematic_system.hpp"
#include "core/debug_overlay.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "ecs/ecs.hpp"
#include "scene/scene_manager.hpp"
#include "physics/physics_system.hpp"
#include "physics/debug_renderer.hpp"
#include "scripting/script_engine.hpp"
#include "networking/NetworkManager.hpp"
#include "gameplay/GameInstance.hpp"

namespace KumariEngine::Core {

Engine::Engine() = default;
Engine::~Engine() {
    Shutdown();
}

bool Engine::Initialize(std::string_view windowTitle, int width, int height) {
    Logger::Info("Engine", "Initializing Kumari Engine...");

    // 1. Initialize Window System
    m_window = std::make_unique<Window::Window>();
    if (!m_window->Initialize(windowTitle, width, height)) {
        Logger::Error("Engine", "Failed to initialize Window system.");
        return false;
    }

    // 2. Initialize Input System
    m_input = std::make_unique<Input::Input>();
    m_input->Initialize(m_window->GetNativeWindow());

    // 3. Initialize Vulkan Renderer
    auto vulkanRenderer = std::make_unique<Renderer::VulkanRenderer>();
    if (!vulkanRenderer->Initialize(m_window.get())) {
        Logger::Error("Engine", "Failed to initialize Vulkan Renderer.");
        return false;
    }
    m_renderer = std::move(vulkanRenderer);

    // Initialize ECS registry and Scene Graph Manager
    m_registry = std::make_unique<ECS::Registry>();
    if (!Scripting::ScriptEngine::Get().Initialize(m_registry.get())) {
        Logger::Error("Engine", "Failed to initialize Script Engine.");
        return false;
    }
    Scene::SceneManager::Get().Initialize(m_registry.get());
    Scene::SceneManager::Get().SetChunkSize(64.0f);
    Scene::SceneManager::Get().SetLoadRadius(2);
    Scene::SceneManager::Get().SetUnloadRadius(3);

    // Initialize Network Foundation
    if (!Networking::NetworkManager::Get().Initialize()) {
        Logger::Error("Engine", "Failed to initialize Network system.");
        return false;
    }

    // Initialize Physics simulation wrapper
    m_physicsSystem = std::make_unique<Physics::PhysicsSystem>();

    // Initialize Terrain Manager
    Terrain::TerrainManager::Get().Initialize(1337, 64.0f);

    // Register a default free-fly camera
    auto activeCam = std::make_shared<Camera::Camera>();
    activeCam->SetMode(Camera::CameraMode::Free);
    activeCam->SetTranslationSmoothing(0.0f);
    activeCam->SetRotationSmoothing(0.0f);
    activeCam->SetPosition(glm::vec3(0.0f, 20.0f, 0.0f));
    activeCam->SetPriority(100);
    Camera::CameraManager::Get().RegisterCamera("Main", activeCam);
    Camera::CameraManager::Get().SetActiveCamera("Main");

    // Lock cursor for WASD/mouse fly navigation
    glfwSetInputMode(m_window->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Instantiate debug overlay
    m_debugOverlay = std::make_unique<DebugOverlay>();

    // Initialize GameInstance and Gameplay Foundation
    if (!GameFramework::GameInstance::Get().Initialize(m_registry.get())) {
        Logger::Error("Engine", "Failed to initialize GameInstance.");
        return false;
    }

    m_running = true;
    m_lastFrameTime = static_cast<float>(glfwGetTime());

    Logger::Info("Engine", "Kumari Engine initialized successfully.");
    return true;
}

void Engine::Run() {
    Logger::Info("Engine", "Entering main game loop.");
    while (m_running) {
        ProcessEvents();

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;

        Update(deltaTime);
        Render();
    }
}

void Engine::ProcessEvents() {
    if (m_window->ShouldClose()) {
        m_running = false;
    }
    glfwPollEvents();
}

void Engine::Update(float deltaTime) {
    m_input->Update();

    // Escape exits the application
    if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE)) {
        Logger::Info("Engine", "Escape key detected. Triggering shutdown.");
        m_running = false;
    }

    // Run scripting engine updates
    Scripting::ScriptEngine::Get().Update(deltaTime);

    // Camera System update (runs before manager to apply tracking/shake parameters)
    Camera::CameraSystem::Get().Update(m_registry.get(), deltaTime, m_input.get());

    // Cinematic / Timeline System update
    Timeline::CinematicSystem::Get().Update(m_registry.get(), deltaTime);

    // Audio System update
    Audio::AudioSystem::Get().Update(m_registry.get(), deltaTime);

    auto activeCam = Camera::CameraManager::Get().GetActiveCamera();
    if (activeCam) {
        // If the active camera is not controlled by the ECS system, update it manually
        activeCam->Update(deltaTime, m_input.get());
    }
    Camera::CameraManager::Get().Update(deltaTime);

    glm::vec3 viewerPos(0.0f);
    if (activeCam) {
        viewerPos = activeCam->GetCurrentPosition();
    }

    // Fixed timestep physics update
    m_physicsAccumulator += deltaTime;
    const float fixedTime = 0.01667f; // 60 Hz
    while (m_physicsAccumulator >= fixedTime) {
        if (m_physicsSystem) {
            m_physicsSystem->Update(m_registry.get(), fixedTime);
        }
        m_physicsAccumulator -= fixedTime;
    }

    // Update Scene Graph streaming and hierarchy transforms
    Scene::SceneManager::Get().SetViewerPosition(viewerPos);
    Scene::SceneManager::Get().Update(deltaTime);

    auto vulkanRenderer = dynamic_cast<Renderer::VulkanRenderer*>(m_renderer.get());
    VkDevice device = vulkanRenderer ? vulkanRenderer->GetDevice() : VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = vulkanRenderer ? vulkanRenderer->GetPhysicalDevice() : VK_NULL_HANDLE;
    VkCommandPool commandPool = vulkanRenderer ? vulkanRenderer->GetCommandPool() : VK_NULL_HANDLE;
    VkQueue graphicsQueue = vulkanRenderer ? vulkanRenderer->GetGraphicsQueue() : VK_NULL_HANDLE;

    Terrain::TerrainManager::Get().Update(viewerPos, device, physicalDevice, commandPool, graphicsQueue);

    // Listen for debug visual toggle keybinds
    if (m_input->IsKeyPressed(GLFW_KEY_F1)) {
        Terrain::TerrainManager::Get().ToggleWireframe();
    }
    if (m_input->IsKeyPressed(GLFW_KEY_F2)) {
        Terrain::TerrainManager::Get().ToggleChunkBorders();
    }
    if (m_input->IsKeyPressed(GLFW_KEY_F3)) {
        Terrain::TerrainManager::Get().ToggleDebugVis();
    }
    if (m_input->IsKeyPressed(GLFW_KEY_F4)) {
        auto& pdbg = Physics::PhysicsDebugRenderer::Get();
        pdbg.SetEnabled(!pdbg.IsEnabled());
        Logger::Info("Engine", "Physics Debug Drawing: %s", pdbg.IsEnabled() ? "ON" : "OFF");
    }

    if (m_debugOverlay) {
        m_debugOverlay->Update(deltaTime, m_window.get());
    }

    // Update Gameplay Foundation
    GameFramework::GameInstance::Get().Update(deltaTime);

    if (m_updateCallback) {
        m_updateCallback(deltaTime);
    }
}

void Engine::Render() {
    m_renderer->BeginFrame();
    m_renderer->DrawFrame();
    if (m_renderCallback) {
        m_renderCallback();
    }
    m_renderer->EndFrame();
}

void Engine::Shutdown() {
    if (!m_running && !m_window && !m_renderer) return;

    Logger::Info("Engine", "Beginning shutdown sequence...");

    m_running = false;

    Networking::NetworkManager::Get().Shutdown();

    Scene::SceneManager::Get().Shutdown();

    Scripting::ScriptEngine::Get().Shutdown();

    // Shutdown Gameplay Foundation
    GameFramework::GameInstance::Get().Shutdown();

    if (m_physicsSystem) {
        m_physicsSystem.reset();
    }
    if (m_registry) {
        m_registry.reset();
    }

    if (m_renderer) {
        auto vulkanRenderer = dynamic_cast<Renderer::VulkanRenderer*>(m_renderer.get());
        VkDevice device = vulkanRenderer ? vulkanRenderer->GetDevice() : VK_NULL_HANDLE;
        Terrain::TerrainManager::Get().Shutdown(device);
    }

    if (m_debugOverlay) {
        m_debugOverlay.reset();
    }

    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    if (m_input) {
        m_input.reset();
    }

    if (m_window) {
        m_window->Shutdown();
        m_window.reset();
    }

    Logger::Info("Engine", "Shutdown completed cleanly.");
}

} // namespace KumariEngine::Core
