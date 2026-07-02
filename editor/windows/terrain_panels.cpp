#include "terrain_panels.hpp"
#include "editor/undo_redo.hpp"
#include "weather/environment_manager.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Editor {

// TerrainInspectorWindow

void TerrainInspectorWindow::RenderUI() {
    auto& tm = Terrain::TerrainManager::Get();
    Core::Logger::Info("EditorUI", "--- Terrain Inspector Window ---");
    Core::Logger::Info("EditorUI", "  Seed: %d", tm.GetSeed());
    Core::Logger::Info("EditorUI", "  Chunk Size: %.1f", tm.GetChunkSize());
    Core::Logger::Info("EditorUI", "  Active Chunks: %zu", tm.GetLoadedChunkCount());
}

void TerrainInspectorWindow::ModifySettings(uint32_t seed, float chunkSize, int loadRadius, int unloadRadius) {
    auto& tm = Terrain::TerrainManager::Get();
    tm.Initialize(seed, chunkSize);
    tm.SetLoadRadii(loadRadius, unloadRadius);
    Core::Logger::Info("TerrainInspector", "Modified Terrain settings: Seed=%d, Size=%.1f", seed, chunkSize);
}

// TerrainBrushPanelWindow

void TerrainBrushPanelWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- Terrain Brush Panel ---");
    std::string typeStr = "Raise/Lower";
    if (m_type == Terrain::TerrainManager::BrushType::Smooth) typeStr = "Smooth";
    else if (m_type == Terrain::TerrainManager::BrushType::Flatten) typeStr = "Flatten";
    else if (m_type == Terrain::TerrainManager::BrushType::Noise) typeStr = "Noise";
    else if (m_type == Terrain::TerrainManager::BrushType::Erosion) typeStr = "Erosion";

    Core::Logger::Info("EditorUI", "  Brush Type: %s", typeStr.c_str());
    Core::Logger::Info("EditorUI", "  Brush Radius: %.1f", m_radius);
    Core::Logger::Info("EditorUI", "  Brush Strength: %.1f", m_strength);
    Core::Logger::Info("EditorUI", "  Texture Layer: %d", m_targetLayer);
    Core::Logger::Info("EditorUI", "  Vegetation Type: %d (Density: %.1f, Erase: %s)", m_vegType, m_vegDensity, m_vegErase ? "True" : "False");
}

void TerrainBrushPanelWindow::PaintHeight(float worldX, float worldZ, float deltaTime) {
    auto& tm = Terrain::TerrainManager::Get();
    auto oldHeights = tm.GetHeightEdits();

    tm.ApplyBrush(worldX, worldZ, m_type, m_radius, m_strength, deltaTime);

    auto newHeights = tm.GetHeightEdits();
    auto cmd = std::make_shared<ModifyTerrainHeightCommand>(oldHeights, newHeights);
    UndoSystem::Get().Execute(cmd);
}

void TerrainBrushPanelWindow::PaintTexture(float worldX, float worldZ, float deltaTime) {
    auto& tm = Terrain::TerrainManager::Get();
    auto oldWeights = tm.GetLayerEdits();

    tm.ApplyTexturePaint(worldX, worldZ, m_targetLayer, m_radius, m_strength, deltaTime);

    auto newWeights = tm.GetLayerEdits();
    auto cmd = std::make_shared<ModifyTerrainLayersCommand>(oldWeights, newWeights);
    UndoSystem::Get().Execute(cmd);
}

void TerrainBrushPanelWindow::PaintVegetation(float worldX, float worldZ) {
    auto& tm = Terrain::TerrainManager::Get();
    auto oldChunks = tm.GetEditedVegetationChunks();
    auto oldVeg = tm.GetPaintedVegetation();

    tm.ApplyVegetationPaint(worldX, worldZ, m_vegType, m_radius, m_vegDensity, 0.5f, 1.5f, m_vegErase);

    auto newChunks = tm.GetEditedVegetationChunks();
    auto newVeg = tm.GetPaintedVegetation();
    auto cmd = std::make_shared<ModifyTerrainVegetationCommand>(oldChunks, newChunks, oldVeg, newVeg);
    UndoSystem::Get().Execute(cmd);
}

// WorldSettingsPanelWindow

void WorldSettingsPanelWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "--- World Settings Panel ---");
    Core::Logger::Info("EditorUI", "  Temp Road Points: %zu", m_tempRoadPoints.size());
    Core::Logger::Info("EditorUI", "  Temp River Points: %zu", m_tempRiverPoints.size());
}

void WorldSettingsPanelWindow::CreateRoadSpline(float width, int type) {
    auto& tm = Terrain::TerrainManager::Get();
    auto oldRoads = tm.GetRoads();
    auto oldRivers = tm.GetRivers();
    auto oldHeights = tm.GetHeightEdits();
    auto oldWeights = tm.GetLayerEdits();

    tm.CreateRoad(m_tempRoadPoints, width, type);
    tm.ApplySplines();

    auto newRoads = tm.GetRoads();
    auto newRivers = tm.GetRivers();
    auto newHeights = tm.GetHeightEdits();
    auto newWeights = tm.GetLayerEdits();
    auto cmd = std::make_shared<ModifyTerrainSplineCommand>(
        oldRoads, newRoads, oldRivers, newRivers,
        oldHeights, newHeights, oldWeights, newWeights);
    UndoSystem::Get().Execute(cmd);
    m_tempRoadPoints.clear();
}

void WorldSettingsPanelWindow::CreateRiverSpline(float width, float depth) {
    auto& tm = Terrain::TerrainManager::Get();
    auto oldRoads = tm.GetRoads();
    auto oldRivers = tm.GetRivers();
    auto oldHeights = tm.GetHeightEdits();
    auto oldWeights = tm.GetLayerEdits();

    tm.CreateRiver(m_tempRiverPoints, width, depth);
    tm.ApplySplines();

    auto newRoads = tm.GetRoads();
    auto newRivers = tm.GetRivers();
    auto newHeights = tm.GetHeightEdits();
    auto newWeights = tm.GetLayerEdits();
    auto cmd = std::make_shared<ModifyTerrainSplineCommand>(
        oldRoads, newRoads, oldRivers, newRivers,
        oldHeights, newHeights, oldWeights, newWeights);
    UndoSystem::Get().Execute(cmd);
    m_tempRiverPoints.clear();
}

// EnvironmentPanelWindow

void EnvironmentPanelWindow::RenderUI() {
    auto& em = Environment::EnvironmentManager::Get();
    const auto& sky = em.GetSkySettings();
    const auto& fog = em.GetFogSettings();
    const auto& weather = em.GetWeatherSettings();

    Core::Logger::Info("EditorUI", "--- Environment Panel ---");
    Core::Logger::Info("EditorUI", "  Sky Color: (%.2f, %.2f, %.2f)", sky.skyColor.x, sky.skyColor.y, sky.skyColor.z);
    Core::Logger::Info("EditorUI", "  Turbidity: %.1f", sky.turbidity);
    Core::Logger::Info("EditorUI", "  Ambient Color: (%.2f, %.2f, %.2f)", em.GetAmbientColor().x, em.GetAmbientColor().y, em.GetAmbientColor().z);
    Core::Logger::Info("EditorUI", "  Fog: %s (Color: (%.2f, %.2f, %.2f), Density: %.3f)", fog.enabled ? "Enabled" : "Disabled", fog.color.x, fog.color.y, fog.color.z, fog.density);
    Core::Logger::Info("EditorUI", "  Weather: Rain=%.1f, Wind=%.1f", weather.rainIntensity, weather.windSpeed);
}

void EnvironmentPanelWindow::SetSky(const glm::vec3& color, float turbidity, float exposure) {
    auto& em = Environment::EnvironmentManager::Get();
    Environment::SkySettings sky;
    sky.skyColor = color;
    sky.turbidity = turbidity;
    sky.exposure = exposure;
    em.SetSkySettings(sky);
}

void EnvironmentPanelWindow::SetSun(const glm::vec3& direction) {
    Environment::EnvironmentManager::Get().SetSunDirection(direction);
}

void EnvironmentPanelWindow::SetAmbient(const glm::vec3& color, float intensity) {
    Environment::EnvironmentManager::Get().SetAmbientLighting(color, intensity);
}

void EnvironmentPanelWindow::SetFog(bool enabled, const glm::vec3& color, float density, float start, float end) {
    auto& em = Environment::EnvironmentManager::Get();
    Environment::FogSettings fog;
    fog.enabled = enabled;
    fog.color = color;
    fog.density = density;
    fog.start = start;
    fog.end = end;
    em.SetFogSettings(fog);
}

void EnvironmentPanelWindow::SetWeather(float rain, float windSpeed, const glm::vec3& windDir) {
    auto& em = Environment::EnvironmentManager::Get();
    Environment::WeatherSettings weather;
    weather.rainIntensity = rain;
    weather.windSpeed = windSpeed;
    weather.windDirection = windDir;
    em.SetWeatherSettings(weather);
}

} // namespace KumariEngine::Editor
