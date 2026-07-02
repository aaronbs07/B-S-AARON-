#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <volk.h>
#include <glm/glm.hpp>

#include "ssr_manager.hpp"

namespace KumariEngine::Renderer {

enum class AAMode {
    None,
    FXAA,
    TAA
};

struct AntiAliasingSettings {
    AAMode mode = AAMode::TAA;
    float fxaaContrastThreshold = 0.125f;
    float fxaaSubpixelBlending = 0.75f;
    float taaHistoryWeight = 0.9f;
    float taaJitterScale = 1.0f;
};

enum class SSAOQuality {
    Low,
    Medium,
    High
};

struct ToneMappingSettings {
    bool enabled = true;
    float exposure = 1.0f;
    bool enableAutoExposure = false;
    float minExposure = 0.5f;
    float maxExposure = 3.0f;
    float autoExposureSpeed = 1.0f;
    enum class Mode {
        Reinhard,
        ACES
    } mode = Mode::ACES;
    float gamma = 2.2f;
};

struct BloomSettings {
    bool enabled = false;
    float threshold = 0.8f;
    float intensity = 1.0f;
    int blurPasses = 4;
};

struct SSAOSettings {
    bool enabled = false;
    SSAOQuality quality = SSAOQuality::Medium;
    float radius = 0.5f;
    float bias = 0.025f;
    float intensity = 1.0f;
};

struct ColorGradingSettings {
    bool enabled = false;
    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float temperature = 6500.0f; // in Kelvin
    float tint = 0.0f;
    std::string lutPath = "";
    bool lutEnabled = false;
};

struct DepthOfFieldSettings {
    bool enabled = false;
    float focusDistance = 10.0f;
    float aperture = 2.8f;
    float nearBlurRange = 5.0f;
    float farBlurRange = 15.0f;
};

struct MotionBlurSettings {
    bool enabled = false;
    float intensity = 1.0f;
    int maxSamples = 16;
    bool enableCameraMotionBlur = true;
    bool enableObjectMotionBlur = true;
};

struct DebugVisualizerSettings {
    bool enabled = false;
    std::string targetToVisualize = "swapchain"; // e.g. "shadow_atlas", "ssao_target", "bloom_target", "hdr_target", etc.
    bool showGPUTimings = false;
};

class PostProcessingPipeline {
public:
    PostProcessingPipeline();
    ~PostProcessingPipeline() = default;

    // Settings Accessors
    ToneMappingSettings& GetToneMapping() { return m_toneMapping; }
    const ToneMappingSettings& GetToneMapping() const { return m_toneMapping; }

    BloomSettings& GetBloom() { return m_bloom; }
    const BloomSettings& GetBloom() const { return m_bloom; }

    SSAOSettings& GetSSAO() { return m_ssao; }
    const SSAOSettings& GetSSAO() const { return m_ssao; }

    ColorGradingSettings& GetColorGrading() { return m_colorGrading; }
    const ColorGradingSettings& GetColorGrading() const { return m_colorGrading; }

    DepthOfFieldSettings& GetDepthOfField() { return m_dof; }
    const DepthOfFieldSettings& GetDepthOfField() const { return m_dof; }

    MotionBlurSettings& GetMotionBlur() { return m_motionBlur; }
    const MotionBlurSettings& GetMotionBlur() const { return m_motionBlur; }

    SSRSettings& GetSSR() { return m_ssr; }
    const SSRSettings& GetSSR() const { return m_ssr; }

    AntiAliasingSettings& GetAntiAliasing() { return m_antiAliasing; }
    const AntiAliasingSettings& GetAntiAliasing() const { return m_antiAliasing; }

    DebugVisualizerSettings& GetDebugVisualizer() { return m_debugVisualizer; }
    const DebugVisualizerSettings& GetDebugVisualizer() const { return m_debugVisualizer; }

    // Dynamic auto exposure logic
    float GetAdaptedExposure() const { return m_adaptedExposure; }
    void UpdateAutoExposure(float averageLuminance, float deltaTime);

    // Dynamic state and profiling
    void SetGPUTime(const std::string& name, float ms) { m_gpuTimings[name] = ms; }
    float GetGPUTime(const std::string& name) const;
    const std::unordered_map<std::string, float>& GetGPUTimings() const { return m_gpuTimings; }

private:
    ToneMappingSettings m_toneMapping;
    BloomSettings m_bloom;
    SSAOSettings m_ssao;
    ColorGradingSettings m_colorGrading;
    DepthOfFieldSettings m_dof;
    MotionBlurSettings m_motionBlur;
    SSRSettings m_ssr;
    AntiAliasingSettings m_antiAliasing;
    DebugVisualizerSettings m_debugVisualizer;

    float m_adaptedExposure = 1.0f;
    std::unordered_map<std::string, float> m_gpuTimings;
};

} // namespace KumariEngine::Renderer
