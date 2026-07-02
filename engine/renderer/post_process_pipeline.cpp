#include "post_process_pipeline.hpp"
#include <cmath>
#include <algorithm>

namespace KumariEngine::Renderer {

PostProcessingPipeline::PostProcessingPipeline() = default;

void PostProcessingPipeline::UpdateAutoExposure(float averageLuminance, float deltaTime) {
    if (!m_toneMapping.enableAutoExposure) {
        m_adaptedExposure = m_toneMapping.exposure;
        return;
    }

    float avgLum = std::max(averageLuminance, 0.0001f);
    // Key value / middle gray target is typically 0.18 for standard auto-exposure
    float targetExposure = 0.18f / avgLum;
    targetExposure = std::clamp(targetExposure, m_toneMapping.minExposure, m_toneMapping.maxExposure);

    // Exponential adaptation formula: E_new = E_old + (E_target - E_old) * (1 - e^(-dt * speed))
    float rate = 1.0f - std::exp(-deltaTime * m_toneMapping.autoExposureSpeed);
    m_adaptedExposure = m_adaptedExposure + (targetExposure - m_adaptedExposure) * rate;
}

float PostProcessingPipeline::GetGPUTime(const std::string& name) const {
    auto it = m_gpuTimings.find(name);
    return it != m_gpuTimings.end() ? it->second : 0.0f;
}

} // namespace KumariEngine::Renderer
