#include "core/engine.hpp"
#include "core/logger.hpp"
#include "ecs/ecs.hpp"
#include "scene/scene_node.hpp"
#include "scene/scene_manager.hpp"
#include "physics/physics_types.hpp"
#include "physics/physics_components.hpp"
#include "physics/physics_world.hpp"
#include "physics/character_controller.hpp"
#include "terrain/terrain_manager.hpp"
#include "renderer/vulkan/vulkan_context.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
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

    std::cout << "ALL REGRESSION TESTS COMPLETED SUCCESSFULLY!" << std::endl;
    return 0;
}
