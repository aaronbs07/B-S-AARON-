#pragma once

namespace KumariEngine::Renderer {

struct HDRSettings {
    float exposure = 1.0f;
    bool enableToneMapping = true;
};

class HDRPipeline {
public:
    HDRPipeline();
    ~HDRPipeline() = default;

    void SetExposure(float exposure) { m_settings.exposure = exposure; }
    float GetExposure() const { return m_settings.exposure; }

    void SetToneMappingEnabled(bool enable) { m_settings.enableToneMapping = enable; }
    bool IsToneMappingEnabled() const { return m_settings.enableToneMapping; }

    const HDRSettings& GetSettings() const { return m_settings; }

private:
    HDRSettings m_settings;
};

} // namespace KumariEngine::Renderer
