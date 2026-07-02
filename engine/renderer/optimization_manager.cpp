#include "renderer/optimization_manager.hpp"
#include "camera/camera.hpp"
#include "core/logger.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace KumariEngine::Renderer {

OptimizationManager::OptimizationManager() {
    // Set default stats
    m_stats = OptimizationStats();
}

OptimizationManager::~OptimizationManager() {
    Shutdown();
}

void OptimizationManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    m_device = device;
    m_physicalDevice = physicalDevice;

    if (m_device == VK_NULL_HANDLE) {
        Core::Logger::Info("OptimizationManager", "Initialized in Headless/Mock mode.");
        return;
    }

    Core::Logger::Info("OptimizationManager", "Initializing Vulkan optimization resources...");

    // Create HZB Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 16.0f;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_hzbSampler) != VK_SUCCESS) {
        Core::Logger::Error("OptimizationManager", "Failed to create HZB Sampler.");
    }
}

void OptimizationManager::Shutdown() {
    if (m_device != VK_NULL_HANDLE) {
        if (m_hzbSampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_hzbSampler, nullptr);
            m_hzbSampler = VK_NULL_HANDLE;
        }

        for (auto view : m_hzbImageViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, view, nullptr);
            }
        }
        m_hzbImageViews.clear();

        if (m_hzbImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_hzbImage, nullptr);
            m_hzbImage = VK_NULL_HANDLE;
        }

        if (m_hzbMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_hzbMemory, nullptr);
            m_hzbMemory = VK_NULL_HANDLE;
        }
    }
    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
}

void OptimizationManager::RegisterObject(const RenderObject& obj) {
    m_objects[obj.id] = obj;
}

void OptimizationManager::UnregisterObject(uint32_t id) {
    m_objects.erase(id);
}

void OptimizationManager::UpdateObjectTransform(uint32_t id, const glm::mat4& transform) {
    auto it = m_objects.find(id);
    if (it != m_objects.end()) {
        it->second.transform = transform;
    }
}

void OptimizationManager::ClearObjects() {
    m_objects.clear();
}

CameraFrustum OptimizationManager::ExtractFrustumPlanes(const glm::mat4& viewProj) const {
    CameraFrustum f;
    // Left Plane
    f.planes[0] = FrustumPlane(glm::vec4(viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0]));
    // Right Plane
    f.planes[1] = FrustumPlane(glm::vec4(viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0]));
    // Bottom Plane
    f.planes[2] = FrustumPlane(glm::vec4(viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1]));
    // Top Plane
    f.planes[3] = FrustumPlane(glm::vec4(viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1]));
    // Near Plane
    f.planes[4] = FrustumPlane(glm::vec4(viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2], viewProj[2][3] + viewProj[2][2], viewProj[3][3] + viewProj[3][2]));
    // Far Plane
    f.planes[5] = FrustumPlane(glm::vec4(viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2]));
    return f;
}

BoundingBox OptimizationManager::GetWorldAABB(const RenderObject& obj) const {
    BoundingBox box;
    glm::vec3 corners[8] = {
        {obj.localAABB.min.x, obj.localAABB.min.y, obj.localAABB.min.z},
        {obj.localAABB.max.x, obj.localAABB.min.y, obj.localAABB.min.z},
        {obj.localAABB.min.x, obj.localAABB.max.y, obj.localAABB.min.z},
        {obj.localAABB.max.x, obj.localAABB.max.y, obj.localAABB.min.z},
        {obj.localAABB.min.x, obj.localAABB.min.y, obj.localAABB.max.z},
        {obj.localAABB.max.x, obj.localAABB.min.y, obj.localAABB.max.z},
        {obj.localAABB.min.x, obj.localAABB.max.y, obj.localAABB.max.z},
        {obj.localAABB.max.x, obj.localAABB.max.y, obj.localAABB.max.z}
    };

    glm::vec3 worldMin(std::numeric_limits<float>::max());
    glm::vec3 worldMax(-std::numeric_limits<float>::max());

    for (int i = 0; i < 8; ++i) {
        glm::vec3 worldCorner = glm::vec3(obj.transform * glm::vec4(corners[i], 1.0f));
        worldMin = glm::min(worldMin, worldCorner);
        worldMax = glm::max(worldMax, worldCorner);
    }

    box.min = worldMin;
    box.max = worldMax;
    return box;
}

BoundingSphere OptimizationManager::GetWorldSphere(const RenderObject& obj) const {
    BoundingSphere sphere;
    sphere.center = glm::vec3(obj.transform * glm::vec4(obj.localSphere.center, 1.0f));
    
    // Extrapolate scaling from model matrix scale factor
    float scaleX = glm::length(glm::vec3(obj.transform[0]));
    float scaleY = glm::length(glm::vec3(obj.transform[1]));
    float scaleZ = glm::length(glm::vec3(obj.transform[2]));
    float maxScale = glm::max(scaleX, glm::max(scaleY, scaleZ));
    
    sphere.radius = obj.localSphere.radius * maxScale;
    return sphere;
}

void OptimizationManager::OptimizeFrame(const Camera::Camera* camera, double currentTime, float deltaTime) {
    (void)deltaTime;
    m_lastFrameTime = currentTime;
    auto startTime = std::chrono::high_resolution_clock::now();

    m_stats = OptimizationStats();
    m_stats.totalObjects = static_cast<uint32_t>(m_objects.size());
    m_visibleObjects.clear();
    m_activeBatches.clear();
    m_indirectCommands.clear();

    if (m_objects.empty() || !camera) {
        auto endTime = std::chrono::high_resolution_clock::now();
        m_stats.cpuTimingMs = static_cast<float>(std::chrono::duration<double, std::milli>(endTime - startTime).count());
        return;
    }

    glm::vec3 cameraPos = camera->GetCurrentPosition();
    glm::mat4 viewProj = camera->GetProjectionMatrix() * camera->GetViewMatrix();
    CameraFrustum frustum = ExtractFrustumPlanes(viewProj);

    // 1. Traversal and culling
    for (const auto& [id, obj] : m_objects) {
        if (!obj.visible) continue;

        bool culled = false;

        // Frustum culling (AABB & Bounding Sphere support)
        if (m_settings.frustumCullingEnabled) {
            BoundingSphere wsSphere = GetWorldSphere(obj);
            bool sphereIn = true;
            for (int i = 0; i < 6; ++i) {
                if (glm::dot(frustum.planes[i].normal, wsSphere.center) + frustum.planes[i].d < -wsSphere.radius) {
                    sphereIn = false;
                    break;
                }
            }

            if (!sphereIn) {
                // Secondary check using AABB
                BoundingBox wsAABB = GetWorldAABB(obj);
                bool aabbIn = true;
                for (int i = 0; i < 6; ++i) {
                    glm::vec3 p = wsAABB.min;
                    if (frustum.planes[i].normal.x >= 0) p.x = wsAABB.max.x;
                    if (frustum.planes[i].normal.y >= 0) p.y = wsAABB.max.y;
                    if (frustum.planes[i].normal.z >= 0) p.z = wsAABB.max.z;

                    if (glm::dot(frustum.planes[i].normal, p) + frustum.planes[i].d < 0.0f) {
                        aabbIn = false;
                        break;
                    }
                }

                if (!aabbIn) {
                    culled = true;
                    m_stats.frustumCulled++;
                }
            }
        }

        // Occlusion culling simulation / test
        if (!culled && m_settings.occlusionCullingEnabled) {
            BoundingBox wsAABB = GetWorldAABB(obj);
            BoundingSphere wsSphere = GetWorldSphere(obj);
            if (CheckOcclusion(wsAABB, wsSphere)) {
                culled = true;
                m_stats.occlusionCulled++;
            }
        }

        if (!culled) {
            m_visibleObjects.push_back(obj);
        }
    }

    m_stats.visibleObjects = static_cast<uint32_t>(m_visibleObjects.size());

    // 2. LOD Selection
    if (m_settings.lodEnabled) {
        PerformLODSelection(cameraPos);
    } else {
        for (auto& obj : m_visibleObjects) {
            obj.lastLodLevel = 0;
            m_stats.lodCounts[0]++;
        }
    }

    // 3. Render Queue sorting (Material and transparency)
    if (m_settings.renderQueueSortingEnabled) {
        SortRenderQueues(cameraPos);
    }

    // 4. GPU Instancing batching compilation
    if (m_settings.gpuInstancingEnabled) {
        CompileInstancingBatches();
    } else {
        m_stats.drawCalls = m_stats.visibleObjects;
    }

    // 5. Indirect draw generation
    if (m_settings.indirectRenderingEnabled) {
        GenerateIndirectCommands();
    }

    // 6. Streaming Optimization step
    if (m_settings.streamingOptimizationEnabled) {
        UpdateStreaming(currentTime);
    }

    m_stats.memoryUsageBytes = GetStreamingMemoryUsage();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.cpuTimingMs = static_cast<float>(std::chrono::duration<double, std::milli>(endTime - startTime).count());
    m_stats.gpuTimingMs = m_stats.cpuTimingMs * 0.85f; // Mock GPU timing proportionate to CPU load
}

void OptimizationManager::PerformLODSelection(const glm::vec3& cameraPos) {
    for (auto& obj : m_visibleObjects) {
        glm::vec3 objPos = glm::vec3(obj.transform[3]);
        float dist = glm::distance(cameraPos, objPos);

        uint32_t currentLod = 0;
        uint32_t lastLod = obj.lastLodLevel;

        // Apply hysteresis check
        float hyst = m_settings.lodHysteresis;

        if (lastLod == 0) {
            if (dist > m_settings.lodDistances[0] + hyst) currentLod = 1;
            if (dist > m_settings.lodDistances[1] + hyst) currentLod = 2;
        } else if (lastLod == 1) {
            if (dist < m_settings.lodDistances[0] - hyst) currentLod = 0;
            else if (dist > m_settings.lodDistances[1] + hyst) currentLod = 2;
            else currentLod = 1;
        } else { // lastLod == 2
            if (dist < m_settings.lodDistances[1] - hyst) currentLod = 1;
            if (dist < m_settings.lodDistances[0] - hyst) currentLod = 0;
            else currentLod = 2;
        }

        // Clip to maximum available LOD levels
        uint32_t maxLods = static_cast<uint32_t>(obj.lodMeshPaths.size());
        if (maxLods > 0 && currentLod >= maxLods) {
            currentLod = maxLods - 1;
        }

        obj.lastLodLevel = currentLod;

        // Persist LOD level selection to original object map to support frame-to-frame hysteresis
        auto it = m_objects.find(obj.id);
        if (it != m_objects.end()) {
            it->second.lastLodLevel = currentLod;
        }
        
        // LOD Visualization Mode: overrides path or visualizes color code
        if (m_settings.lodVisualizationMode) {
            // Stats indicator tracking LOD
            m_stats.lodCounts[currentLod]++;
        } else {
            m_stats.lodCounts[currentLod]++;
        }

        // Update active mesh path based on LOD
        if (maxLods > currentLod && !obj.lodMeshPaths[currentLod].empty()) {
            obj.meshPath = obj.lodMeshPaths[currentLod];
        }
    }
}

void OptimizationManager::SortRenderQueues(const glm::vec3& cameraPos) {
    // Sort logic:
    // 1. Opaque objects first, transparent objects last.
    // 2. Opaque sorted Front-to-Back (to maximize Early-Z rejection).
    // 3. Transparent sorted Back-to-Front (correct alpha blending).
    // 4. Sub-sort by Material and then Mesh to minimize state transitions.

    std::sort(m_visibleObjects.begin(), m_visibleObjects.end(), [&](const RenderObject& a, const RenderObject& b) {
        if (a.isTransparent != b.isTransparent) {
            return !a.isTransparent; // opaques come first
        }

        float distA = glm::distance(cameraPos, glm::vec3(a.transform[3]));
        float distB = glm::distance(cameraPos, glm::vec3(b.transform[3]));

        if (a.isTransparent) {
            // Transparent: Back-to-Front
            if (std::abs(distA - distB) > 0.01f) {
                return distA > distB;
            }
        } else {
            // Opaque: Front-to-Back
            if (std::abs(distA - distB) > 0.01f) {
                return distA < distB;
            }
        }

        // Sub-sort by Material path
        if (a.materialPath != b.materialPath) {
            return a.materialPath < b.materialPath;
        }

        // Sub-sort by Mesh path
        return a.meshPath < b.meshPath;
    });
}

void OptimizationManager::CompileInstancingBatches() {
    std::unordered_map<std::string, size_t> batchIndices;

    for (const auto& obj : m_visibleObjects) {
        // Create batch key based on mesh, material and active LOD level
        std::string key = obj.meshPath + "|" + obj.materialPath + "|" + std::to_string(obj.lastLodLevel);
        
        auto it = batchIndices.find(key);
        if (it != batchIndices.end()) {
            m_activeBatches[it->second].transforms.push_back(obj.transform);
            m_activeBatches[it->second].objectIds.push_back(obj.id);
        } else {
            InstancingBatch batch{};
            batch.meshPath = obj.meshPath;
            batch.materialPath = obj.materialPath;
            batch.lodLevel = obj.lastLodLevel;
            batch.transforms.push_back(obj.transform);
            batch.objectIds.push_back(obj.id);

            batchIndices[key] = m_activeBatches.size();
            m_activeBatches.push_back(batch);
        }
    }

    m_stats.drawCalls = static_cast<uint32_t>(m_activeBatches.size());
    m_stats.batchedDrawCalls = m_stats.drawCalls;
    
    for (const auto& b : m_activeBatches) {
        if (b.transforms.size() > 1) {
            m_stats.instanceCount += static_cast<uint32_t>(b.transforms.size());
        }
    }
}

void OptimizationManager::GenerateIndirectCommands() {
    uint32_t commandIndex = 0;
    for (const auto& batch : m_activeBatches) {
        DrawIndexedIndirectCommand cmd{};
        cmd.indexCount = 36; // Simulated box index count
        cmd.instanceCount = static_cast<uint32_t>(batch.transforms.size());
        cmd.firstIndex = 0;
        cmd.vertexOffset = 0;
        cmd.firstInstance = commandIndex;

        m_indirectCommands.push_back(cmd);
        commandIndex += cmd.instanceCount;
    }
    m_stats.indirectDrawCommands = static_cast<uint32_t>(m_indirectCommands.size());
}

void OptimizationManager::PrepareHZB(VkCommandBuffer cmd, VkImage srcDepthImage, VkExtent2D extent) {
    (void)cmd; (void)srcDepthImage;
    // Set HZB extent
    m_hzbExtent = extent;
    m_hzbMipLevels = static_cast<uint32_t>(std::floor(std::log2(glm::max(extent.width, extent.height)))) + 1;
}

bool OptimizationManager::CheckOcclusion(const BoundingBox& worldAABB, const BoundingSphere& worldSphere) const {
    (void)worldAABB;
    // In headless/mock mode, simulate occlusion culling:
    // Cull objects that are located directly behind mock occluder bounds.
    // For example, if there is a massive hill or building occluding static geometry,
    // objects with center coordinates placed between certain occluded boundaries are skipped.
    
    // We simulate occlusion check:
    // If center coordinates lie in a mock blocked region (e.g. Z < -10 and X between -5 and 5, depth blocked)
    glm::vec3 c = worldSphere.center;
    if (c.z < -25.0f && std::abs(c.x) < 8.0f && std::abs(c.y) < 5.0f) {
        return true; // Culled!
    }
    return false;
}

void OptimizationManager::RequestBackgroundAssetLoad(const std::string& path, const std::string& type, size_t size) {
    double accessTime = m_lastFrameTime;
    if (accessTime == 0.0) {
        accessTime = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    auto it = m_streamedResources.find(path);
    if (it != m_streamedResources.end()) {
        it->second.lastAccessTime = accessTime;
        it->second.loaded = true;
    } else {
        StreamedResourceEntry entry{};
        entry.path = path;
        entry.type = type;
        entry.sizeBytes = size;
        entry.lastAccessTime = accessTime;
        entry.loaded = true;

        m_streamedResources[path] = entry;
        Core::Logger::Info("Streaming", "Requested background stream load for asset: %s (%s, %zu bytes)", path.c_str(), type.c_str(), size);
    }
}

void OptimizationManager::UpdateStreaming(double currentTime) {
    // Evict unused assets from memory
    std::vector<std::string> toEvict;
    for (auto& [path, entry] : m_streamedResources) {
        if (entry.loaded && (currentTime - entry.lastAccessTime > m_resourceUnloadDelay)) {
            toEvict.push_back(path);
        }
    }

    for (const auto& path : toEvict) {
        auto& entry = m_streamedResources[path];
        entry.loaded = false;
        Core::Logger::Info("Streaming", "Unloaded expired background streamed asset: %s (Released %zu bytes)", path.c_str(), entry.sizeBytes);
    }
}

size_t OptimizationManager::GetStreamingMemoryUsage() const {
    size_t total = 0;
    for (const auto& [_, entry] : m_streamedResources) {
        if (entry.loaded) {
            total += entry.sizeBytes;
        }
    }
    return total;
}

} // namespace KumariEngine::Renderer
