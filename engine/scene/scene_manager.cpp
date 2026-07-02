#include "scene_manager.hpp"
#include "core/logger.hpp"
#include "asset_pipeline/asset_manager.hpp"
#include <cmath>
#include <algorithm>
#include <thread>

namespace KumariEngine::Scene {

void SceneManager::Initialize(ECS::Registry* registry) {
    m_registry = registry;
    m_rootNode = std::make_unique<SceneNode>("Root");
    Core::Logger::Info("SceneManager", "Scene Manager initialized.");
}

void SceneManager::Shutdown() {
    Core::Logger::Info("SceneManager", "Shutting down Scene Manager...");

    // Wait for all loading futures to avoid thread crashes when accessing destroyed objects
    for (auto& [coord, future] : m_loadingChunks) {
        if (future.valid()) {
            try {
                future.wait();
            } catch (...) {}
        }
    }
    m_loadingChunks.clear();

    // Clear loaded chunks
    for (auto& [coord, chunk] : m_loadedChunks) {
        if (chunk.chunkNode) {
            DestroyNode(chunk.chunkNode);
        }
    }
    m_loadedChunks.clear();

    m_rootNode.reset();
    m_registry = nullptr;
    Core::Logger::Info("SceneManager", "Scene Manager shut down cleanly.");
}

void SceneManager::Reset() {
    if (m_rootNode) {
        while (!m_rootNode->GetChildren().empty()) {
            DestroyNode(m_rootNode->GetChildren().back().get());
        }
    }
    m_entityNodeMap.clear();
}

void SceneManager::Update(float deltaTime) {
    (void)deltaTime;
    if (!m_rootNode) return;

    // 1. Process loading tasks that are ready
    ProcessLoadedChunks();

    // 2. Perform streaming checks based on viewer position
    UpdateStreaming();

    // 3. Update all transforms recursively from the root
    m_rootNode->UpdateTransforms();
}

SceneNode* SceneManager::CreateNode(std::string_view name, SceneNode* parent) {
    auto node = std::make_unique<SceneNode>(name);
    SceneNode* rawNode = node.get();
    
    if (!parent) {
        parent = m_rootNode.get();
    }
    
    if (parent) {
        parent->AddChild(std::move(node));
    }
    return rawNode;
}

void SceneManager::DestroyNode(SceneNode* node) {
    if (!node || node == m_rootNode.get()) return;

    // Recursively destroy ECS entities in subtree
    struct EntityDestroyer {
        static void DestroyEntityTree(SceneNode* n, ECS::Registry* reg) {
            for (auto& child : n->GetChildren()) {
                DestroyEntityTree(child.get(), reg);
            }
            if (n->GetEntity() != ECS::NULL_ENTITY && reg) {
                reg->DestroyEntity(n->GetEntity());
                n->SetEntity(ECS::NULL_ENTITY);
            }
        }
    };
    EntityDestroyer::DestroyEntityTree(node, m_registry);

    // Detach from parent to trigger destruction of the unique_ptr hierarchy
    if (node->GetParent()) {
        node->GetParent()->RemoveChild(node);
    }
}

bool SceneManager::IsChunkLoaded(int32_t x, int32_t z) const {
    return m_loadedChunks.find(ChunkCoord{x, z}) != m_loadedChunks.end();
}

bool SceneManager::IsChunkLoading(int32_t x, int32_t z) const {
    return m_loadingChunks.find(ChunkCoord{x, z}) != m_loadingChunks.end();
}

void SceneManager::ProcessLoadedChunks() {
    int32_t viewerChunkX = static_cast<int32_t>(std::floor(m_viewerPosition.x / m_chunkSize));
    int32_t viewerChunkZ = static_cast<int32_t>(std::floor(m_viewerPosition.z / m_chunkSize));

    for (auto it = m_loadingChunks.begin(); it != m_loadingChunks.end();) {
        auto& [coord, future] = *it;
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto asset = future.get();
            
            int32_t distX = std::abs(coord.x - viewerChunkX);
            int32_t distZ = std::abs(coord.z - viewerChunkZ);
            int32_t maxDist = std::max(distX, distZ);

            if (maxDist <= m_unloadRadius) {
                Chunk chunk;
                chunk.coord = coord;
                chunk.state = Chunk::State::Loaded;
                
                std::string chunkNodeName = "ChunkNode_" + std::to_string(coord.x) + "_" + std::to_string(coord.z);
                chunk.chunkNode = CreateNode(chunkNodeName, m_rootNode.get());
                chunk.chunkNode->SetLocalPosition(glm::vec3(coord.x * m_chunkSize, 0.0f, coord.z * m_chunkSize));

                if (asset && m_registry) {
                    chunk.entities.reserve(asset->entities.size());
                    for (const auto& entData : asset->entities) {
                        ECS::Entity entity = m_registry->CreateEntity();
                        SceneNode* entNode = CreateNode(entData.name, chunk.chunkNode);
                        entNode->SetEntity(entity);
                        entNode->SetLocalPosition(entData.localPosition);
                        entNode->SetLocalRotation(entData.localRotation);
                        entNode->SetLocalScale(entData.localScale);

                        chunk.entities.push_back(entity);
                    }
                }

                m_loadedChunks[coord] = chunk;
                Core::Logger::Info("SceneManager", "Successfully streamed in chunk (%d, %d)", coord.x, coord.z);
            } else {
                Core::Logger::Info("SceneManager", "Discarded chunk (%d, %d) upon load completion as it is outside unload radius", coord.x, coord.z);
            }
            it = m_loadingChunks.erase(it);
        } else {
            ++it;
        }
    }
}

void SceneManager::UpdateStreaming() {
    int32_t viewerChunkX = static_cast<int32_t>(std::floor(m_viewerPosition.x / m_chunkSize));
    int32_t viewerChunkZ = static_cast<int32_t>(std::floor(m_viewerPosition.z / m_chunkSize));

    // 1. Unload distant chunks
    for (auto it = m_loadedChunks.begin(); it != m_loadedChunks.end();) {
        const auto& coord = it->first;
        int32_t distX = std::abs(coord.x - viewerChunkX);
        int32_t distZ = std::abs(coord.z - viewerChunkZ);
        if (std::max(distX, distZ) > m_unloadRadius) {
            auto& chunk = it->second;
            
            if (chunk.chunkNode) {
                DestroyNode(chunk.chunkNode);
            }
            
            Core::Logger::Info("SceneManager", "Unloaded chunk (%d, %d) and destroyed associated ECS entities.", coord.x, coord.z);
            it = m_loadedChunks.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Load nearby chunks
    for (int32_t x = viewerChunkX - m_loadRadius; x <= viewerChunkX + m_loadRadius; ++x) {
        for (int32_t z = viewerChunkZ - m_loadRadius; z <= viewerChunkZ + m_loadRadius; ++z) {
            ChunkCoord coord{x, z};
            if (m_loadedChunks.find(coord) == m_loadedChunks.end() &&
                m_loadingChunks.find(coord) == m_loadingChunks.end()) {
                
                std::string path = "world/chunk_" + std::to_string(x) + "_" + std::to_string(z);
                auto future = Asset::AssetManager::Get().LoadAsync<ChunkAsset>(path, [x, z]() {
                    auto asset = std::make_shared<ChunkAsset>();
                    
                    // Simulate loading latency
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    
                    for (int i = 0; i < 3; ++i) {
                        EntityData data;
                        data.name = "Chunk_" + std::to_string(x) + "_" + std::to_string(z) + "_Entity_" + std::to_string(i);
                        data.localPosition = glm::vec3(i * 10.0f, 0.0f, i * 10.0f);
                        data.localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                        data.localScale = glm::vec3(1.0f);
                        asset->entities.push_back(data);
                    }
                    return asset;
                });
                
                m_loadingChunks[coord] = std::move(future);
                Core::Logger::Info("SceneManager", "Triggered asynchronous stream-in for chunk (%d, %d)", x, z);
            }
        }
    }
}

void SceneManager::RegisterEntityNode(ECS::Entity entity, SceneNode* node) {
    if (entity != ECS::NULL_ENTITY && node) {
        m_entityNodeMap[entity] = node;
    }
}

void SceneManager::UnregisterEntityNode(ECS::Entity entity) {
    if (entity != ECS::NULL_ENTITY) {
        m_entityNodeMap.erase(entity);
    }
}

SceneNode* SceneManager::GetNodeByEntity(ECS::Entity entity) const {
    if (entity == ECS::NULL_ENTITY) return nullptr;
    auto it = m_entityNodeMap.find(entity);
    if (it != m_entityNodeMap.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace KumariEngine::Scene
