#include "light_manager.hpp"
#include <algorithm>

namespace KumariEngine::Renderer {

LightManager::LightManager() {
    m_sceneData.lightCount = 0;
}

void LightManager::ClearLights() {
    m_sceneData.lightCount = 0;
    for (int i = 0; i < 32; ++i) {
        m_sceneData.shadowMatrices[i] = glm::mat4(1.0f);
    }
}

void LightManager::AddLight(Lighting::LightType type, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float radius, float innerCutoff, float outerCutoff, bool castShadows, float shadowBias, float shadowNormalBias) {
    if (m_sceneData.lightCount >= 32) return;

    GPULight& gpuLight = m_sceneData.lights[m_sceneData.lightCount];
    gpuLight.type = static_cast<int>(type);
    gpuLight.castShadows = castShadows ? 1 : 0;
    gpuLight.shadowBias = shadowBias;
    gpuLight.shadowNormalBias = shadowNormalBias;
    gpuLight.position = position;
    gpuLight.direction = direction;
    gpuLight.color = color;
    gpuLight.intensity = intensity;
    gpuLight.radius = radius;
    gpuLight.innerCutoff = glm::cos(glm::radians(innerCutoff));
    gpuLight.outerCutoff = glm::cos(glm::radians(outerCutoff));

    m_sceneData.lightCount++;
}

const GPUSceneData& LightManager::GetSceneData(const glm::vec3& cameraPos) {
    m_sceneData.cameraPos = cameraPos;
    return m_sceneData;
}

} // namespace KumariEngine::Renderer
