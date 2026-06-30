#include "core/engine.hpp"
#include "core/logger.hpp"
#include "ecs/ecs.hpp"
#include "resource/resource_manager.hpp"
#include "asset_pipeline/asset_manager.hpp"
#include "scene/scene_node.hpp"
#include "scene/scene_manager.hpp"
#include "camera/camera.hpp"
#include "camera/camera_manager.hpp"
#include "physics/physics_types.hpp"
#include "physics/physics_components.hpp"
#include "physics/physics_world.hpp"
#include "physics/spatial_hash_grid.hpp"
#include "physics/raycast.hpp"
#include "physics/character_controller.hpp"
#include "physics/physics_system.hpp"
#include "terrain/terrain_manager.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cassert>
#include <cmath>

struct Position {
    float x, y;
};

struct Velocity {
    float dx, dy;
};

struct MockAsset : public KumariEngine::Resource::Resource {
    static inline int constructorCount = 0;
    std::string data;
    MockAsset(std::string d) : data(d) {
        constructorCount++;
    }
};

void RunECSScalingTest() {
    KumariEngine::Core::Logger::Info("ECS_Perf", "Starting ECS scaling test for 100,000 entities...");

    KumariEngine::ECS::Registry registry;
    
    // Test pre-allocation / Reserve
    registry.RegisterComponent<Position>();
    registry.RegisterComponent<Velocity>();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    registry.Reserve<Position>(100000);
    registry.Reserve<Velocity>(100000);
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> reserveDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("ECS_Perf", "Pre-allocated pools for 100,000 components in %.3f ms", reserveDuration.count());

    // Create 100,000 entities
    startTime = std::chrono::high_resolution_clock::now();
    KumariEngine::Core::Logger::Info("ECS_Perf", "Before Entity Creation Loop");
    for (int i = 0; i < 100000; ++i) {
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Creating entity index %d...", i);
        }
        auto entity = registry.CreateEntity();
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Created entity index %d with ID %u", i, entity);
        }
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Assigning Position to entity %u...", entity);
        }
        registry.AddComponent<Position>(entity, Position{ static_cast<float>(i), static_cast<float>(i) });
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Assigned Position to entity %u", entity);
        }
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Assigning Velocity to entity %u...", entity);
        }
        registry.AddComponent<Velocity>(entity, Velocity{ 1.0f, 1.0f });
        if (i < 5 || i >= 99995) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Assigned Velocity to entity %u", entity);
        }
    }
    KumariEngine::Core::Logger::Info("ECS_Perf", "After Entity Creation Loop");
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> creationDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("ECS_Perf", "Created 100,000 entities with components in %.3f ms", creationDuration.count());

    // Run Each system loop for 60 frames (simulating 60fps)
    double totalTimeMs = 0.0;
    KumariEngine::Core::Logger::Info("ECS_Perf", "Before Each() loops");
    for (int frame = 0; frame < 60; ++frame) {
        auto loopStart = std::chrono::high_resolution_clock::now();
        if (frame == 0 || frame == 59) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Before Registry::Each for frame %d", frame);
        }
        registry.Each<Position, Velocity>([](auto entity, Position& pos, Velocity& vel) {
            (void)entity;
            pos.x += vel.dx * 0.016f;
            pos.y += vel.dy * 0.016f;
        });
        if (frame == 0 || frame == 59) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "After Registry::Each for frame %d", frame);
        }

        auto loopEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> frameDuration = loopEnd - loopStart;
        totalTimeMs += frameDuration.count();
    }
    KumariEngine::Core::Logger::Info("ECS_Perf", "After Each() loops");
    
    double avgTimeMs = totalTimeMs / 60.0;
    KumariEngine::Core::Logger::Info("ECS_Perf", "Average ECS Each loop time (100,000 entities): %.3f ms (Target: < 16.6ms for 60fps)", avgTimeMs);

    // Destroy 100,000 entities
    startTime = std::chrono::high_resolution_clock::now();
    KumariEngine::Core::Logger::Info("ECS_Perf", "Before Entity Destruction Loop");
    for (uint32_t i = 1; i <= 100000; ++i) {
        if (i <= 5 || i >= 99996) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Destroying entity %u...", i);
        }
        registry.DestroyEntity(i);
        if (i <= 5 || i >= 99996) {
            KumariEngine::Core::Logger::Info("ECS_Perf", "Destroyed entity %u", i);
        }
    }
    KumariEngine::Core::Logger::Info("ECS_Perf", "After Entity Destruction Loop");
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> destroyDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("ECS_Perf", "Destroyed 100,000 entities in %.3f ms", destroyDuration.count());
}

void RunResourceAssetTest() {
    KumariEngine::Core::Logger::Info("Resource_Test", "Starting Resource & Asset Manager verification test...");

    auto& resMgr = KumariEngine::Resource::ResourceManager::Get();
    auto& assetMgr = KumariEngine::Asset::AssetManager::Get();

    // 1. Verify synchronous loading and caching
    auto asset1 = resMgr.Load<MockAsset>("test_asset_1", []() {
        return std::make_shared<MockAsset>("Data1");
    });
    assert(asset1 != nullptr);
    assert(asset1->data == "Data1");
    assert(MockAsset::constructorCount == 1);

    // Retrieve again from cache, count shouldn't increase
    auto asset1_cached = resMgr.Get<MockAsset>("test_asset_1");
    assert(asset1_cached == asset1);
    assert(MockAsset::constructorCount == 1);
    KumariEngine::Core::Logger::Info("Resource_Test", "Synchronous loading and caching: PASSED");

    // 2. Verify asynchronous loading and caching
    auto futureAsset = assetMgr.LoadAsync<MockAsset>("test_asset_async", []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulating background read
        return std::make_shared<MockAsset>("AsyncData");
    });

    KumariEngine::Core::Logger::Info("Resource_Test", "Async load triggered. Doing other work...");
    
    // Wait for the asset to finish loading
    std::shared_ptr<MockAsset> asyncAsset = futureAsset.get();
    assert(asyncAsset != nullptr);
    assert(asyncAsset->data == "AsyncData");
    
    // Verify it is cached
    auto asyncAsset_cached = resMgr.Get<MockAsset>("test_asset_async");
    assert(asyncAsset_cached == asyncAsset);
    KumariEngine::Core::Logger::Info("Resource_Test", "Asynchronous loading and caching: PASSED");

    // 3. Verify thread-safety
    constexpr int threadCount = 10;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<MockAsset>> loadedAssets(threadCount);
    
    int initialCount = MockAsset::constructorCount;
    (void)initialCount;
    
    for (int i = 0; i < threadCount; ++i) {
        threads.push_back(std::thread([i, &loadedAssets]() {
            auto& rMgr = KumariEngine::Resource::ResourceManager::Get();
            loadedAssets[i] = rMgr.Load<MockAsset>("concurrent_asset", []() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return std::make_shared<MockAsset>("ConcurrentData");
            });
        }));
    }
    
    for (auto& t : threads) {
        t.join();
    }

    // Verify all threads got the same instance and constructor was called exactly once
    for (int i = 0; i < threadCount; ++i) {
        assert(loadedAssets[i] == loadedAssets[0]);
    }
    assert(MockAsset::constructorCount == initialCount + 1);
    KumariEngine::Core::Logger::Info("Resource_Test", "Thread-safety concurrent loading: PASSED");

    // Clean up
    resMgr.Clear();
    resMgr.UnloadUnused();
    KumariEngine::Core::Logger::Info("Resource_Test", "All Resource & Asset Manager tests completed successfully.");
}

void RunSceneGraphHierarchyTest() {
    KumariEngine::Core::Logger::Info("SceneGraph_Test", "Starting hierarchical Scene Graph transforms test...");

    // Create scene nodes
    auto parent = std::make_unique<KumariEngine::Scene::SceneNode>("Parent");
    auto child = std::make_unique<KumariEngine::Scene::SceneNode>("Child");
    auto grandchild = std::make_unique<KumariEngine::Scene::SceneNode>("Grandchild");

    // Local positions
    parent->SetLocalPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    child->SetLocalPosition(glm::vec3(0.0f, 20.0f, 0.0f));
    grandchild->SetLocalPosition(glm::vec3(0.0f, 0.0f, 30.0f));

    // Build hierarchy
    KumariEngine::Scene::SceneNode* childRaw = child.get();
    KumariEngine::Scene::SceneNode* grandchildRaw = grandchild.get();
    
    parent->AddChild(std::move(child));
    childRaw->AddChild(std::move(grandchild));

    // Update transforms
    parent->UpdateTransforms();

    // Verify initial world positions (matrix translation column)
    glm::vec3 parentWorldPos = glm::vec3(parent->GetWorldMatrix()[3]);
    glm::vec3 childWorldPos = glm::vec3(childRaw->GetWorldMatrix()[3]);
    glm::vec3 grandchildWorldPos = glm::vec3(grandchildRaw->GetWorldMatrix()[3]);

    assert(glm::distance(parentWorldPos, glm::vec3(10.0f, 0.0f, 0.0f)) < 0.001f);
    assert(glm::distance(childWorldPos, glm::vec3(10.0f, 20.0f, 0.0f)) < 0.001f);
    assert(glm::distance(grandchildWorldPos, glm::vec3(10.0f, 20.0f, 30.0f)) < 0.001f);
    
    KumariEngine::Core::Logger::Info("SceneGraph_Test", "World positions verified successfully.");

    // Test reparenting
    auto parent2 = std::make_unique<KumariEngine::Scene::SceneNode>("Parent2");
    parent2->SetLocalPosition(glm::vec3(100.0f, 100.0f, 100.0f));

    // Remove child from parent (retrieving unique_ptr)
    auto childPtr = parent->RemoveChild(childRaw);
    assert(childPtr != nullptr);
    assert(childRaw->GetParent() == nullptr);

    // Attach to parent2
    parent2->AddChild(std::move(childPtr));
    assert(childRaw->GetParent() == parent2.get());

    // Update transforms
    parent->UpdateTransforms();
    parent2->UpdateTransforms();

    // Verify updated world positions
    glm::vec3 parent2WorldPos = glm::vec3(parent2->GetWorldMatrix()[3]);
    childWorldPos = glm::vec3(childRaw->GetWorldMatrix()[3]);
    grandchildWorldPos = glm::vec3(grandchildRaw->GetWorldMatrix()[3]);

    assert(glm::distance(parent2WorldPos, glm::vec3(100.0f, 100.0f, 100.0f)) < 0.001f);
    assert(glm::distance(childWorldPos, glm::vec3(100.0f, 120.0f, 100.0f)) < 0.001f);
    assert(glm::distance(grandchildWorldPos, glm::vec3(100.0f, 120.0f, 130.0f)) < 0.001f);

    KumariEngine::Core::Logger::Info("SceneGraph_Test", "Reparenting world positions verified successfully.");
    KumariEngine::Core::Logger::Info("SceneGraph_Test", "Scene Graph transforms test: PASSED");
}

void RunWorldStreamingTest() {
    KumariEngine::Core::Logger::Info("Streaming_Test", "Starting world streaming lifecycle verification test...");

    KumariEngine::ECS::Registry registry;
    auto& sceneMgr = KumariEngine::Scene::SceneManager::Get();

    sceneMgr.Initialize(&registry);
    sceneMgr.SetChunkSize(100.0f);
    sceneMgr.SetLoadRadius(1);    // Load 3x3 grid around viewer (dist <= 1)
    sceneMgr.SetUnloadRadius(2);  // Unload when dist > 2

    // 1. Viewer at (0, 0)
    sceneMgr.SetViewerPosition(glm::vec3(0.0f));
    
    // First update triggers load for 3x3 = 9 chunks
    sceneMgr.Update(0.016f);
    assert(sceneMgr.GetLoadingChunkCount() == 9);
    assert(sceneMgr.GetLoadedChunkCount() == 0);

    KumariEngine::Core::Logger::Info("Streaming_Test", "Waiting for chunks around (0,0) to finish async loading...");
    
    // Spin until they load (simulate main loop ticks)
    auto startTime = std::chrono::high_resolution_clock::now();
    while (sceneMgr.GetLoadedChunkCount() < 9) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        sceneMgr.Update(0.016f);

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - startTime;
        if (elapsed.count() > 5.0) {
            std::cerr << "Timeout waiting for chunks to load" << std::endl;
            assert(false && "Chunk loading timed out");
        }
    }

    assert(sceneMgr.GetLoadingChunkCount() == 0);
    assert(sceneMgr.GetLoadedChunkCount() == 9);
    assert(sceneMgr.IsChunkLoaded(0, 0));
    assert(sceneMgr.IsChunkLoaded(1, -1));

    KumariEngine::Core::Logger::Info("Streaming_Test", "Chunks loaded. Verifying scene nodes & ECS entities...");
    
    auto rootNode = sceneMgr.GetRootNode();
    assert(rootNode != nullptr);
    assert(rootNode->GetChildren().size() == 9); // 9 chunk nodes

    // Move the viewer to (400, 0, 400), which is at chunk (4, 4)
    // Distance from (4, 4) to any of the old chunks: max(|4 - 1|, |4 - 1|) = 3 chunks.
    // 3 > m_unloadRadius (2), so old 9 chunks must be unloaded.
    sceneMgr.SetViewerPosition(glm::vec3(400.0f, 0.0f, 400.0f));
    
    sceneMgr.Update(0.016f);

    // Old 9 chunks should be destroyed immediately
    assert(!sceneMgr.IsChunkLoaded(0, 0));
    assert(sceneMgr.GetLoadingChunkCount() == 9); // Loading new 9 chunks around (4,4)

    KumariEngine::Core::Logger::Info("Streaming_Test", "Waiting for new chunks around (4,4) to load...");
    
    startTime = std::chrono::high_resolution_clock::now();
    while (sceneMgr.GetLoadedChunkCount() < 9) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        sceneMgr.Update(0.016f);

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - startTime;
        if (elapsed.count() > 5.0) {
            std::cerr << "Timeout waiting for new chunks to load" << std::endl;
            assert(false && "New chunk loading timed out");
        }
    }

    assert(sceneMgr.IsChunkLoaded(4, 4));
    assert(sceneMgr.GetLoadedChunkCount() == 9);
    assert(rootNode->GetChildren().size() == 9); // Only 9 active chunk nodes

    // Verify all old entities were safely destroyed and we only have the new 27 entities
    int nodeEntityCount = 0;
    for (const auto& chunkNode : rootNode->GetChildren()) {
        nodeEntityCount += static_cast<int>(chunkNode->GetChildren().size());
    }
    assert(nodeEntityCount == 27);

    sceneMgr.Shutdown();
    
    // Verify cleanup
    assert(rootNode->GetChildren().empty());
    
    // Unused resources eviction
    KumariEngine::Resource::ResourceManager::Get().Clear();
    KumariEngine::Resource::ResourceManager::Get().UnloadUnused();

    KumariEngine::Core::Logger::Info("Streaming_Test", "World streaming lifecycle verification test: PASSED");
}

void RunStreamingThreadSafetyTest() {
    KumariEngine::Core::Logger::Info("ThreadSafety_Test", "Starting thread safety & stress test under high streaming churn...");

    KumariEngine::ECS::Registry registry;
    auto& sceneMgr = KumariEngine::Scene::SceneManager::Get();

    sceneMgr.Initialize(&registry);
    sceneMgr.SetChunkSize(50.0f);
    sceneMgr.SetLoadRadius(2);
    sceneMgr.SetUnloadRadius(3);

    // Teleport viewer rapidly to stress-test async loading/unloading
    for (int i = 0; i < 20; ++i) {
        float offset = i * 200.0f;
        sceneMgr.SetViewerPosition(glm::vec3(offset, 0.0f, offset));
        sceneMgr.Update(0.016f);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1 + (i % 3)));
    }

    KumariEngine::Core::Logger::Info("ThreadSafety_Test", "Viewer settled. Waiting for final load to stabilize...");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    while (sceneMgr.GetLoadingChunkCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        sceneMgr.Update(0.016f);

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - startTime;
        if (elapsed.count() > 5.0) {
            std::cerr << "Timeout waiting for stress test to stabilize" << std::endl;
            assert(false && "Stress test stabilization timed out");
        }
    }

    sceneMgr.Shutdown();

    KumariEngine::Resource::ResourceManager::Get().Clear();
    KumariEngine::Resource::ResourceManager::Get().UnloadUnused();

    KumariEngine::Core::Logger::Info("ThreadSafety_Test", "Thread safety & stress test: PASSED");
}

void RunCameraSystemTests() {
    using namespace KumariEngine::Camera;
    KumariEngine::Core::Logger::Info("Camera_Test", "Starting Camera System verification tests...");

    // 1. Initial State
    auto camera = std::make_shared<Camera>();
    assert(camera->GetMode() == CameraMode::Free);
    assert(glm::distance(camera->GetPosition(), glm::vec3(0.0f)) < 0.001f);
    assert(camera->GetFov() == 60.0f);
    assert(camera->GetPriority() == 0);
    assert(camera->GetRoll() == 0.0f);

    // 2. View and Projection Matrices
    camera->SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));
    camera->SetYaw(0.0f); // Looking straight down -Z
    camera->SetPitch(0.0f);
    camera->SetTranslationSmoothing(0.0f);
    camera->SetRotationSmoothing(0.0f);
    camera->Update(0.016f, nullptr);

    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 proj = camera->GetProjectionMatrix();
    (void)view;
    (void)proj;

    // Verify view matrix projection of forward vector
    glm::vec3 expectedForward(0.0f, 0.0f, -1.0f);
    glm::vec3 forward = camera->GetForward();
    (void)expectedForward;
    (void)forward;
    assert(glm::distance(forward, expectedForward) < 0.001f);

    // 3. Smooth Damping (Interpolation)
    camera->SetPosition(glm::vec3(10.0f, 10.0f, 10.0f));
    camera->SetTranslationSmoothing(5.0f); // slow lerp
    // Position target is (10,10,10), current position should still be (0,5,10) before update,
    // and slightly moved towards target after update.
    assert(glm::distance(camera->GetCurrentPosition(), glm::vec3(0.0f, 5.0f, 10.0f)) < 0.001f);
    camera->Update(0.1f, nullptr); // deltaTime = 0.1s
    assert(camera->GetCurrentPosition().x > 0.0f && camera->GetCurrentPosition().x < 10.0f);
    
    // Snap to target
    camera->SetTranslationSmoothing(0.0f);
    camera->Update(0.016f, nullptr);
    assert(glm::distance(camera->GetCurrentPosition(), glm::vec3(10.0f, 10.0f, 10.0f)) < 0.001f);
    KumariEngine::Core::Logger::Info("Camera_Test", "Smooth damping: PASSED");

    // 4. Zoom support
    camera->SetFov(45.0f);
    camera->Update(0.016f, nullptr);
    // Let's loop a few frames to let FOV converge.
    for (int i = 0; i < 50; ++i) {
        camera->Update(0.1f, nullptr);
    }
    assert(std::abs(camera->GetProjectionMatrix()[1][1] - (1.0f / std::tan(glm::radians(45.0f) * 0.5f))) < 0.1f);
    KumariEngine::Core::Logger::Info("Camera_Test", "Zoom support: PASSED");

    // 5. Camera Collision (Free Cam pushing/sliding)
    camera->SetMode(CameraMode::Free);
    camera->SetTranslationSmoothing(0.0f);
    camera->SetRotationSmoothing(0.0f);
    camera->EnableCollision(true);
    camera->SetCameraRadius(0.5f);
    camera->ClearColliders();

    // Register a sphere collider at (5, 0, 0) with radius 1.5
    CameraCollider sphereCol;
    sphereCol.type = CameraCollider::Type::Sphere;
    sphereCol.center = glm::vec3(5.0f, 0.0f, 0.0f);
    sphereCol.radius = 1.5f;
    camera->AddCollider(sphereCol);

    // Set position to (3.2f, 0.0f, 0.0f) - distance to sphere center is 1.8f.
    // Combined radius is 1.5 + 0.5 = 2.0. So it is penetrating by 0.2 units.
    // ResolveCollision should push it back to (3.0f, 0.0f, 0.0f)
    camera->SetPosition(glm::vec3(3.2f, 0.0f, 0.0f));
    camera->Update(0.016f, nullptr);
    // Position should be pushed back to x = 3.0f
    assert(std::abs(camera->GetCurrentPosition().x - 3.0f) < 0.01f);
    KumariEngine::Core::Logger::Info("Camera_Test", "Free Camera sphere collision: PASSED");

    // 6. Third Person Spring-Arm Obstruction Avoidance
    camera->SetMode(CameraMode::ThirdPerson);
    camera->SetTargetPosition(glm::vec3(0.0f));
    camera->SetTargetHeightOffset(glm::vec3(0.0f)); // no height offset for simplicity
    camera->SetShoulderOffset(glm::vec3(0.0f));
    camera->SetYaw(0.0f); // Looking towards -Z, camera is at +Z
    camera->SetPitch(0.0f);
    camera->SetTargetOrbitDistance(10.0f);
    camera->SetOrbitDistance(10.0f);
    camera->ClearColliders();

    // Desired camera position is (0, 0, 10).
    // Let's place a sphere collider in between target (0,0,0) and camera at (0,0,5) with radius 1.0f.
    CameraCollider midSphere;
    midSphere.type = CameraCollider::Type::Sphere;
    midSphere.center = glm::vec3(0.0f, 0.0f, 5.0f);
    midSphere.radius = 1.0f;
    camera->AddCollider(midSphere);

    camera->Update(0.016f, nullptr);
    // The camera should shorten its distance.
    assert(camera->GetCurrentPosition().z < 9.0f);
    KumariEngine::Core::Logger::Info("Camera_Test", "Third-Person obstruction avoidance: PASSED");

    // 7. Frustum Culling visibility
    camera->SetMode(CameraMode::Free);
    camera->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    camera->SetYaw(0.0f); // looking down -Z
    camera->SetPitch(0.0f);
    camera->SetFov(90.0f);
    camera->SetAspect(1.0f);
    camera->SetNearClip(1.0f);
    camera->SetFarClip(100.0f);
    camera->Update(0.016f, nullptr);

    // Object A at (0, 0, -10) is inside frustum
    assert(camera->IsSphereVisible(glm::vec3(0.0f, 0.0f, -10.0f), 1.0f));
    
    // Object B at (0, 0, 10) is behind camera
    assert(!camera->IsSphereVisible(glm::vec3(0.0f, 0.0f, 10.0f), 1.0f));

    // Object C far to the right (50, 0, -5) is outside frustum planes
    assert(!camera->IsSphereVisible(glm::vec3(50.0f, 0.0f, -5.0f), 1.0f));
    KumariEngine::Core::Logger::Info("Camera_Test", "Frustum culling: PASSED");

    // 8. Camera Manager (Priority and Blend Transitions)
    auto& mgr = CameraManager::Get();
    mgr.Clear();

    auto cam1 = std::make_shared<Camera>();
    cam1->SetPriority(10);
    cam1->SetPosition(glm::vec3(0.0f));
    cam1->SetTranslationSmoothing(0.0f);
    cam1->SetRotationSmoothing(0.0f);

    auto cam2 = std::make_shared<Camera>();
    cam2->SetPriority(20);
    cam2->SetPosition(glm::vec3(100.0f));
    cam2->SetTranslationSmoothing(0.0f);
    cam2->SetRotationSmoothing(0.0f);

    mgr.RegisterCamera("Cam1", cam1);
    mgr.RegisterCamera("Cam2", cam2);

    // Active camera should be cam2 because priority 20 > 10
    assert(mgr.GetActiveCamera() == cam2);

    // Start transition blend from Cam1 to Cam2
    mgr.SetActiveCamera("Cam1");
    assert(mgr.GetActiveCamera() == cam1);

    mgr.BlendToCamera("Cam2", 1.0f, EasingCurve::Linear);
    assert(mgr.IsBlending());
    assert(mgr.GetActiveCamera() != cam1 && mgr.GetActiveCamera() != cam2); // returns m_blendCamera

    // Tick transition
    mgr.Update(0.5f);
    assert(mgr.IsBlending());
    // Position should be exactly midway (50.0f)
    assert(std::abs(mgr.GetActiveCamera()->GetCurrentPosition().x - 50.0f) < 0.01f);

    mgr.Update(0.6f); // exceeds 1.0s blend
    assert(!mgr.IsBlending());
    assert(mgr.GetActiveCamera() == cam2);
    KumariEngine::Core::Logger::Info("Camera_Test", "CameraManager blending & priority: PASSED");

    // 9. Debug Tools
    auto corners = cam1->GetFrustumCorners();
    (void)corners;
    assert(corners.size() == 8);
    for (const auto& corner : corners) {
        (void)corner;
        assert(!std::isnan(corner.x) && !std::isnan(corner.y) && !std::isnan(corner.z));
    }
    KumariEngine::Core::Logger::Info("Camera_Test", "Debug Frustum Corners: PASSED");

    mgr.Clear();
    KumariEngine::Core::Logger::Info("Camera_Test", "All Camera System verification tests completed successfully!");
}

void RunPhysicsNarrowPhaseTests() {
    using namespace KumariEngine::Physics;
    KumariEngine::Core::Logger::Info("Physics_Test", "Starting narrow-phase collision verification tests...");

    // 1. Sphere vs Sphere
    Sphere s1{glm::vec3(0.0f), 1.0f};
    Sphere s2{glm::vec3(1.5f, 0.0f, 0.0f), 1.0f};
    ContactPoint cp;
    bool col = PhysicsWorld::CheckSphereSphere(s1, s2, cp);
    assert(col);
    (void)col;
    assert(std::abs(cp.penetration - 0.5f) < 0.001f);
    assert(std::abs(std::abs(cp.normal.x) - 1.0f) < 0.001f);
    KumariEngine::Core::Logger::Info("Physics_Test", "Sphere-Sphere narrow-phase: PASSED");

    // 2. AABB vs AABB
    AABB a1{glm::vec3(-1.0f), glm::vec3(1.0f)};
    AABB a2{glm::vec3(0.8f, -1.0f, -1.0f), glm::vec3(2.8f, 1.0f, 1.0f)};
    col = PhysicsWorld::CheckAABBAABB(a1, a2, cp);
    assert(col);
    assert(std::abs(cp.penetration - 0.2f) < 0.001f);
    assert(std::abs(std::abs(cp.normal.x) - 1.0f) < 0.001f);
    KumariEngine::Core::Logger::Info("Physics_Test", "AABB-AABB narrow-phase: PASSED");

    // 3. Capsule vs Capsule
    Capsule c1{glm::vec3(0.0f), 1.0f, 0.5f};
    Capsule c2{glm::vec3(0.8f, 0.0f, 0.0f), 1.0f, 0.5f};
    col = PhysicsWorld::CheckCapsuleCapsule(c1, c2, cp);
    assert(col);
    assert(std::abs(cp.penetration - 0.2f) < 0.001f);
    assert(std::abs(std::abs(cp.normal.x) - 1.0f) < 0.001f);
    KumariEngine::Core::Logger::Info("Physics_Test", "Capsule-Capsule narrow-phase: PASSED");

    KumariEngine::Core::Logger::Info("Physics_Test", "All narrow-phase collision tests: PASSED");
}

void RunRaycastTests() {
    using namespace KumariEngine::Physics;
    KumariEngine::Core::Logger::Info("Raycast_Test", "Starting raycasting verification tests...");

    Ray ray{glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    Sphere s{glm::vec3(0.0f), 1.0f};
    RaycastHit hit;
    bool col = Raycast::RaycastSphere(ray, s, hit);
    assert(col);
    (void)col;
    assert(std::abs(hit.distance - 4.0f) < 0.001f);
    assert(std::abs(hit.point.z - 1.0f) < 0.001f);
    assert(std::abs(hit.normal.z - 1.0f) < 0.001f);
    KumariEngine::Core::Logger::Info("Raycast_Test", "Ray-Sphere intersection: PASSED");

    AABB a{glm::vec3(-1.0f), glm::vec3(1.0f)};
    col = Raycast::RaycastAABB(ray, a, hit);
    assert(col);
    assert(std::abs(hit.distance - 4.0f) < 0.001f);
    assert(std::abs(hit.point.z - 1.0f) < 0.001f);
    KumariEngine::Core::Logger::Info("Raycast_Test", "Ray-AABB intersection: PASSED");

    KumariEngine::Core::Logger::Info("Raycast_Test", "All raycasting tests: PASSED");
}

void RunKCCMovementTests() {
    using namespace KumariEngine::Physics;
    KumariEngine::Core::Logger::Info("KCC_Test", "Starting Kinematic Character Controller movement tests...");

    // Initialize TerrainManager for height queries
    KumariEngine::Terrain::TerrainManager::Get().Initialize(1337, 64.0f);

    KumariEngine::ECS::Registry registry;
    auto& sceneMgr = KumariEngine::Scene::SceneManager::Get();
    sceneMgr.Initialize(&registry);

    KumariEngine::ECS::Entity player = registry.CreateEntity();
    auto* playerNode = sceneMgr.CreateNode("PlayerNode");
    playerNode->SetEntity(player);
    
    float startTerrainH = KumariEngine::Terrain::TerrainManager::Get().GetHeightAt(0.0f, 0.0f);
    playerNode->SetLocalPosition(glm::vec3(0.0f, startTerrainH + 10.0f, 0.0f));

    auto& pc = registry.AddComponent<PhysicsComponent>(player);
    pc.bodyType = BodyType::Kinematic;
    pc.collider.type = ColliderType::Capsule;
    pc.collider.shape = Capsule{glm::vec3(0.0f), 1.0f, 0.5f};

    auto& cc = registry.AddComponent<CharacterControllerComponent>(player);
    cc.moveDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    cc.isGrounded = false;

    PhysicsWorld world;
    CharacterController controller;

    // First update should move player right (+X)
    controller.Update(&registry, player, pc, cc, world, 0.1f);
    glm::vec3 pos = playerNode->GetLocalPosition();
    assert(pos.x > 0.0f);
    
    // Simulate falling and snap to terrain
    cc.moveDirection = glm::vec3(0.0f);
    cc.verticalVelocity = -20.0f; // falling fast
    
    // Tick multiple frames to allow KCC to reach ground
    for (int step = 0; step < 20; ++step) {
        controller.Update(&registry, player, pc, cc, world, 0.05f);
        if (cc.isGrounded) break;
    }
    pos = playerNode->GetLocalPosition();
    
    float terrainH = KumariEngine::Terrain::TerrainManager::Get().GetHeightAt(pos.x, pos.z);
    std::cout << "KCC Debug - pos: (" << pos.x << ", " << pos.y << ", " << pos.z << "), terrainH: " << terrainH 
              << ", isGrounded: " << (cc.isGrounded ? "true" : "false") << ", diff: " << std::abs(pos.y - (terrainH + 1.5f)) << std::endl;
    assert(std::abs(pos.y - (terrainH + 1.5f)) < 0.1f); // bottom sphere center is pos.y - 1.5f
    (void)terrainH;
    assert(cc.isGrounded);

    sceneMgr.Shutdown();
    KumariEngine::Core::Logger::Info("KCC_Test", "Kinematic Character Controller tests: PASSED");
}

void RunPhysicsPerformanceStressTest() {
    using namespace KumariEngine::Physics;
    KumariEngine::Core::Logger::Info("Physics_Stress", "Starting Physics Performance Stress Test (100,000 entities)...");

    KumariEngine::ECS::Registry registry;
    
    registry.RegisterComponent<PhysicsComponent>();
    registry.Reserve<PhysicsComponent>(100000);

    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        KumariEngine::ECS::Entity e = registry.CreateEntity();
        auto& pc = registry.AddComponent<PhysicsComponent>(e);
        pc.bodyType = (i % 10 == 0) ? BodyType::Dynamic : BodyType::Static;
        pc.collider.type = ColliderType::Sphere;
        pc.collider.shape = Sphere{glm::vec3(0.0f), 0.5f};
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> creationDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("Physics_Stress", "Spawned 100,000 physics entities in %.3f ms", creationDuration.count());

    SpatialHashGrid grid(2.0f);
    grid.Resize(100000);

    // Measure spatial grid insertions
    startTime = std::chrono::high_resolution_clock::now();
    grid.Clear();
    for (uint32_t i = 1; i <= 100000; ++i) {
        AABB aabb{glm::vec3(i * 0.1f - 0.5f), glm::vec3(i * 0.1f + 0.5f)};
        grid.Insert(i, aabb);
    }
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> insertDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("Physics_Stress", "Broad-phase Spatial Hash Grid Insert for 100,000 entities: %.3f ms", insertDuration.count());

    // Measure querying time
    startTime = std::chrono::high_resolution_clock::now();
    uint32_t results[64];
    uint32_t totalQueries = 0;
    for (uint32_t i = 1; i <= 1000; ++i) { // check 1,000 sample entity queries
        AABB queryAABB{glm::vec3(i * 0.1f - 1.0f), glm::vec3(i * 0.1f + 1.0f)};
        totalQueries += grid.Query(queryAABB, results, 64, i);
    }
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> queryDuration = endTime - startTime;
    KumariEngine::Core::Logger::Info("Physics_Stress", "Broad-phase Spatial Hash Grid Query (1,000 samples): %.3f ms (found %u overlaps)", queryDuration.count(), totalQueries);

    KumariEngine::Core::Logger::Info("Physics_Stress", "Physics Performance Stress Test: PASSED");
}

int main() {
    KumariEngine::Core::Logger::Info("SmokeTest", "Starting automated engine lifecycle test...");

    KumariEngine::Core::Engine engine;

    // Run the ECS scaling test
    RunECSScalingTest();

    // Run the Resource & Asset Manager test
    RunResourceAssetTest();

    // Run the Scene Graph test
    RunSceneGraphHierarchyTest();

    // Run the World Streaming test
    RunWorldStreamingTest();

    // Run the Streaming Thread-Safety test
    RunStreamingThreadSafetyTest();

    // Run the Camera System tests
    RunCameraSystemTests();

    // Run the Physics narrow-phase, raycast, KCC, and stress tests
    RunPhysicsNarrowPhaseTests();
    RunRaycastTests();
    RunKCCMovementTests();
    RunPhysicsPerformanceStressTest();

    // Enable detailed debug logging for Phase C verification
    KumariEngine::Core::Logger::EnableCategories({"SceneManager", "ChunkLoad", "ChunkUnload", "SceneGraph", "ECS"});

    // Configure streaming parameters (configurable values)
    auto& sceneMgr = KumariEngine::Scene::SceneManager::Get();
    sceneMgr.SetChunkSize(64.0f);
    sceneMgr.SetLoadRadius(2);
    sceneMgr.SetUnloadRadius(3);

    // Verify successful initialization of windowing, input, Volk, and Vulkan renderer pipeline
    if (!engine.Initialize("Kumari Engine Smoke Test", 800, 600)) {
        KumariEngine::Core::Logger::Error("SmokeTest", "Vulkan lifecycle initialization failed.");
        return 1;
    }

    KumariEngine::Core::Logger::Info("SmokeTest", "Initialization verified. Shutting down system...");

    // Verify clean resource release
    engine.Shutdown();

    KumariEngine::Core::Logger::Info("SmokeTest", "Automated lifecycle test completed successfully.");
    return 0;
}
