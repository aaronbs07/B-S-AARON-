#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "lighting/light_component.hpp"

namespace KumariEngine::Renderer {

struct GPULight {
    int type = 0; // 0 = Directional, 1 = Point, 2 = Spot
    int castShadows = 0;
    float shadowBias = 0.005f;
    float shadowNormalBias = 0.01f;
    glm::vec3 position{0.0f};
    float padding2 = 0.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float padding3 = 0.0f;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float innerCutoff = 15.0f;
    float outerCutoff = 20.0f;
    float padding4[2] = {0.0f, 0.0f};
};

struct GPUSceneData {
    glm::vec3 cameraPos{0.0f};
    int lightCount = 0;
    GPULight lights[32];
    glm::mat4 shadowMatrices[32];
};

class LightManager {
public:
    LightManager();
    ~LightManager() = default;

    void ClearLights();
    void AddLight(Lighting::LightType type, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float radius, float innerCutoff, float outerCutoff, bool castShadows = false, float shadowBias = 0.005f, float shadowNormalBias = 0.01f);

    const GPUSceneData& GetSceneData(const glm::vec3& cameraPos);
    void SetShadowMatrix(int index, const glm::mat4& matrix) { if (index >= 0 && index < 32) m_sceneData.shadowMatrices[index] = matrix; }

private:
    GPUSceneData m_sceneData;
};

} // namespace KumariEngine::Renderer
