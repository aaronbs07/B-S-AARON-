#include "environment_manager.hpp"
#include "save/SaveSystem.hpp"
#include "core/logger.hpp"

namespace KumariEngine::Environment {

bool EnvironmentManager::SaveEnvironment(const std::string& filepath) const {
    return Save::SaveSystem::WriteSave(filepath, [this](Save::BinaryWriter& writer) {
        // Sky
        writer.WriteFloat(m_sky.skyColor.x);
        writer.WriteFloat(m_sky.skyColor.y);
        writer.WriteFloat(m_sky.skyColor.z);
        writer.WriteFloat(m_sky.turbidity);
        writer.WriteFloat(m_sky.exposure);

        // Sun
        writer.WriteFloat(m_sunDirection.x);
        writer.WriteFloat(m_sunDirection.y);
        writer.WriteFloat(m_sunDirection.z);

        // Ambient
        writer.WriteFloat(m_ambientColor.x);
        writer.WriteFloat(m_ambientColor.y);
        writer.WriteFloat(m_ambientColor.z);
        writer.WriteFloat(m_ambientIntensity);

        // Fog
        writer.WriteBool(m_fog.enabled);
        writer.WriteFloat(m_fog.color.x);
        writer.WriteFloat(m_fog.color.y);
        writer.WriteFloat(m_fog.color.z);
        writer.WriteFloat(m_fog.density);
        writer.WriteFloat(m_fog.start);
        writer.WriteFloat(m_fog.end);

        // Weather
        writer.WriteFloat(m_weather.rainIntensity);
        writer.WriteFloat(m_weather.windSpeed);
        writer.WriteFloat(m_weather.windDirection.x);
        writer.WriteFloat(m_weather.windDirection.y);
        writer.WriteFloat(m_weather.windDirection.z);

        // Volumetric Fog
        writer.WriteBool(m_volumetricFog.enabled);
        writer.WriteFloat(m_volumetricFog.density);
        writer.WriteFloat(m_volumetricFog.anisotropy);
        writer.WriteFloat(m_volumetricFog.scatteringColor.x);
        writer.WriteFloat(m_volumetricFog.scatteringColor.y);
        writer.WriteFloat(m_volumetricFog.scatteringColor.z);
        writer.WriteFloat(m_volumetricFog.heightFalloff);
        writer.WriteFloat(m_volumetricFog.heightOffset);

        // Light Shafts
        writer.WriteBool(m_lightShafts.enabled);
        writer.WriteFloat(m_lightShafts.density);
        writer.WriteFloat(m_lightShafts.weight);
        writer.WriteFloat(m_lightShafts.decay);
        writer.WriteFloat(m_lightShafts.exposure);
        writer.WriteInt32(static_cast<int32_t>(m_lightShafts.samples));

        // Atmospheric Settings
        writer.WriteBool(m_atmospheric.enabled);
        writer.WriteFloat(m_atmospheric.timeOfDay);
        writer.WriteFloat(m_atmospheric.cycleSpeed);
        writer.WriteFloat(m_atmospheric.turbidity);
        writer.WriteFloat(m_atmospheric.groundAlbedo.x);
        writer.WriteFloat(m_atmospheric.groundAlbedo.y);
        writer.WriteFloat(m_atmospheric.groundAlbedo.z);
        writer.WriteFloat(m_atmospheric.sunColor.x);
        writer.WriteFloat(m_atmospheric.sunColor.y);
        writer.WriteFloat(m_atmospheric.sunColor.z);
        writer.WriteFloat(m_atmospheric.sunIntensity);
        writer.WriteFloat(m_atmospheric.moonColor.x);
        writer.WriteFloat(m_atmospheric.moonColor.y);
        writer.WriteFloat(m_atmospheric.moonColor.z);
        writer.WriteFloat(m_atmospheric.moonIntensity);
        writer.WriteFloat(m_atmospheric.cloudCoverage);
        writer.WriteFloat(m_atmospheric.cloudSpeed);

        return true;
    });
}

bool EnvironmentManager::LoadEnvironment(const std::string& filepath) {
    return Save::SaveSystem::ReadSave(filepath, [this](Save::BinaryReader& reader, uint32_t version) {
        (void)version;
        // Sky
        float sx, sy, sz, turb, exp;
        if (!reader.ReadFloat(sx) || !reader.ReadFloat(sy) || !reader.ReadFloat(sz) ||
            !reader.ReadFloat(turb) || !reader.ReadFloat(exp)) return false;
        m_sky.skyColor = glm::vec3(sx, sy, sz);
        m_sky.turbidity = turb;
        m_sky.exposure = exp;

        // Sun
        float dx, dy, dz;
        if (!reader.ReadFloat(dx) || !reader.ReadFloat(dy) || !reader.ReadFloat(dz)) return false;
        m_sunDirection = glm::normalize(glm::vec3(dx, dy, dz));

        // Ambient
        float ax, ay, az, aIntensity;
        if (!reader.ReadFloat(ax) || !reader.ReadFloat(ay) || !reader.ReadFloat(az) || !reader.ReadFloat(aIntensity)) return false;
        m_ambientColor = glm::vec3(ax, ay, az);
        m_ambientIntensity = aIntensity;

        // Fog
        bool fogEnabled;
        float fx, fy, fz, fDensity, fStart, fEnd;
        if (!reader.ReadBool(fogEnabled) || !reader.ReadFloat(fx) || !reader.ReadFloat(fy) || !reader.ReadFloat(fz) ||
            !reader.ReadFloat(fDensity) || !reader.ReadFloat(fStart) || !reader.ReadFloat(fEnd)) return false;
        m_fog.enabled = fogEnabled;
        m_fog.color = glm::vec3(fx, fy, fz);
        m_fog.density = fDensity;
        m_fog.start = fStart;
        m_fog.end = fEnd;

        // Weather
        float rain, wSpeed, wdx, wdy, wdz;
        if (!reader.ReadFloat(rain) || !reader.ReadFloat(wSpeed) ||
            !reader.ReadFloat(wdx) || !reader.ReadFloat(wdy) || !reader.ReadFloat(wdz)) return false;
        m_weather.rainIntensity = rain;
        m_weather.windSpeed = wSpeed;
        m_weather.windDirection = glm::vec3(wdx, wdy, wdz);

        // Volumetric Fog
        bool volFogEnabled;
        float vfD, vfA, vfSCx, vfSCy, vfSCz, vfFH, vfFO;
        if (!reader.ReadBool(volFogEnabled) || !reader.ReadFloat(vfD) || !reader.ReadFloat(vfA) ||
            !reader.ReadFloat(vfSCx) || !reader.ReadFloat(vfSCy) || !reader.ReadFloat(vfSCz) ||
            !reader.ReadFloat(vfFH) || !reader.ReadFloat(vfFO)) return false;
        m_volumetricFog.enabled = volFogEnabled;
        m_volumetricFog.density = vfD;
        m_volumetricFog.anisotropy = vfA;
        m_volumetricFog.scatteringColor = glm::vec3(vfSCx, vfSCy, vfSCz);
        m_volumetricFog.heightFalloff = vfFH;
        m_volumetricFog.heightOffset = vfFO;

        // Light Shafts
        bool lsEnabled;
        float lsD, lsW, lsDecay, lsExp;
        int32_t lsSamples32;
        if (!reader.ReadBool(lsEnabled) || !reader.ReadFloat(lsD) || !reader.ReadFloat(lsW) ||
            !reader.ReadFloat(lsDecay) || !reader.ReadFloat(lsExp) || !reader.ReadInt32(lsSamples32)) return false;
        m_lightShafts.enabled = lsEnabled;
        m_lightShafts.density = lsD;
        m_lightShafts.weight = lsW;
        m_lightShafts.decay = lsDecay;
        m_lightShafts.exposure = lsExp;
        m_lightShafts.samples = static_cast<int>(lsSamples32);

        // Atmospheric Settings
        bool atmosEnabled;
        float atTime, atSpeed, atTurb, atGAx, atGAy, atGAz, atSCx, atSCy, atSCz, atSunInt, atMCx, atMCy, atMCz, atMoonInt, atCC, atCS;
        if (!reader.ReadBool(atmosEnabled) || !reader.ReadFloat(atTime) || !reader.ReadFloat(atSpeed) || !reader.ReadFloat(atTurb) ||
            !reader.ReadFloat(atGAx) || !reader.ReadFloat(atGAy) || !reader.ReadFloat(atGAz) ||
            !reader.ReadFloat(atSCx) || !reader.ReadFloat(atSCy) || !reader.ReadFloat(atSCz) || !reader.ReadFloat(atSunInt) ||
            !reader.ReadFloat(atMCx) || !reader.ReadFloat(atMCy) || !reader.ReadFloat(atMCz) || !reader.ReadFloat(atMoonInt) ||
            !reader.ReadFloat(atCC) || !reader.ReadFloat(atCS)) return false;
        m_atmospheric.enabled = atmosEnabled;
        m_atmospheric.timeOfDay = atTime;
        m_atmospheric.cycleSpeed = atSpeed;
        m_atmospheric.turbidity = atTurb;
        m_atmospheric.groundAlbedo = glm::vec3(atGAx, atGAy, atGAz);
        m_atmospheric.sunColor = glm::vec3(atSCx, atSCy, atSCz);
        m_atmospheric.sunIntensity = atSunInt;
        m_atmospheric.moonColor = glm::vec3(atMCx, atMCy, atMCz);
        m_atmospheric.moonIntensity = atMoonInt;
        m_atmospheric.cloudCoverage = atCC;
        m_atmospheric.cloudSpeed = atCS;

        return true;
    });
}

} // namespace KumariEngine::Environment
