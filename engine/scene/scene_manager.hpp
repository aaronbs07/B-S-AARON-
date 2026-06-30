#pragma once
#include <unordered_map>
#include <future>
#include <string>
#include <string_view>
#include <memory>
#include <glm/glm.hpp>
#include "scene_node.hpp"
#include "ecs/ecs.hpp"
#include "resource/resource.hpp"

namespace KumariEngine::Scene {

struct ChunkCoord {
    int32_t x;
    int32_t z;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const {
        return (static_cast<std::size_t>(c.x) * 397) ^ static_cast<std::size_t>(c.z);
    }
};

struct EntityData {
    std::string name;
    glm::vec3 localPosition;
    glm::quat localRotation;
    glm::vec3 localScale;
};

class ChunkAsset : public Resource::Resource {
public:
    ChunkAsset() = default;
    virtual ~ChunkAsset() override = default;

    std::vector<EntityData> entities;
};

struct Chunk {
    ChunkCoord coord;
    enum class State {
        Unloaded,
        Loading,
        Loaded
    };
    State state = State::Unloaded;
    std::vector<ECS::Entity> entities;
    SceneNode* chunkNode = nullptr;
};

class SceneManager {
public:
    static SceneManager& Get() {
        static SceneManager instance;
        return instance;
    }

    // Prevent copy/assignment
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    void Update(float deltaTime);

    SceneNode* CreateNode(std::string_view name, SceneNode* parent = nullptr);
    void DestroyNode(SceneNode* node);

    void SetViewerPosition(const glm::vec3& pos) { m_viewerPosition = pos; }
    const glm::vec3& GetViewerPosition() const { return m_viewerPosition; }

    SceneNode* GetRootNode() const { return m_rootNode.get(); }
    ECS::Registry* GetRegistry() const { return m_registry; }

    void SetChunkSize(float size) { m_chunkSize = size; }
    float GetChunkSize() const { return m_chunkSize; }

    void SetLoadRadius(int32_t radius) { m_loadRadius = radius; }
    int32_t GetLoadRadius() const { return m_loadRadius; }

    void SetUnloadRadius(int32_t radius) { m_unloadRadius = radius; }
    int32_t GetUnloadRadius() const { return m_unloadRadius; }

    bool IsChunkLoaded(int32_t x, int32_t z) const;
    bool IsChunkLoading(int32_t x, int32_t z) const;
    size_t GetLoadedChunkCount() const { return m_loadedChunks.size(); }
    size_t GetLoadingChunkCount() const { return m_loadingChunks.size(); }

private:
    SceneManager() = default;
    ~SceneManager() = default;

    void UpdateStreaming();
    void ProcessLoadedChunks();

    ECS::Registry* m_registry = nullptr;
    std::unique_ptr<SceneNode> m_rootNode;

    float m_chunkSize = 64.0f;
    int32_t m_loadRadius = 2;
    int32_t m_unloadRadius = 3;
    glm::vec3 m_viewerPosition{0.0f};

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_loadedChunks;
    std::unordered_map<ChunkCoord, std::future<std::shared_ptr<ChunkAsset>>, ChunkCoordHash> m_loadingChunks;
};

} // namespace KumariEngine::Scene
