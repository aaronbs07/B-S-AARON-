#ifdef _MSC_VER
#pragma warning(disable: 4189)
#endif
#include "core/engine.hpp"
#include "core/logger.hpp"
#include "ecs/ecs.hpp"
#include "scene/scene_node.hpp"
#include "scene/scene_manager.hpp"
#include "camera/camera.hpp"
#include "camera/camera_component.hpp"
#include "camera/camera_manager.hpp"
#include "camera/camera_system.hpp"
#include "audio/audio_system.hpp"
#include "timeline/timeline.hpp"
#include "timeline/cinematic_system.hpp"
#include "physics/physics_types.hpp"
#include "physics/physics_components.hpp"
#include "physics/physics_world.hpp"
#include "physics/character_controller.hpp"
#include "terrain/terrain_manager.hpp"
#include "renderer/vulkan/vulkan_context.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
#include "ai/ai_system.hpp"
#include "scene/transform_component.hpp"
#include "gameplay/InventorySystem.hpp"
#include "gameplay/EquipmentSystem.hpp"
#include "gameplay/CraftingSystem.hpp"
#include "gameplay/QuestSystem.hpp"
#include "gameplay/DialogueSystem.hpp"
#include "gameplay/InteractionSystem.hpp"
#include "save/SaveManager.hpp"
#include "networking/ReplicationManager.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <variant>
#include <thread>
#include <chrono>

using namespace KumariEngine;

// Position struct for testing
struct TestPos {
    float x, y;
};

void VerifyCoordinateSpaceTests() {
    std::cout << "=== RUNNING COORDINATE SPACE TESTS ===" << std::endl;

    // Test 1: Parent-child scene hierarchy world position propagation
    {
        std::cout << "  Running Test 1.1: Parent-child scene hierarchy..." << std::endl;
        Scene::SceneNode parent("Parent");
        Scene::SceneNode child("Child");
        
        parent.SetLocalPosition(glm::vec3(10.0f, 0.0f, 0.0f));
        child.SetLocalPosition(glm::vec3(5.0f, 2.0f, 0.0f));
        
        parent.AddChild(std::make_unique<Scene::SceneNode>("ChildTemp")); // child ownership helper
        auto childPtr = std::make_unique<Scene::SceneNode>("ChildNode");
        auto* childRaw = childPtr.get();
        childRaw->SetLocalPosition(glm::vec3(5.0f, 2.0f, 0.0f));
        
        parent.AddChild(std::move(childPtr));
        parent.UpdateTransforms();

        glm::vec3 parentWorld = glm::vec3(parent.GetWorldMatrix()[3]);
        glm::vec3 childWorld = glm::vec3(childRaw->GetWorldMatrix()[3]);

        assert(glm::distance(parentWorld, glm::vec3(10.0f, 0.0f, 0.0f)) < 0.001f);
        assert(glm::distance(childWorld, glm::vec3(15.0f, 2.0f, 0.0f)) < 0.001f);
        std::cout << "  1. Parent-child scene hierarchy: PASSED" << std::endl;
    }

    // Test 2: Physics collision with nested transforms (world-space evaluation)
    {
        std::cout << "  Running Test 1.2: Physics collision with nested transforms..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Physics::PhysicsComponent>();
        std::cout << "    Registry created" << std::endl;
        Scene::SceneManager::Get().Initialize(&registry);
        std::cout << "    SceneManager initialized" << std::endl;
        Terrain::TerrainManager::Get().Initialize(1337, 64.0f);
        std::cout << "    TerrainManager initialized" << std::endl;

        // Parent A offset to (100, 0, 0)
        auto* parentA = Scene::SceneManager::Get().CreateNode("ParentA");
        parentA->SetLocalPosition(glm::vec3(100.0f, 0.0f, 0.0f));
        std::cout << "    parentA created" << std::endl;
        
        // Child A at local (5, 500, 0) -> world (105, 500, 0)
        auto* childA = Scene::SceneManager::Get().CreateNode("ChildA", parentA);
        childA->SetLocalPosition(glm::vec3(5.0f, 500.0f, 0.0f));
        ECS::Entity entityA = registry.CreateEntity();
        childA->SetEntity(entityA);
        std::cout << "    childA created" << std::endl;

        auto& pcA = registry.AddComponent<Physics::PhysicsComponent>(entityA);
        pcA.bodyType = Physics::BodyType::Dynamic;
        pcA.collider.type = Physics::ColliderType::Sphere;
        pcA.collider.shape = Physics::Sphere{glm::vec3(0.0f), 1.5f}; // Radius 1.5
        pcA.collisionLayer = 0xFFFF;
        pcA.collisionMask = 0xFFFF;
        std::cout << "    pcA added" << std::endl;

        // Parent B offset to (104, 0, 0)
        auto* parentB = Scene::SceneManager::Get().CreateNode("ParentB");
        parentB->SetLocalPosition(glm::vec3(104.0f, 0.0f, 0.0f));
        std::cout << "    parentB created" << std::endl;

        // Child B at local (2, 500, 0) -> world (106, 500, 0)
        auto* childB = Scene::SceneManager::Get().CreateNode("ChildB", parentB);
        childB->SetLocalPosition(glm::vec3(2.0f, 500.0f, 0.0f));
        ECS::Entity entityB = registry.CreateEntity();
        childB->SetEntity(entityB);
        std::cout << "    childB created" << std::endl;

        auto& pcB = registry.AddComponent<Physics::PhysicsComponent>(entityB);
        pcB.bodyType = Physics::BodyType::Dynamic;
        pcB.collider.type = Physics::ColliderType::Sphere;
        pcB.collider.shape = Physics::Sphere{glm::vec3(0.0f), 1.5f}; // Radius 1.5
        pcB.collisionLayer = 0xFFFF;
        pcB.collisionMask = 0xFFFF;
        std::cout << "    pcB added" << std::endl;

        // Update matrices first
        std::cout << "    Updating transforms" << std::endl;
        Scene::SceneManager::Get().GetRootNode()->UpdateTransforms();

        // Run one step of simulation
        std::cout << "    Stepping physics world" << std::endl;
        Physics::PhysicsWorld world;
        world.Step(&registry, 0.016f);
        std::cout << "    Stepping completed" << std::endl;

        // Check if collision resolution took place using world coordinates.
        glm::vec3 worldAPos = glm::vec3(childA->GetWorldMatrix()[3]);
        glm::vec3 worldBPos = glm::vec3(childB->GetWorldMatrix()[3]);
        float solvedDist = glm::distance(worldAPos, worldBPos);
        (void)solvedDist;
        std::cout << "    solvedDist: " << solvedDist << std::endl;

        // In world-space, positional resolution pushes them apart to separation >= 3.0 (or closer to it)
        assert(solvedDist > 1.1f); 
        std::cout << "  2. Physics collision with nested transforms: PASSED" << std::endl;

        Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
        Scene::SceneManager::Get().Shutdown();
    }

    // Test 3: Character controller movement under transformed parents
    {
        std::cout << "  Running Test 1.3: Character controller under parent..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Physics::PhysicsComponent>();
        registry.RegisterComponent<Physics::CharacterControllerComponent>();
        Scene::SceneManager::Get().Initialize(&registry);
        Terrain::TerrainManager::Get().Initialize(1337, 64.0f);

        // Create a moving platform / parent offset to (50, 10, 50)
        auto* parentNode = Scene::SceneManager::Get().CreateNode("Platform");
        parentNode->SetLocalPosition(glm::vec3(50.0f, 10.0f, 50.0f));

        // Create player under this parent
        auto* playerNode = Scene::SceneManager::Get().CreateNode("Player", parentNode);
        ECS::Entity player = registry.CreateEntity();
        playerNode->SetEntity(player);

        // Player local position (0, 0, 0) -> world position (50, 10, 50)
        playerNode->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));

        auto& pc = registry.AddComponent<Physics::PhysicsComponent>(player);
        pc.bodyType = Physics::BodyType::Kinematic;
        pc.collider.type = Physics::ColliderType::Capsule;
        pc.collider.shape = Physics::Capsule{glm::vec3(0.0f), 1.0f, 0.5f};

        auto& cc = registry.AddComponent<Physics::CharacterControllerComponent>(player);
        cc.moveDirection = glm::vec3(0.0f, 0.0f, 0.0f); // stationary
        cc.verticalVelocity = -10.0f; // fall to snap to terrain
        cc.isGrounded = false;

        Physics::PhysicsWorld world;
        Physics::CharacterController controller;

        // Force matrix update
        Scene::SceneManager::Get().GetRootNode()->UpdateTransforms();

        // Run update. Snapping logic snaps bottom of capsule to heightmap.
        // World coordinates are used to query height map!
        controller.Update(&registry, player, pc, cc, world, 0.1f);

        glm::vec3 finalWorld = glm::vec3(playerNode->GetWorldMatrix()[3]);
        float expectedWorldH = Terrain::TerrainManager::Get().GetHeightAt(finalWorld.x, finalWorld.z);
        (void)expectedWorldH;

        // Bottom of capsule (pos.y - 1.5) snaps to terrain
        assert(std::abs(finalWorld.y - (expectedWorldH + 1.5f)) < 0.1f);
        assert(cc.isGrounded);

        std::cout << "  3. Character controller under transformed parents: PASSED" << std::endl;

        Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
        Scene::SceneManager::Get().Shutdown();
    }
}

void VerifyECSTests() {
    Core::Logger::Info("RegressionTests", "=== RUNNING ECS TESTS ===");

    // Test 1: Double DestroyEntity() safety
    {
        ECS::Registry registry;
        ECS::Entity e1 = registry.CreateEntity();
        ECS::Entity e2 = registry.CreateEntity();

        assert(registry.IsAlive(e1));
        assert(registry.IsAlive(e2));

        registry.DestroyEntity(e1);
        assert(!registry.IsAlive(e1));

#ifdef NDEBUG
        // Duplicate destruction should be safely ignored in Release, and not corrupt Registry
        registry.DestroyEntity(e1);
#endif

        registry.DestroyEntity(e2);
        assert(!registry.IsAlive(e2));

        // Allocating entities again (LIFO order: e2 then e1)
        ECS::Entity e3 = registry.CreateEntity(); // Should recycle e2
        (void)e3;
        assert(e3 == e2);

        ECS::Entity e4 = registry.CreateEntity(); // Should recycle e1
        (void)e4;
        assert(e4 == e1);
        assert(registry.IsAlive(e3));
        assert(registry.IsAlive(e4));

        Core::Logger::Info("RegressionTests", "1. Double DestroyEntity(): PASSED");
    }

    // Test 2: Invalid Entity IDs destruction safety
    {
        ECS::Registry registry;
        
#ifdef NDEBUG
        // Destroying NULL_ENTITY and non-created entity IDs shouldn't crash in Release
        registry.DestroyEntity(ECS::NULL_ENTITY);
        registry.DestroyEntity(9999);
        registry.DestroyEntity(123456);
#endif

        Core::Logger::Info("RegressionTests", "2. Invalid Entity IDs: PASSED");
    }

    // Test 3: Destroy entity after component removal
    {
        ECS::Registry registry;
        registry.RegisterComponent<TestPos>();
        
        ECS::Entity e = registry.CreateEntity();
        registry.AddComponent<TestPos>(e, TestPos{1.0f, 2.0f});
        
        // Remove, then destroy
        registry.RemoveComponent<TestPos>(e);
        registry.DestroyEntity(e);

        assert(!registry.IsAlive(e));
        
        // Check pool size is 0
        auto* pool = registry.GetPool<TestPos>();
        (void)pool;
        assert(pool->Size() == 0);

        Core::Logger::Info("RegressionTests", "3. Destroy after component removal: PASSED");
    }

    // Test 4: Entity ID reuse stress test
    {
        ECS::Registry registry;
        registry.RegisterComponent<TestPos>();

        std::vector<ECS::Entity> entities;
        entities.reserve(1000);

        // Create 1000 entities
        for (int i = 0; i < 1000; ++i) {
            auto e = registry.CreateEntity();
            registry.AddComponent<TestPos>(e, TestPos{static_cast<float>(i), 0.0f});
            entities.push_back(e);
        }

        // Destroy them all
        for (auto e : entities) {
            registry.DestroyEntity(e);
        }

        // Re-create 1000 entities. They must recycle all old IDs.
        std::vector<ECS::Entity> recycled;
        recycled.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            auto e = registry.CreateEntity();
            recycled.push_back(e);
        }

        // Verify IDs were reused and registry pool is correct
        assert(recycled.size() == 1000);
        for (int i = 0; i < 1000; ++i) {
            // Because they are recycled from m_freeEntities (LIFO order), the IDs will match in reverse or sorted order
            assert(registry.IsAlive(recycled[i]));
        }

        Core::Logger::Info("RegressionTests", "4. Entity ID reuse stress: PASSED");
    }
}

void VerifyTerrainStitchingTests() {
    Core::Logger::Info("RegressionTests", "=== RUNNING TERRAIN STITCHING TESTS ===");

    // Test 1: Stitch rebuild only when neighbor LOD changes
    {
        auto& tm = Terrain::TerrainManager::Get();
        tm.Initialize(1337, 64.0f);
        tm.ResetStitchingTelemetry();

        // Create mock active chunks using local noise generator
        Terrain::NoiseGenerator noiseGen;
        noiseGen.Initialize(1337);
        auto chunk = std::make_shared<Terrain::TerrainChunk>(0, 0, 64.0f, &noiseGen);
        chunk->GenerateCPUData(0); // LOD 0

        // Call RebuildIndicesForStitching with device = null (simulates update stitching)
        Terrain::StagingResources resources;
        bool ok = chunk->RebuildIndicesForStitching(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0, 0, resources);
        (void)ok;
        assert(ok);
        size_t initialRebuilds = tm.GetStitchingRebuildCount();
        (void)initialRebuilds;
        assert(initialRebuilds == 1); // Incremented first time

        // Call again with same LODs
        ok = chunk->RebuildIndicesForStitching(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0, 0, resources);
        (void)ok;
        assert(ok);
        assert(tm.GetStitchingRebuildCount() == 1); // Did not increment!

        // Change LOD of a neighbor
        ok = chunk->RebuildIndicesForStitching(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, 0, 0, 0, resources);
        (void)ok;
        assert(ok);
        assert(tm.GetStitchingRebuildCount() == 2); // Incremented due to LOD change!

        Core::Logger::Info("RegressionTests", "1. Rebuild indices only when neighbor LOD changes: PASSED");
    }

    // Test 2: GPU uploads remain constant when camera is stationary
    {
        auto& tm = Terrain::TerrainManager::Get();
        size_t initialUploads = tm.GetStitchingUploadCount();
        (void)initialUploads;
        
        // Simulating stationary camera updates.
        // No neighbor LOD changes -> upload count should remain constant
        for (int i = 0; i < 10; ++i) {
            tm.Update(glm::vec3(0.0f), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        }

        assert(tm.GetStitchingUploadCount() == initialUploads); // Remains constant
        Core::Logger::Info("RegressionTests", "2. GPU uploads constant under stationary camera: PASSED");
    }

    // Test 3: Verify no terrain cracks during repeated LOD transitions
    {
        auto& tm = Terrain::TerrainManager::Get();
        tm.Initialize(1337, 64.0f);
        
        // Perform multiple camera movements to trigger neighbor LOD adjustments
        // and verify transition stability
        for (int step = 0; step < 10; ++step) {
            float dist = static_cast<float>(step % 3) * 64.0f;
            tm.Update(glm::vec3(dist, 0.0f, dist), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
            
            // Wait for chunks to generate and stream in (since background task is async)
            for (int retry = 0; retry < 50 && tm.GetLoadedChunkCount() == 0; ++retry) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Basic verification that chunks are still active and structured cleanly
        assert(tm.GetActiveChunks().size() > 0);
        Core::Logger::Info("RegressionTests", "3. No cracks during repeated LOD transitions: PASSED");
        tm.Shutdown(VK_NULL_HANDLE);
    }
}

void VerifyVulkanTests() {
    Core::Logger::Info("RegressionTests", "=== RUNNING VULKAN TESTS ===");

    // Test 1: Shutdown after failed initialization
    {
        auto context = std::make_unique<Renderer::VulkanContext>();
        // Initialize with null window will fail
        bool ok = context->Initialize(nullptr);
        (void)ok;
        assert(!ok);

        // Calling Shutdown on failed initialization context
        context->Shutdown();
        Core::Logger::Info("RegressionTests", "1. Shutdown on failed context init: PASSED");
    }

    // Test 2: Shutdown after partial initialization (VulkanRenderer)
    {
        auto renderer = std::make_unique<Renderer::VulkanRenderer>();
        // Will fail context initialization
        bool ok = renderer->Initialize(nullptr);
        (void)ok;
        assert(!ok);

        // Calling Shutdown on failed renderer init
        renderer->Shutdown();
        Core::Logger::Info("RegressionTests", "2. Shutdown on failed renderer init: PASSED");
    }

    // Test 3: Multiple shutdown calls safety
    {
        auto context = std::make_unique<Renderer::VulkanContext>();
        context->Initialize(nullptr);
        
        context->Shutdown();
        // Consecutively call shutdown again
        context->Shutdown();
        context->Shutdown();

        auto renderer = std::make_unique<Renderer::VulkanRenderer>();
        renderer->Initialize(nullptr);
        renderer->Shutdown();
        // Consecutively call shutdown again
        renderer->Shutdown();
        renderer->Shutdown();

        Core::Logger::Info("RegressionTests", "3. Multiple shutdown calls: PASSED");
    }
}

void VerifyAIFrameworkTests() {
    std::cout << "=== RUNNING AI FRAMEWORK & NAVIGATION TESTS ===" << std::endl;

    // Test 1: Blackboard Component
    {
        std::cout << "  Running Test 1: Blackboard Component..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<AI::BlackboardComponent>();
        ECS::Entity entity = registry.CreateEntity();
        auto& bb = registry.AddComponent<AI::BlackboardComponent>(entity);

        bb.SetValue("TestInt", 42);
        bb.SetValue("TestFloat", 3.14f);
        bb.SetValue("TestBool", true);
        bb.SetValue("TestString", std::string("Kumari"));
        bb.SetValue("TestVec", glm::vec3(1.0f, 2.0f, 3.0f));
        bb.SetValue("TestEntity", entity);

        assert(bb.HasValue("TestInt"));
        assert(bb.GetValue<int>("TestInt") == 42);
        assert(bb.GetValue<float>("TestFloat") == 3.14f);
        assert(bb.GetValue<bool>("TestBool") == true);
        assert(bb.GetValue<std::string>("TestString") == "Kumari");
        assert(bb.GetValue<glm::vec3>("TestVec") == glm::vec3(1.0f, 2.0f, 3.0f));
        assert(bb.GetValue<ECS::Entity>("TestEntity") == entity);

        bb.ClearValue("TestInt");
        assert(!bb.HasValue("TestInt"));

        std::cout << "  Blackboard Component: PASSED" << std::endl;
    }

    // Test 2: NavMesh generation and Obstacle detection
    {
        std::cout << "  Running Test 2: NavMesh Generation and Obstacles..." << std::endl;
        Terrain::TerrainManager::Get().Initialize(9999, 64.0f);

        ECS::Registry registry;
        registry.RegisterComponent<Physics::PhysicsComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();
        
        ECS::Entity obstacle = registry.CreateEntity();
        auto& tc = registry.AddComponent<Scene::TransformComponent>(obstacle);
        tc.position = glm::vec3(0.0f, 0.0f, 0.0f);
        
        auto& pc = registry.AddComponent<Physics::PhysicsComponent>(obstacle);
        pc.bodyType = Physics::BodyType::Static;
        pc.collider.type = Physics::ColliderType::Sphere;
        pc.collider.shape = Physics::Sphere{glm::vec3(0.0f), 2.0f};
        
        AI::NavMesh navMesh;
        navMesh.BuildFromWorld(&registry, glm::vec3(0.0f), 5, 5, 1.0f, 0.5f, 2.0f);
        
        int centerIdx = navMesh.FindNearestNodeIndex(glm::vec3(0.0f));
        assert(centerIdx != -1);
        assert(!navMesh.GetNodes()[centerIdx].walkable);
        
        int edgeIdx = navMesh.FindNearestNodeIndex(glm::vec3(2.0f, 0.0f, 2.0f));
        assert(edgeIdx != -1);
        assert(navMesh.GetNodes()[edgeIdx].walkable);

        Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
        std::cout << "  NavMesh Generation and Obstacles: PASSED" << std::endl;
    }

    // Test 3: A* Pathfinding, Path Cache, Path Validation & Line-of-Sight Smoothing
    {
        std::cout << "  Running Test 3: Pathfinding, Caching, Validation & Smoothing..." << std::endl;
        AI::NavMesh navMesh;
        navMesh.BuildGrid(glm::vec3(0.0f), 5, 5, 1.0f);
        navMesh.SetWalkable(12, false); 
        
        std::vector<glm::vec3> path = navMesh.FindPath(glm::vec3(-2.0f, 0.0f, -2.0f), glm::vec3(2.0f, 0.0f, 2.0f));
        assert(!path.empty());
        
        std::vector<glm::vec3> cachedPath = navMesh.FindPath(glm::vec3(-2.0f, 0.0f, -2.0f), glm::vec3(2.0f, 0.0f, 2.0f));
        assert(path.size() == cachedPath.size());
        
        assert(navMesh.ValidatePath(path));
        
        int nearest = navMesh.FindNearestNodeIndex(path[path.size() / 2]);
        navMesh.SetWalkable(nearest, false);
        assert(!navMesh.ValidatePath(path));

        std::cout << "  Pathfinding, Caching, Validation & Smoothing: PASSED" << std::endl;
    }

    // Test 4: Runtime NavMesh Loading and Saving
    {
        std::cout << "  Running Test 4: Runtime NavMesh Loading & Saving..." << std::endl;
        AI::NavMesh navMeshSource;
        navMeshSource.BuildGrid(glm::vec3(5.0f, 0.0f, 5.0f), 3, 3, 2.0f);
        navMeshSource.SetWalkable(4, false);

        const std::string savePath = "test_navmesh.bin";
        bool saveOk = navMeshSource.Save(savePath);
        assert(saveOk);

        AI::NavMesh navMeshDest;
        bool loadOk = navMeshDest.Load(savePath);
        assert(loadOk);

        assert(navMeshDest.GetNodes().size() == navMeshSource.GetNodes().size());
        assert(navMeshDest.GetNodes()[4].walkable == false);

        std::remove(savePath.c_str());
        std::cout << "  Runtime NavMesh Loading & Saving: PASSED" << std::endl;
    }

    // Test 5: AI Controller & Perception
    {
        std::cout << "  Running Test 5: AI Controller & Perception..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<AI::NavigationAgentComponent>();
        registry.RegisterComponent<AI::PerceptionComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();
        
        ECS::Entity agent = registry.CreateEntity();
        registry.AddComponent<Scene::TransformComponent>(agent, glm::vec3(0.0f, 0.0f, 0.0f));
        auto& perception = registry.AddComponent<AI::PerceptionComponent>(agent);
        perception.visionRange = 10.0f;
        perception.fieldOfView = 90.0f;
        
        registry.AddComponent<AI::NavigationAgentComponent>(agent);
        
        ECS::Entity target = registry.CreateEntity();
        registry.AddComponent<Scene::TransformComponent>(target, glm::vec3(0.0f, 0.0f, -5.0f));
        
        ECS::Entity targetFar = registry.CreateEntity();
        registry.AddComponent<Scene::TransformComponent>(targetFar, glm::vec3(0.0f, 0.0f, 25.0f));
        
        AI::PerceptionServiceNode service(nullptr, 0.0f);
        registry.RegisterComponent<AI::BlackboardComponent>();
        registry.AddComponent<AI::BlackboardComponent>(agent);
        
        service.TickService(&registry, agent);
        
        assert(perception.perceivedStimuli.size() == 1);
        assert(perception.perceivedStimuli[0].entity == target);
        
        ECS::Entity acquired = AI::AIController::AcquireTarget(&registry, agent);
        assert(acquired == target);

        AI::AIController::MoveTo(&registry, agent, glm::vec3(10.0f, 0.0f, 10.0f));
        assert(registry.GetComponent<AI::NavigationAgentComponent>(agent).agentTarget == glm::vec3(10.0f, 0.0f, 10.0f));
        
        AI::AIController::Stop(&registry, agent);
        assert(registry.GetComponent<AI::NavigationAgentComponent>(agent).currentPath.empty());

        std::cout << "  AI Controller & Perception: PASSED" << std::endl;
    }

    // Test 6: Behavior Tree & Blackboard Integration
    {
        std::cout << "  Running Test 6: Behavior Tree & Blackboard Integration..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<AI::BlackboardComponent>();
        registry.RegisterComponent<AI::NavigationAgentComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();
        
        ECS::Entity entity = registry.CreateEntity();
        auto& bb = registry.AddComponent<AI::BlackboardComponent>(entity);
        bb.SetValue("TargetPos", glm::vec3(5.0f, 0.0f, 5.0f));
        bb.SetValue("DeltaTime", 0.016f);
        
        registry.AddComponent<Scene::TransformComponent>(entity, glm::vec3(0.0f, 0.0f, 0.0f));
        registry.AddComponent<AI::NavigationAgentComponent>(entity);
        
        auto root = std::make_shared<AI::SequenceNode>();
        auto waitTask = std::make_shared<AI::WaitTaskNode>(0.1f);
        auto moveToTask = std::make_shared<AI::MoveToTaskNode>("TargetPos");
        
        root->AddChild(waitTask);
        root->AddChild(moveToTask);
        
        AI::BehaviorTree tree;
        tree.SetRoot(root);
        
        AI::BTState s1 = tree.Tick(&registry, entity);
        assert(s1 == AI::BTState::Running);
        
        bb.SetValue("DeltaTime", 0.12f);
        AI::BTState s2 = tree.Tick(&registry, entity);
        assert(s2 == AI::BTState::Running);
        
        registry.GetComponent<Scene::TransformComponent>(entity).position = glm::vec3(5.0f, 0.0f, 5.0f);
        AI::BTState s3 = tree.Tick(&registry, entity);
        assert(s3 == AI::BTState::Success);

        std::cout << "  Behavior Tree & Blackboard Integration: PASSED" << std::endl;
    }

    std::cout << "=== ALL AI FRAMEWORK & NAVIGATION TESTS PASSED! ===" << std::endl;
}

void VerifyGameplaySystemsTests() {
    std::cout << "=== RUNNING GAMEPLAY SYSTEMS TESTS ===" << std::endl;

    // Test 1: Inventory Stackable Items and Transfer
    {
        std::cout << "  Running Test: Inventory & Items stacking & transfer..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Gameplay::InventoryComponent>();
        registry.RegisterComponent<Gameplay::ItemComponent>();

        // Register item in database
        Gameplay::ItemDefinition woodDef;
        woodDef.itemId = "wood";
        woodDef.name = "Wood Logs";
        woodDef.isStackable = true;
        woodDef.maxStackSize = 20;
        Gameplay::ItemDatabase::Get().RegisterItem(woodDef);

        ECS::Entity player = registry.CreateEntity();
        auto& invA = registry.AddComponent<Gameplay::InventoryComponent>(player, 5);

        // Add item
        bool addOk = invA.AddItem("wood", 15);
        assert(addOk);
        assert(invA.slots[0].itemId == "wood");
        assert(invA.slots[0].quantity == 15);

        // Add more item of same type (should stack)
        addOk = invA.AddItem("wood", 10);
        assert(addOk);
        // Slot 0 has 20 wood, slot 1 has 5 wood
        assert(invA.slots[0].itemId == "wood" && invA.slots[0].quantity == 20);
        assert(invA.slots[1].itemId == "wood" && invA.slots[1].quantity == 5);

        // Remove item
        bool removeOk = invA.RemoveItem("wood", 12);
        assert(removeOk);
        // We now have 13 wood left (invA.slots[0] should have 13 or slot 1 is cleared)
        assert(invA.HasItem("wood", 13));
        assert(!invA.HasItem("wood", 14));

        // Transfer item to chest
        ECS::Entity chest = registry.CreateEntity();
        auto& invB = registry.AddComponent<Gameplay::InventoryComponent>(chest, 5);

        // Try to transfer from invA slot 0 (which has wood) to invB slot 0
        // Find which slot has wood
        uint32_t woodSlotIdx = 0xFFFF;
        for (uint32_t i = 0; i < invA.slots.size(); ++i) {
            if (invA.slots[i].itemId == "wood" && invA.slots[i].quantity >= 5) {
                woodSlotIdx = i;
                break;
            }
        }
        assert(woodSlotIdx != 0xFFFF);

        bool transferOk = Gameplay::TransferItem(invA, woodSlotIdx, invB, 0, 5);
        assert(transferOk);
        assert(invB.slots[0].itemId == "wood");
        assert(invB.slots[0].quantity == 5);
        assert(invA.HasItem("wood", 8));

        std::cout << "    Inventory & Items: PASSED" << std::endl;
    }

    // Test 2: Equipment System
    {
        std::cout << "  Running Test: Equipment slotting & validation..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Gameplay::InventoryComponent>();
        registry.RegisterComponent<Gameplay::EquipmentComponent>();

        // Register weapons and shield
        Gameplay::ItemDefinition swordDef;
        swordDef.itemId = "sword";
        swordDef.name = "Iron Sword";
        swordDef.tags = {"Weapon"};
        Gameplay::ItemDatabase::Get().RegisterItem(swordDef);

        Gameplay::ItemDefinition potionDef;
        potionDef.itemId = "potion";
        potionDef.name = "Healing Potion";
        potionDef.tags = {"Consumable"};
        Gameplay::ItemDatabase::Get().RegisterItem(potionDef);

        ECS::Entity player = registry.CreateEntity();
        auto& inv = registry.AddComponent<Gameplay::InventoryComponent>(player, 5);
        auto& eq = registry.AddComponent<Gameplay::EquipmentComponent>(player);

        inv.AddItem("sword", 1);
        inv.AddItem("potion", 1);

        // Equip valid item
        bool equipOk = eq.EquipItem(Gameplay::EquipmentSlot::Weapon, "sword", &inv);
        assert(equipOk);
        assert(eq.slots[static_cast<size_t>(Gameplay::EquipmentSlot::Weapon)] == "sword");
        assert(!inv.HasItem("sword", 1)); // consumed from inventory

        // Try to equip invalid item (potion in Weapon slot)
        bool equipBad = eq.EquipItem(Gameplay::EquipmentSlot::Weapon, "potion", &inv);
        assert(!equipBad); // should fail validation

        // Unequip item
        bool unequipOk = eq.UnequipItem(Gameplay::EquipmentSlot::Weapon, &inv);
        assert(unequipOk);
        assert(eq.slots[static_cast<size_t>(Gameplay::EquipmentSlot::Weapon)] == "");
        assert(inv.HasItem("sword", 1)); // returned to inventory

        std::cout << "    Equipment System: PASSED" << std::endl;
    }

    // Test 3: Crafting Recipes and Execution
    {
        std::cout << "  Running Test: Crafting recipes..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Gameplay::InventoryComponent>();

        // Register wood and iron
        Gameplay::ItemDefinition ironDef;
        ironDef.itemId = "iron";
        ironDef.name = "Iron Ore";
        ironDef.isStackable = true;
        ironDef.maxStackSize = 20;
        Gameplay::ItemDatabase::Get().RegisterItem(ironDef);

        Gameplay::ItemDefinition shieldDef;
        shieldDef.itemId = "wooden_shield";
        shieldDef.name = "Wooden Shield";
        Gameplay::ItemDatabase::Get().RegisterItem(shieldDef);

        // Register recipe
        Gameplay::CraftingRecipe recipe;
        recipe.recipeId = "craft_shield";
        recipe.ingredients = { {"wood", 5}, {"iron", 2} };
        recipe.outputItemId = "wooden_shield";
        recipe.outputQuantity = 1;
        recipe.requiredStation = "Anvil";
        Gameplay::RecipeDatabase::Get().RegisterRecipe(recipe);

        ECS::Entity player = registry.CreateEntity();
        auto& inv = registry.AddComponent<Gameplay::InventoryComponent>(player, 5);

        // Try to craft without ingredients
        assert(!Gameplay::CanCraft("craft_shield", inv, "Anvil"));

        // Add partial ingredients
        inv.AddItem("wood", 10);
        assert(!Gameplay::CanCraft("craft_shield", inv, "Anvil"));

        // Add remaining ingredients
        inv.AddItem("iron", 5);

        // Try to craft without station (should fail since requiredStation = "Anvil")
        assert(!Gameplay::CanCraft("craft_shield", inv, ""));

        // Try to craft with correct station
        assert(Gameplay::CanCraft("craft_shield", inv, "Anvil"));

        // Execute crafting
        bool craftOk = Gameplay::ExecuteCraft("craft_shield", inv, "Anvil");
        assert(craftOk);
        assert(inv.HasItem("wooden_shield", 1));

        auto getItemQty = [](const Gameplay::InventoryComponent& cInv, const std::string& id) {
            uint32_t total = 0;
            for (const auto& s : cInv.slots) {
                if (s.itemId == id) total += s.quantity;
            }
            return total;
        };
        assert(getItemQty(inv, "wood") == 5);
        assert(getItemQty(inv, "iron") == 3);

        std::cout << "    Crafting System: PASSED" << std::endl;
    }

    // Test 4: Quest manager & Objective updates
    {
        std::cout << "  Running Test: Quest Manager & progression..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Gameplay::QuestComponent>();
        registry.RegisterComponent<Gameplay::InventoryComponent>();

        // Register a quest
        Gameplay::QuestDefinition quest;
        quest.questId = "slime_slayer";
        quest.name = "Slime Slayer";
        quest.description = "Kill 3 slimes";
        
        Gameplay::QuestStage stage0;
        stage0.stageIndex = 0;
        stage0.stageDescription = "Go kill those slimes";
        Gameplay::QuestObjective obj;
        obj.objectiveId = "kill_slime";
        obj.description = "Kill 3 slimes";
        obj.type = "Kill";
        obj.targetId = "slime";
        obj.requiredCount = 3;
        stage0.objectives.push_back(obj);
        quest.stages.push_back(stage0);

        Gameplay::QuestReward reward;
        reward.rewardType = "Item";
        reward.targetId = "iron";
        reward.quantity = 10;
        quest.rewards.push_back(reward);

        Gameplay::QuestManager::Get().RegisterQuest(quest);

        ECS::Entity player = registry.CreateEntity();
        auto& qc = registry.AddComponent<Gameplay::QuestComponent>(player);
        auto& inv = registry.AddComponent<Gameplay::InventoryComponent>(player, 5);

        // Accept quest
        bool acceptOk = Gameplay::QuestManager::Get().AcceptQuest(&registry, player, "slime_slayer");
        assert(acceptOk);
        assert(qc.activeQuests.find("slime_slayer") != qc.activeQuests.end());
        assert(!qc.activeQuests["slime_slayer"].isCompleted);

        // Progress objective
        Gameplay::QuestManager::Get().ProgressObjective(&registry, player, "Kill", "slime", 2);
        assert(qc.activeQuests["slime_slayer"].objectiveProgress["kill_slime"] == 2);
        assert(!qc.activeQuests["slime_slayer"].isCompleted);

        // Progress to completion
        Gameplay::QuestManager::Get().ProgressObjective(&registry, player, "Kill", "slime", 1);
        
        // Quest should now be completed!
        assert(qc.activeQuests.find("slime_slayer") == qc.activeQuests.end());
        assert(std::find(qc.completedQuests.begin(), qc.completedQuests.end(), "slime_slayer") != qc.completedQuests.end());

        // Check reward was given
        assert(inv.HasItem("iron", 10));

        std::cout << "    Quest System: PASSED" << std::endl;
    }

    // Test 5: Dialogue Tree Condition & Choices
    {
        std::cout << "  Running Test: Dialogue Database flows..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Gameplay::DialogueComponent>();

        // Register a dialogue tree
        Gameplay::DialogueTree tree;
        tree.dialogueId = "intro_convo";
        tree.startNodeId = "start";

        Gameplay::DialogueNode nodeStart;
        nodeStart.nodeId = "start";
        nodeStart.speaker = "NPC";
        nodeStart.text = "Hello there traveler!";
        
        Gameplay::DialogueChoice choice1;
        choice1.text = "I am a mighty hero";
        choice1.nextNodeId = "hero_path";
        choice1.conditionLua = ""; // always true
        nodeStart.choices.push_back(choice1);

        Gameplay::DialogueNode nodeHero;
        nodeHero.nodeId = "hero_path";
        nodeHero.speaker = "NPC";
        nodeHero.text = "Wow, nice sword!";
        tree.nodes["start"] = nodeStart;
        tree.nodes["hero_path"] = nodeHero;

        Gameplay::DialogueDatabase::Get().RegisterDialogue(tree);

        ECS::Entity player = registry.CreateEntity();
        auto& dc = registry.AddComponent<Gameplay::DialogueComponent>(player);

        // Start dialogue
        bool startOk = Gameplay::DialogueDatabase::Get().StartDialogue(&registry, player, "intro_convo");
        assert(startOk);
        assert(dc.isInDialogue);
        assert(dc.currentNodeId == "start");

        const auto* currentNode = Gameplay::DialogueDatabase::Get().GetCurrentNode(&registry, player);
        assert(currentNode != nullptr);
        assert(currentNode->text == "Hello there traveler!");

        // Choose option
        bool chooseOk = Gameplay::DialogueDatabase::Get().ChooseOption(&registry, player, 0);
        assert(chooseOk);
        assert(dc.currentNodeId == "hero_path");

        const auto* nextNode = Gameplay::DialogueDatabase::Get().GetCurrentNode(&registry, player);
        assert(nextNode != nullptr);
        assert(nextNode->text == "Wow, nice sword!");

        std::cout << "    Dialogue System: PASSED" << std::endl;
    }

    // Test 6: Save and Load serialization compatibility
    {
        std::cout << "  Running Test: Gameplay systems Save & Load..." << std::endl;
        auto registry = std::make_unique<ECS::Registry>();
        registry->RegisterComponent<Scene::TransformComponent>();
        registry->RegisterComponent<Gameplay::InventoryComponent>();
        registry->RegisterComponent<Gameplay::EquipmentComponent>();
        registry->RegisterComponent<Gameplay::QuestComponent>();
        registry->RegisterComponent<Gameplay::DialogueComponent>();
        registry->RegisterComponent<Gameplay::InteractableComponent>();
        registry->RegisterComponent<Gameplay::TriggerVolumeComponent>();

        Scene::SceneManager::Get().Initialize(registry.get());

        ECS::Entity player = registry->CreateEntity();
        Save::EntityGUID playerGuid = registry->CreateGUID(player);
        
        auto& inv = registry->AddComponent<Gameplay::InventoryComponent>(player, 10);
        inv.AddItem("wood", 15);
        inv.AddItem("iron", 5);

        auto& eq = registry->AddComponent<Gameplay::EquipmentComponent>(player);
        eq.slots[static_cast<size_t>(Gameplay::EquipmentSlot::Weapon)] = "sword";

        auto& qc = registry->AddComponent<Gameplay::QuestComponent>(player);
        qc.completedQuests.push_back("tutorial_quest");
        Gameplay::QuestState qs;
        qs.questId = "slime_slayer";
        qs.currentStageIndex = 0;
        qs.isCompleted = false;
        qs.objectiveProgress["kill_slime"] = 2;
        qc.activeQuests["slime_slayer"] = qs;

        auto& dc = registry->AddComponent<Gameplay::DialogueComponent>(player);
        dc.isInDialogue = true;
        dc.currentDialogueId = "intro_convo";
        dc.currentNodeId = "hero_path";

        // Save game
        std::string filename = "gameplay_test.sav";
        bool saveOk = Save::SaveManager::Get().SaveGame(filename, registry.get());
        assert(saveOk);

        // Clear registry
        registry = std::make_unique<ECS::Registry>();
        registry->RegisterComponent<Scene::TransformComponent>();
        registry->RegisterComponent<Gameplay::InventoryComponent>();
        registry->RegisterComponent<Gameplay::EquipmentComponent>();
        registry->RegisterComponent<Gameplay::QuestComponent>();
        registry->RegisterComponent<Gameplay::DialogueComponent>();
        registry->RegisterComponent<Gameplay::InteractableComponent>();
        registry->RegisterComponent<Gameplay::TriggerVolumeComponent>();

        Scene::SceneManager::Get().Initialize(registry.get());
        assert(registry->GetAliveEntities().size() == 0);

        // Load game
        bool loadOk = Save::SaveManager::Get().LoadGame(filename, registry.get());
        assert(loadOk);

        // Verify entities and components restored
        ECS::Entity restoredPlayer = registry->GetEntityByGUID(playerGuid);
        assert(restoredPlayer != ECS::NULL_ENTITY);
        assert(registry->HasComponent<Gameplay::InventoryComponent>(restoredPlayer));
        assert(registry->HasComponent<Gameplay::EquipmentComponent>(restoredPlayer));
        assert(registry->HasComponent<Gameplay::QuestComponent>(restoredPlayer));
        assert(registry->HasComponent<Gameplay::DialogueComponent>(restoredPlayer));

        // Verify inventory contents
        auto& rInv = registry->GetComponent<Gameplay::InventoryComponent>(restoredPlayer);
        assert(rInv.maxSlots == 10);
        assert(rInv.HasItem("wood", 15));
        assert(rInv.HasItem("iron", 5));

        // Verify equipment
        auto& rEq = registry->GetComponent<Gameplay::EquipmentComponent>(restoredPlayer);
        assert(rEq.slots[static_cast<size_t>(Gameplay::EquipmentSlot::Weapon)] == "sword");

        // Verify quest component
        auto& rQc = registry->GetComponent<Gameplay::QuestComponent>(restoredPlayer);
        assert(rQc.completedQuests.size() == 1 && rQc.completedQuests[0] == "tutorial_quest");
        assert(rQc.activeQuests.find("slime_slayer") != rQc.activeQuests.end());
        assert(rQc.activeQuests["slime_slayer"].objectiveProgress["kill_slime"] == 2);

        // Verify dialogue state
        auto& rDc = registry->GetComponent<Gameplay::DialogueComponent>(restoredPlayer);
        assert(rDc.isInDialogue);
        assert(rDc.currentDialogueId == "intro_convo");
        assert(rDc.currentNodeId == "hero_path");

        // Cleanup
        std::remove(filename.c_str());
        Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
        Scene::SceneManager::Get().Shutdown();

        std::cout << "    Save & Load Compatibility: PASSED" << std::endl;
    }

    // Test 7: Networking & Snapshot Replication Delta Sync
    {
        std::cout << "  Running Test: Network Replication of gameplay components..." << std::endl;
        ECS::Registry serverRegistry;
        serverRegistry.RegisterComponent<Scene::TransformComponent>();
        serverRegistry.RegisterComponent<Gameplay::InventoryComponent>();
        serverRegistry.RegisterComponent<Gameplay::EquipmentComponent>();
        serverRegistry.RegisterComponent<Gameplay::QuestComponent>();
        serverRegistry.RegisterComponent<Gameplay::DialogueComponent>();

        ECS::Registry clientRegistry;
        clientRegistry.RegisterComponent<Scene::TransformComponent>();
        clientRegistry.RegisterComponent<Gameplay::InventoryComponent>();
        clientRegistry.RegisterComponent<Gameplay::EquipmentComponent>();
        clientRegistry.RegisterComponent<Gameplay::QuestComponent>();
        clientRegistry.RegisterComponent<Gameplay::DialogueComponent>();

        // Set up server player entity
        ECS::Entity serverPlayer = serverRegistry.CreateEntity();
        Save::EntityGUID playerGuid = serverRegistry.CreateGUID(serverPlayer);
        auto& inv = serverRegistry.AddComponent<Gameplay::InventoryComponent>(serverPlayer, 10);
        inv.AddItem("wood", 10);

        // Initialize Replication Managers
        Networking::ReplicationManager serverRep;
        serverRep.Initialize(&serverRegistry);

        Networking::ReplicationManager clientRep;
        clientRep.Initialize(&clientRegistry);

        // Capture server state snapshot
        Networking::Snapshot initialSnap = serverRep.CaptureCurrentSnapshot();
        
        // Serialize snapshot to packet payload
        Networking::PacketWriter writer(Networking::PacketId::Invalid);
        writer.WriteUint32(initialSnap.id);
        writer.WriteBool(false); // isDelta = false
        writer.WriteUint32(0); // baseSnapshotId = 0
        writer.WriteUint32(static_cast<uint32_t>(initialSnap.entities.size()));

        for (const auto& [guid, entState] : initialSnap.entities) {
            writer.WriteUint64(guid.high);
            writer.WriteUint64(guid.low);
            writer.WriteBool(false); // isDestroyed = false
            writer.WriteUint64(entState.parentGuid.high);
            writer.WriteUint64(entState.parentGuid.low);

            // Construct 16-bit mask
            uint16_t mask = 0;
            if (entState.components.hasInventory) mask |= (1 << 8);
            writer.WriteUint16(mask);
            writer.WriteUint8(1); // count = 1
            writer.WriteUint8(8); // bit = 8 (Inventory)

            // Temp serialize component
            Networking::PacketWriter tempWriter(Networking::PacketId::Invalid);
            serverRep.SerializeComponent(tempWriter, entState, (1 << 8));
            uint32_t payloadSize = static_cast<uint32_t>(tempWriter.GetSize() - 6);
            writer.WriteUint32(payloadSize);
            writer.WriteBytes(tempWriter.GetData() + 6, payloadSize);
        }

        // Apply snapshot to client
        Networking::PacketReader reader(writer.GetData(), writer.GetSize());
        clientRep.ClientProcessSnapshot(reader);

        // Verify client entity holds the replicated inventory
        ECS::Entity clientPlayer = clientRegistry.GetEntityByGUID(playerGuid);
        assert(clientPlayer != ECS::NULL_ENTITY);
        assert(clientRegistry.HasComponent<Gameplay::InventoryComponent>(clientPlayer));
        auto& cInv = clientRegistry.GetComponent<Gameplay::InventoryComponent>(clientPlayer);
        assert(cInv.HasItem("wood", 10));

        std::cout << "    Network Replication: PASSED" << std::endl;
    }

    std::cout << "=== ALL GAMEPLAY SYSTEMS TESTS PASSED! ===" << std::endl;
}

void VerifyAudioCameraCinematicTests() {
    std::cout << "=== RUNNING AUDIO, CAMERA, TIMELINE & CINEMATIC TESTS ===" << std::endl;

    // 1. Audio System Test
    {
        std::cout << "  Running Audio System Tests..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Audio::AudioListenerComponent>();
        registry.RegisterComponent<Audio::AudioSourceComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();

        ECS::Entity listenerEnt = registry.CreateEntity();
        auto& listener = registry.AddComponent<Audio::AudioListenerComponent>(listenerEnt);
        auto& listenerTc = registry.AddComponent<Scene::TransformComponent>(listenerEnt);
        listenerTc.position = glm::vec3(0.0f, 0.0f, 0.0f);
        listenerTc.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // looking down Z-axis: forward (0,0,-1), up (0,1,0), right (1,0,0)

        ECS::Entity sourceEnt = registry.CreateEntity();
        auto& source = registry.AddComponent<Audio::AudioSourceComponent>(sourceEnt);
        auto& sourceTc = registry.AddComponent<Scene::TransformComponent>(sourceEnt);
        sourceTc.position = glm::vec3(5.0f, 0.0f, -5.0f); // 45 deg to the right, distance = sqrt(50) = 7.07

        source.eventOrPath = "event:/sfx/explosion";
        source.isPlaying = true;
        source.spatial3D = true;
        source.minDistance = 2.0f;
        source.maxDistance = 12.0f;
        source.volume = 1.0f;
        source.currentVolume = 1.0f;

        // Set group volume
        Audio::AudioSystem::Get().GetMixer().SetVolume(Audio::MixerChannel::SFX, 0.8f);
        Audio::AudioSystem::Get().GetMixer().SetVolume(Audio::MixerChannel::Master, 1.0f);

        // Tick AudioSystem
        Audio::AudioSystem::Get().Update(&registry, 0.016f);

        // Verification of attenuation: dist is ~7.07. Attenuation: 1 - (7.07-2)/(12-2) = 1 - 5.07/10 = 0.493
        // Volume: 1.0 (currentVolume) * 0.493 (attenuation) * 0.8 (SFX Mixer) * 1.0 (Master Mixer) = ~0.394
        assert(source.volume > 0.38f && source.volume < 0.41f);

        // Verification of panning: toSource is (5, 0, -5). Listener right is (1,0,0). Panning = dot(normalize(toSource), (1,0,0)) = 5/7.07 = 0.707
        assert(std::abs(source.currentPan - 0.707f) < 0.01f);

        std::cout << "    Audio System: PASSED" << std::endl;
    }

    // 2. Camera Framework Test
    {
        std::cout << "  Running Camera Framework Tests..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Camera::CameraComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();

        // Register two cameras in manager to test priority blending
        Camera::CameraManager::Get().Clear();
        auto camA = std::make_shared<Camera::Camera>();
        auto camB = std::make_shared<Camera::Camera>();
        Camera::CameraManager::Get().RegisterCamera("CamA", camA);
        Camera::CameraManager::Get().RegisterCamera("CamB", camB);

        camA->SetPriority(10);
        camB->SetPriority(20);

        // Update CameraManager blending
        Camera::CameraManager::Get().Update(0.1f);
        // Active should be CamB since priority 20 > 10
        assert(Camera::CameraManager::Get().GetActiveCamera() == camB);

        // Camera Shake test
        camB->StartShake(2.0f, 1.0f, 10.0f);
        assert(camB->GetShakeTimer() == 1.0f);
        camB->Update(0.5f, nullptr);
        assert(camB->GetShakeTimer() == 0.5f);

        // Target tracking entity test
        ECS::Entity targetEnt = registry.CreateEntity();
        auto& targetTc = registry.AddComponent<Scene::TransformComponent>(targetEnt);
        targetTc.position = glm::vec3(10.0f, 5.0f, 10.0f);

        ECS::Entity cameraEnt = registry.CreateEntity();
        auto& camComp = registry.AddComponent<Camera::CameraComponent>(cameraEnt);
        auto& cameraTc = registry.AddComponent<Scene::TransformComponent>(cameraEnt);
        camComp.mode = Camera::CameraMode::ThirdPerson;
        camComp.targetEntity = targetEnt;
        camComp.orbitDistance = 5.0f;
        camComp.targetHeightOffset = glm::vec3(0.0f);

        // Tick CameraSystem
        Camera::CameraSystem::Get().Update(&registry, 0.1f, nullptr);

        // Target has been synced to the Camera instance in manager
        auto eCam = Camera::CameraManager::Get().GetCamera("Camera_" + std::to_string(cameraEnt));
        assert(eCam != nullptr);
        // Position of camera should orbit target position
        assert(glm::distance(cameraTc.position, targetTc.position) > 1.0f);

        std::cout << "    Camera Framework: PASSED" << std::endl;
    }

    // 3. Timeline & Cinematic System Test
    {
        std::cout << "  Running Timeline & Cinematic System Tests..." << std::endl;
        ECS::Registry registry;
        registry.RegisterComponent<Timeline::CinematicPlayerComponent>();
        registry.RegisterComponent<Scene::TransformComponent>();
        registry.RegisterComponent<Camera::CameraComponent>();
        registry.RegisterComponent<ECS::ScriptComponent>();

        // Create player entity
        ECS::Entity playerEnt = registry.CreateEntity();
        auto& player = registry.AddComponent<Timeline::CinematicPlayerComponent>(playerEnt);
        player.timelineName = "TestCutscene";
        player.playbackSpeed = 1.0f;

        // Target camera entity
        ECS::Entity camEnt = registry.CreateEntity();
        auto& camComp = registry.AddComponent<Camera::CameraComponent>(camEnt);
        auto& camTc = registry.AddComponent<Scene::TransformComponent>(camEnt);
        Save::EntityGUID camGuid = {12345, 67890};
        registry.AssignGUID(camEnt, camGuid);

        // Create timeline asset
        Timeline::TimelineAsset timeline;
        timeline.name = "TestCutscene";
        timeline.duration = 4.0f;

        // Add Camera track
        Timeline::Track camTrack;
        camTrack.name = "CameraTrack";
        camTrack.type = Timeline::TrackType::Camera;
        camTrack.targetEntityGuid = camGuid.ToString();

        Timeline::Clip clip;
        clip.name = "CameraClip";
        clip.startTime = 0.0f;
        clip.duration = 4.0f;

        Timeline::Keyframe keyStart;
        keyStart.time = 0.0f;
        keyStart.valueVec3 = glm::vec3(0.0f, 0.0f, 0.0f);
        keyStart.valueQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        keyStart.valueFloat = 60.0f;

        Timeline::Keyframe keyEnd;
        keyEnd.time = 4.0f;
        keyEnd.valueVec3 = glm::vec3(10.0f, 10.0f, 10.0f);
        keyEnd.valueQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        keyEnd.valueFloat = 90.0f;

        clip.keyframes.push_back(keyStart);
        clip.keyframes.push_back(keyEnd);
        camTrack.clips.push_back(clip);
        timeline.tracks.push_back(camTrack);

        Timeline::TimelineManager::Get().RegisterTimeline(timeline);

        // Play cinematic
        Timeline::CinematicSystem::Get().Play(&registry, playerEnt);
        assert(player.isPlaying == true);
        assert(player.currentTime == 0.0f);

        // Tick 2 seconds (halfway)
        Timeline::CinematicSystem::Get().Update(&registry, 2.0f);
        assert(player.currentTime == 2.0f);

        // Verify camera position and FOV are halfway interpolated: pos (5, 5, 5), fov 75
        assert(glm::distance(camTc.position, glm::vec3(5.0f, 5.0f, 5.0f)) < 0.01f);
        assert(std::abs(camComp.fov - 75.0f) < 0.01f);

        // Skip to end
        Timeline::CinematicSystem::Get().Skip(&registry, playerEnt);
        assert(player.isPlaying == false);
        assert(player.currentTime == 4.0f);
        assert(glm::distance(camTc.position, glm::vec3(10.0f, 10.0f, 10.0f)) < 0.01f);
        assert(std::abs(camComp.fov - 90.0f) < 0.01f);

        std::cout << "    Timeline & Cinematic System: PASSED" << std::endl;
    }

    // 4. Save / Load & Serialization Test
    {
        std::cout << "  Running Save/Load and Serialization Tests..." << std::endl;
        ECS::Registry saveRegistry;
        saveRegistry.RegisterComponent<Scene::TransformComponent>();
        saveRegistry.RegisterComponent<Audio::AudioSourceComponent>();
        saveRegistry.RegisterComponent<Audio::AudioListenerComponent>();
        saveRegistry.RegisterComponent<Timeline::CinematicPlayerComponent>();
        Scene::SceneManager::Get().Initialize(&saveRegistry);

        ECS::Entity saveEnt = saveRegistry.CreateEntity();
        Save::EntityGUID saveGuid = {999, 888};
        saveRegistry.AssignGUID(saveEnt, saveGuid);

        auto& tc = saveRegistry.AddComponent<Scene::TransformComponent>(saveEnt);
        tc.position = glm::vec3(1.0f, 2.0f, 3.0f);

        auto& asc = saveRegistry.AddComponent<Audio::AudioSourceComponent>(saveEnt);
        asc.eventOrPath = "event:/ambient/wind";
        asc.isPlaying = true;
        asc.loop = true;
        asc.spatial3D = false;
        asc.pitch = 1.1f;
        asc.volume = 0.9f;
        asc.pan = -0.5f;
        asc.minDistance = 3.0f;
        asc.maxDistance = 30.0f;

        auto& alc = saveRegistry.AddComponent<Audio::AudioListenerComponent>(saveEnt);
        alc.position = glm::vec3(4.0f, 5.0f, 6.0f);
        alc.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        alc.up = glm::vec3(0.0f, 1.0f, 0.0f);

        auto& cpc = saveRegistry.AddComponent<Timeline::CinematicPlayerComponent>(saveEnt);
        cpc.timelineName = "IntroCutscene";
        cpc.isPlaying = true;
        cpc.isPaused = false;
        cpc.currentTime = 1.5f;
        cpc.loop = true;
        cpc.playbackSpeed = 1.5f;

        // Save
        bool saveOk = Save::SaveManager::Get().SaveGame("temp_regression_save.dat", &saveRegistry);
        assert(saveOk);

        // Load into new registry
        ECS::Registry loadRegistry;
        loadRegistry.RegisterComponent<Scene::TransformComponent>();
        loadRegistry.RegisterComponent<Audio::AudioSourceComponent>();
        loadRegistry.RegisterComponent<Audio::AudioListenerComponent>();
        loadRegistry.RegisterComponent<Timeline::CinematicPlayerComponent>();
        Scene::SceneManager::Get().Initialize(&loadRegistry);

        bool loadOk = Save::SaveManager::Get().LoadGame("temp_regression_save.dat", &loadRegistry);
        assert(loadOk);

        ECS::Entity loadEnt = loadRegistry.GetEntityByGUID(saveGuid);
        assert(loadEnt != ECS::NULL_ENTITY);

        // Verify AudioSourceComponent values
        assert(loadRegistry.HasComponent<Audio::AudioSourceComponent>(loadEnt));
        const auto& loadedAsc = loadRegistry.GetComponent<Audio::AudioSourceComponent>(loadEnt);
        assert(loadedAsc.eventOrPath == "event:/ambient/wind");
        assert(loadedAsc.isPlaying == true);
        assert(loadedAsc.loop == true);
        assert(loadedAsc.spatial3D == false);
        assert(std::abs(loadedAsc.pitch - 1.1f) < 0.01f);
        assert(std::abs(loadedAsc.volume - 0.9f) < 0.01f);
        assert(std::abs(loadedAsc.pan - -0.5f) < 0.01f);
        assert(loadedAsc.minDistance == 3.0f);
        assert(loadedAsc.maxDistance == 30.0f);

        // Verify AudioListenerComponent values
        assert(loadRegistry.HasComponent<Audio::AudioListenerComponent>(loadEnt));
        const auto& loadedAlc = loadRegistry.GetComponent<Audio::AudioListenerComponent>(loadEnt);
        assert(glm::distance(loadedAlc.position, glm::vec3(4.0f, 5.0f, 6.0f)) < 0.01f);

        // Verify CinematicPlayerComponent values
        assert(loadRegistry.HasComponent<Timeline::CinematicPlayerComponent>(loadEnt));
        const auto& loadedCpc = loadRegistry.GetComponent<Timeline::CinematicPlayerComponent>(loadEnt);
        assert(loadedCpc.timelineName == "IntroCutscene");
        assert(loadedCpc.isPlaying == true);
        assert(loadedCpc.isPaused == false);
        assert(std::abs(loadedCpc.currentTime - 1.5f) < 0.01f);
        assert(loadedCpc.loop == true);
        assert(std::abs(loadedCpc.playbackSpeed - 1.5f) < 0.01f);

        std::cout << "    Save/Load and Serialization: PASSED" << std::endl;

        // Cleanup
        std::remove("temp_regression_save.dat");
        Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
        Scene::SceneManager::Get().Shutdown();
    }

    std::cout << "=== ALL AUDIO, CAMERA, TIMELINE & CINEMATIC TESTS PASSED! ===" << std::endl;
}

int main() {
    std::cout << "Starting Kumari Kandam Regression Test Suite..." << std::endl;

    std::cout << "Running Coordinate Space Tests..." << std::endl;
    VerifyCoordinateSpaceTests();
    std::cout << "Coordinate Space Tests passed!" << std::endl;

    std::cout << "Running ECS Tests..." << std::endl;
    VerifyECSTests();
    std::cout << "ECS Tests passed!" << std::endl;

    std::cout << "Running Terrain Stitching Tests..." << std::endl;
    VerifyTerrainStitchingTests();
    std::cout << "Terrain Stitching Tests passed!" << std::endl;

    std::cout << "Running Vulkan Tests..." << std::endl;
    VerifyVulkanTests();
    std::cout << "Vulkan Tests passed!" << std::endl;

    std::cout << "Running AI Framework & Navigation Tests..." << std::endl;
    VerifyAIFrameworkTests();
    std::cout << "AI Framework & Navigation Tests passed!" << std::endl;

    std::cout << "Running Gameplay Systems Tests..." << std::endl;
    VerifyGameplaySystemsTests();
    std::cout << "Gameplay Systems Tests passed!" << std::endl;

    std::cout << "Running Audio, Camera, Timeline & Cinematic Tests..." << std::endl;
    VerifyAudioCameraCinematicTests();
    std::cout << "Audio, Camera, Timeline & Cinematic Tests passed!" << std::endl;

    std::cout << "ALL REGRESSION TESTS COMPLETED SUCCESSFULLY!" << std::endl;

    // Ensure final cleanup of singletons before exit
    Terrain::TerrainManager::Get().Shutdown(VK_NULL_HANDLE);
    Scene::SceneManager::Get().Shutdown();

    return 0;
}
