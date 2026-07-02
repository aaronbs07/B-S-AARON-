#pragma once
#include <glm/glm.hpp>

namespace KumariEngine::Lighting {

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightComponent {
    LightType type = LightType::Directional;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float innerCutoff = 15.0f;
    float outerCutoff = 20.0f;

    bool castShadows = false;
    float shadowBias = 0.005f;
    float shadowNormalBias = 0.01f;
    int shadowMapResolution = 1024;

    LightComponent() = default;
};

} // namespace KumariEngine::Lighting
