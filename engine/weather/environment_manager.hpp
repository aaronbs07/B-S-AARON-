#pragma once
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include "save/BinaryWriter.hpp"
#include "save/BinaryReader.hpp"

#include "renderer/volumetric_lighting_manager.hpp"
#include "weather/atmospheric_renderer.hpp"

namespace KumariEngine::Environment {

struct SkySettings {
    glm::vec3 skyColor{0.2f, 0.4f, 0.8f};
    float turbidity = 2.0f;
    float exposure = 1.0f;
};

struct FogSettings {
    bool enabled = true;
    glm::vec3 color{0.5f, 0.6f, 0.7f};
    float density = 0.01f;
    float start = 10.0f;
    float end = 100.0f;
};

struct WeatherSettings {
    float rainIntensity = 0.0f;
    float windSpeed = 1.0f;
    glm::vec3 windDirection{1.0f, 0.0f, 0.0f};
};

class EnvironmentManager {
public:
    static EnvironmentManager& Get() {
        static EnvironmentManager instance;
        return instance;
    }

    EnvironmentManager(const EnvironmentManager&) = delete;
    EnvironmentManager& operator=(const EnvironmentManager&) = delete;

    void SetSkySettings(const SkySettings& settings) { m_sky = settings; }
    const SkySettings& GetSkySettings() const { return m_sky; }

    void SetSunDirection(const glm::vec3& dir) { m_sunDirection = glm::normalize(dir); }
    const glm::vec3& GetSunDirection() const { return m_sunDirection; }

    void SetAmbientLighting(const glm::vec3& color, float intensity) {
        m_ambientColor = color;
        m_ambientIntensity = intensity;
    }
    const glm::vec3& GetAmbientColor() const { return m_ambientColor; }
    float GetAmbientIntensity() const { return m_ambientIntensity; }

    void SetFogSettings(const FogSettings& settings) { m_fog = settings; }
    const FogSettings& GetFogSettings() const { return m_fog; }

    void SetWeatherSettings(const WeatherSettings& settings) { m_weather = settings; }
    const WeatherSettings& GetWeatherSettings() const { return m_weather; }

    void SetVolumetricFogSettings(const Renderer::VolumetricFogSettings& settings) { m_volumetricFog = settings; }
    const Renderer::VolumetricFogSettings& GetVolumetricFogSettings() const { return m_volumetricFog; }

    void SetLightShaftSettings(const Renderer::LightShaftSettings& settings) { m_lightShafts = settings; }
    const Renderer::LightShaftSettings& GetLightShaftSettings() const { return m_lightShafts; }

    void SetAtmosphericSettings(const AtmosphericSettings& settings) { m_atmospheric = settings; }
    const AtmosphericSettings& GetAtmosphericSettings() const { return m_atmospheric; }

    bool SaveEnvironment(const std::string& filepath) const;
    bool LoadEnvironment(const std::string& filepath);

private:
    EnvironmentManager() = default;
    ~EnvironmentManager() = default;

    SkySettings m_sky;
    glm::vec3 m_sunDirection{0.0f, -1.0f, 0.0f};
    glm::vec3 m_ambientColor{0.1f, 0.1f, 0.1f};
    float m_ambientIntensity = 0.5f;
    FogSettings m_fog;
    WeatherSettings m_weather;
    Renderer::VolumetricFogSettings m_volumetricFog;
    Renderer::LightShaftSettings m_lightShafts;
    AtmosphericSettings m_atmospheric;
};

} // namespace KumariEngine::Environment
