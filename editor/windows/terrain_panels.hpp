#pragma once
#include "editor/window_system.hpp"
#include "terrain/terrain_manager.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace KumariEngine::Editor {

class TerrainInspectorWindow : public EditorWindow {
public:
    TerrainInspectorWindow() : EditorWindow("Terrain Inspector") {}
    void RenderUI() override;
    void ModifySettings(uint32_t seed, float chunkSize, int loadRadius, int unloadRadius);
};

class TerrainBrushPanelWindow : public EditorWindow {
public:
    TerrainBrushPanelWindow() : EditorWindow("Terrain Brush Panel") {}
    void RenderUI() override;

    // Brush parameter accessors
    void SetBrushType(Terrain::TerrainManager::BrushType type) { m_type = type; }
    void SetRadius(float radius) { m_radius = radius; }
    void SetStrength(float strength) { m_strength = strength; }
    void SetTextureLayer(int layer) { m_targetLayer = layer; }
    void SetVegetationType(int type) { m_vegType = type; }
    void SetVegetationDensity(float density) { m_vegDensity = density; }
    void SetVegetationErase(bool erase) { m_vegErase = erase; }

    // Execute painting
    void PaintHeight(float worldX, float worldZ, float deltaTime);
    void PaintTexture(float worldX, float worldZ, float deltaTime);
    void PaintVegetation(float worldX, float worldZ);

private:
    Terrain::TerrainManager::BrushType m_type = Terrain::TerrainManager::BrushType::RaiseLower;
    float m_radius = 5.0f;
    float m_strength = 2.0f;
    int m_targetLayer = 0;
    int m_vegType = 0;
    float m_vegDensity = 5.0f;
    bool m_vegErase = false;
};

class WorldSettingsPanelWindow : public EditorWindow {
public:
    WorldSettingsPanelWindow() : EditorWindow("World Settings") {}
    void RenderUI() override;

    void AddRoadPoint(const glm::vec3& pt) { m_tempRoadPoints.push_back(pt); }
    void AddRiverPoint(const glm::vec3& pt) { m_tempRiverPoints.push_back(pt); }
    void ClearTempPoints() { m_tempRoadPoints.clear(); m_tempRiverPoints.clear(); }

    void CreateRoadSpline(float width, int type);
    void CreateRiverSpline(float width, float depth);

private:
    std::vector<glm::vec3> m_tempRoadPoints;
    std::vector<glm::vec3> m_tempRiverPoints;
};

class EnvironmentPanelWindow : public EditorWindow {
public:
    EnvironmentPanelWindow() : EditorWindow("Environment panel") {}
    void RenderUI() override;

    void SetSky(const glm::vec3& color, float turbidity, float exposure);
    void SetSun(const glm::vec3& direction);
    void SetAmbient(const glm::vec3& color, float intensity);
    void SetFog(bool enabled, const glm::vec3& color, float density, float start, float end);
    void SetWeather(float rain, float windSpeed, const glm::vec3& windDir);
};

} // namespace KumariEngine::Editor
