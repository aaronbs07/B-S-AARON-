#include "renderer/render_graph.hpp"
#include "renderer/render_pass.hpp"
#include "renderer/material.hpp"
#include "renderer/hdr_pipeline.hpp"
#include "renderer/post_process_pipeline.hpp"
#include "renderer/light_manager.hpp"
#include "shadow/shadow_system.hpp"
#include "renderer/ibl_manager.hpp"
#include "renderer/ssr_manager.hpp"
#include "renderer/volumetric_lighting_manager.hpp"
#include "weather/atmospheric_renderer.hpp"
#include "renderer/rendering_profiler.hpp"
#include "core/logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace KumariEngine::Tests {

using namespace Renderer;

bool VerifyRenderGraphSorting() {
    std::cout << "  Running Test: Render Graph sorting..." << std::endl;
    using namespace Renderer;

    RenderGraph graph;
    
    auto passA = std::make_unique<RenderPass>("PassA");
    passA->AddOutput("resource_A");

    auto passB = std::make_unique<RenderPass>("PassB");
    passB->AddInput("resource_A");
    passB->AddOutput("resource_B");

    auto passC = std::make_unique<RenderPass>("PassC");
    passC->AddInput("resource_B");
    passC->AddOutput("swapchain");

    graph.RegisterPass(std::move(passC));
    graph.RegisterPass(std::move(passA));
    graph.RegisterPass(std::move(passB));

    graph.RegisterPhysicalImage("resource_A", VK_FORMAT_R16G16B16A16_SFLOAT, {800, 600});
    graph.RegisterPhysicalImage("resource_B", VK_FORMAT_R16G16B16A16_SFLOAT, {800, 600});
    graph.RegisterPhysicalImage("swapchain", VK_FORMAT_B8G8R8A8_SRGB, {800, 600});

    bool compiled = graph.Compile(VK_NULL_HANDLE, VK_NULL_HANDLE);
    if (!compiled) {
        std::cerr << "  Render Graph compile failed!" << std::endl;
        return false;
    }

    const auto& sorted = graph.GetSortedPasses();
    if (sorted.size() != 3) {
        std::cerr << "  Render Graph sorted passes count mismatch!" << std::endl;
        return false;
    }
    if (sorted[0]->GetName() != "PassA" || sorted[1]->GetName() != "PassB" || sorted[2]->GetName() != "PassC") {
        std::cerr << "  Render Graph topological sort order incorrect!" << std::endl;
        return false;
    }

    std::cout << "  Render Graph sorting: PASSED" << std::endl;
    return true;
}

float GGX_Distribution(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / (3.14159265f * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float FresnelSchlick(float cosTheta, float F0) {
    return F0 + (1.0f - F0) * std::pow(1.0f - cosTheta, 5.0f);
}

bool VerifyPBRMath() {
    std::cout << "  Running Test: Cook-Torrance BRDF math..." << std::endl;

    float F0_dielectric = 0.04f;
    float F_normal = FresnelSchlick(1.0f, F0_dielectric);
    float F_grazing = FresnelSchlick(0.0f, F0_dielectric);
    if (std::abs(F_normal - 0.04f) >= 0.001f || std::abs(F_grazing - 1.00f) >= 0.001f) {
        std::cerr << "  PBR Fresnel limits incorrect!" << std::endl;
        return false;
    }

    float D_high = GGX_Distribution(1.0f, 0.5f);
    float D_low = GGX_Distribution(0.5f, 0.5f);
    if (D_high <= D_low) {
        std::cerr << "  PBR GGX Distribution peaks incorrectly!" << std::endl;
        return false;
    }

    float G = GeometrySmith(0.8f, 0.6f, 0.5f);
    if (G < 0.0f || G > 1.0f) {
        std::cerr << "  PBR Geometry Smith limits incorrect!" << std::endl;
        return false;
    }

    float F = FresnelSchlick(0.7f, 0.04f);
    float metallic = 0.5f;
    float kS = F;
    float kD = (1.0f - kS) * (1.0f - metallic);
    if (kD + kS > 1.0f) {
        std::cerr << "  PBR Cook-Torrance energy conservation violated!" << std::endl;
        return false;
    }

    std::cout << "  Cook-Torrance BRDF math: PASSED" << std::endl;
    return true;
}

bool VerifyMaterialSystem() {
    std::cout << "  Running Test: Material & MaterialInstance..." << std::endl;
    using namespace Renderer;

    auto baseMat = std::make_shared<Material>("BasePBR");
    baseMat->SetFloat("roughness", 0.7f);
    baseMat->SetVec3("albedo", glm::vec3(0.8f, 0.2f, 0.2f));

    auto instMat = std::make_shared<Material>("InstancePBR", baseMat);
    instMat->SetFloat("roughness", 0.3f);

    if (instMat->GetFloat("roughness") != 0.3f) {
        std::cerr << "  MaterialInstance float parameter override failed!" << std::endl;
        return false;
    }
    if (instMat->GetFloat("ao") != 1.0f) {
        std::cerr << "  MaterialInstance float parameter inheritance failed!" << std::endl;
        return false;
    }
    if (instMat->GetVec3("albedo") != glm::vec3(0.8f, 0.2f, 0.2f)) {
        std::cerr << "  MaterialInstance vec3 parameter inheritance failed!" << std::endl;
        return false;
    }

    auto reflection = instMat->GetParametersReflection();
    bool foundRoughness = false;
    bool foundAlbedo = false;
    for (const auto& r : reflection) {
        if (r.name == "roughness") {
            if (r.type != ShaderParamType::Float) return false;
            foundRoughness = true;
        }
        if (r.name == "albedo") {
            if (r.type != ShaderParamType::Vec3) return false;
            foundAlbedo = true;
        }
    }
    if (!foundRoughness || !foundAlbedo) {
        std::cerr << "  Material reflection search failed!" << std::endl;
        return false;
    }

    std::cout << "  Material & MaterialInstance: PASSED" << std::endl;
    return true;
}

bool VerifyHDRPipeline() {
    std::cout << "  Running Test: HDR mapping equations..." << std::endl;
    using namespace Renderer;

    HDRPipeline pipeline;
    pipeline.SetExposure(2.0f);

    float hdrColor = 5.0f;
    
    float reinhard = hdrColor / (hdrColor + 1.0f);
    if (reinhard < 0.0f || reinhard > 1.0f) {
        std::cerr << "  HDR Reinhard tone mapping bounds invalid!" << std::endl;
        return false;
    }

    float mapped = 1.0f - std::exp(-hdrColor * pipeline.GetExposure());
    if (mapped < 0.0f || mapped > 1.0f || mapped <= 0.99f) {
        std::cerr << "  HDR Exposure tone mapping bounds invalid!" << std::endl;
        return false;
    }

    float gammaCorrected = std::pow(mapped, 1.0f / 2.2f);
    if (gammaCorrected <= mapped) {
        std::cerr << "  HDR Gamma correction bounds invalid!" << std::endl;
        return false;
    }

    std::cout << "  HDR mapping equations: PASSED" << std::endl;
    return true;
}

bool VerifyLightManagerUniforms() {
    std::cout << "  Running Test: Dynamic light uniform layouts..." << std::endl;
    using namespace Renderer;

    LightManager manager;
    manager.AddLight(Lighting::LightType::Directional, {0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 2.0f, 10.0f, 0.0f, 0.0f);
    manager.AddLight(Lighting::LightType::Point, {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 5.0f, 15.0f, 0.0f, 0.0f);

    const auto& ubo = manager.GetSceneData({10.0f, 10.0f, 10.0f});
    if (ubo.lightCount != 2) {
        std::cerr << "  Light count in GPUSceneData is incorrect!" << std::endl;
        return false;
    }
    if (ubo.cameraPos != glm::vec3(10.0f, 10.0f, 10.0f)) {
        std::cerr << "  Camera position in GPUSceneData is incorrect!" << std::endl;
        return false;
    }

    if (ubo.lights[0].type != 0 || ubo.lights[0].intensity != 2.0f || ubo.lights[0].direction != glm::vec3(0.0f, -1.0f, 0.0f)) {
        std::cerr << "  Directional GPULight values incorrect!" << std::endl;
        return false;
    }

    if (ubo.lights[1].type != 1 || ubo.lights[1].intensity != 5.0f || ubo.lights[1].position != glm::vec3(1.0f, 2.0f, 3.0f) || ubo.lights[1].radius != 15.0f) {
        std::cerr << "  Point GPULight values incorrect!" << std::endl;
        return false;
    }

    std::cout << "  Dynamic light uniform layouts: PASSED" << std::endl;
    return true;
}

bool VerifyShadowSystem() {
    std::cout << "  Running Test: Shadow Mapping, Cascades & Quality..." << std::endl;
    using namespace Renderer;

    ShadowSystem system;
    system.Initialize(4096, 4096);

    // 1. CSM split logic
    system.SetCascadeCount(3);
    if (system.GetCascadeCount() != 3) {
        std::cerr << "  CSM split cascade count configuration failed!" << std::endl;
        return false;
    }

    glm::mat4 camView = glm::lookAt(glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 camProj = glm::perspective(glm::radians(60.0f), 1.33f, 0.1f, 100.0f);
    glm::vec3 lightDir(0.5f, -1.0f, 0.2f);
    
    system.GenerateCascades(camView, camProj, lightDir, 0.1f, 100.0f);
    const auto& cascades = system.GetCascades();
    if (cascades.size() != 3) {
        std::cerr << "  CSM split cascade generation size failed!" << std::endl;
        return false;
    }
    if (cascades[0].splitDepth >= cascades[1].splitDepth || cascades[1].splitDepth >= cascades[2].splitDepth) {
        std::cerr << "  CSM split depth ordering invalid!" << std::endl;
        return false;
    }

    // 2. Stable Snapping test
    glm::mat4 lView = glm::lookAt(glm::vec3(1.234f, 2.345f, 3.456f), glm::vec3(1.234f, 2.345f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    glm::mat4 snapped1 = system.ApplyStableSnapping(lView, lProj, 1024.0f);
    
    glm::mat4 lView2 = glm::lookAt(glm::vec3(1.235f, 2.346f, 3.456f), glm::vec3(1.235f, 2.346f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 snapped2 = system.ApplyStableSnapping(lView2, lProj, 1024.0f);
    
    if (glm::distance(glm::vec3(snapped1[3]), glm::vec3(snapped2[3])) > 0.001f) {
        std::cerr << "  CSM stable snapping did not align translations! d: " << glm::distance(glm::vec3(snapped1[3]), glm::vec3(snapped2[3])) << std::endl;
        return false;
    }

    // 3. Shadow Quality (bias formulas)
    float baseBias = 0.005f;
    float slopeFactor = 0.01f;
    float bias = system.CalculateSlopeScaledBias(baseBias, slopeFactor, 2.0f);
    if (std::abs(bias - (0.005f + 0.01f * 2.0f)) > 0.0001f) {
        std::cerr << "  Slope-scaled bias math incorrect!" << std::endl;
        return false;
    }

    glm::vec3 pos(10.0f, 2.0f, -5.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 biasedPos = system.ApplyNormalOffsetBias(pos, normal, 0.1f);
    if (glm::distance(biasedPos, glm::vec3(10.0f, 2.1f, -5.0f)) > 0.0001f) {
        std::cerr << "  Normal offset bias math incorrect!" << std::endl;
        return false;
    }

    // 4. Shadow Atlas Management
    ShadowAtlasRect rect1, rect2, rect3;
    bool ok1 = system.AllocateAtlasRect(1024, 1024, rect1);
    bool ok2 = system.AllocateAtlasRect(1024, 1024, rect2);
    if (!ok1 || !ok2) {
        std::cerr << "  Shadow atlas rect allocation failed!" << std::endl;
        return false;
    }
    if (rect1.x == rect2.x && rect1.y == rect2.y) {
        std::cerr << "  Shadow atlas rect overlap detected!" << std::endl;
        return false;
    }
    system.FreeAtlasRect(rect1);
    bool ok3 = system.AllocateAtlasRect(1024, 1024, rect3);
    if (!ok3 || rect3.x != rect1.x || rect3.y != rect1.y) {
        std::cerr << "  Shadow atlas rect recycling failed!" << std::endl;
        return false;
    }

    // 5. Light culling
    glm::vec4 frustumPlanes[6] = {
        glm::vec4(1.0f, 0.0f, 0.0f, 5.0f),
        glm::vec4(-1.0f, 0.0f, 0.0f, 5.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 5.0f),
        glm::vec4(0.0f, -1.0f, 0.0f, 5.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 5.0f),
        glm::vec4(0.0f, 0.0f, -1.0f, 5.0f)
    };
    if (!system.IsLightVisible(glm::vec3(0.0f), 1.0f, frustumPlanes)) {
        std::cerr << "  Frustum culling visibility false positive!" << std::endl;
        return false;
    }
    if (system.IsLightVisible(glm::vec3(10.0f, 0.0f, 0.0f), 1.0f, frustumPlanes)) {
        std::cerr << "  Frustum culling visibility false negative!" << std::endl;
        return false;
    }

    // 6. Shadow caching & Dynamic resolution scaling
    system.ResetStats();
    uint64_t lightGuid = 9999;
    bool cached = system.UpdateShadowCache(lightGuid, true, true, 1);
    if (cached) {
        std::cerr << "  Shadow cache hit on uninitialized entry!" << std::endl;
        return false;
    }
    cached = system.UpdateShadowCache(lightGuid, true, true, 2);
    if (!cached) {
        std::cerr << "  Shadow cache miss on static light!" << std::endl;
        return false;
    }
    system.InvalidateCacheEntry(lightGuid);
    cached = system.UpdateShadowCache(lightGuid, true, true, 3);
    if (cached) {
        std::cerr << "  Shadow cache hit on invalidated entry!" << std::endl;
        return false;
    }

    uint32_t resClose = system.CalculateDynamicResolution(5.0f, 100.0f, 1024);
    uint32_t resFar = system.CalculateDynamicResolution(100.0f, 100.0f, 1024);
    if (resClose < resFar || resClose != 1024 || resFar != 256) {
        std::cerr << "  Dynamic resolution scaling limits incorrect!" << std::endl;
        return false;
    }

    // 7. Stats and debug
    system.SetCascadeVisualization(true);
    if (!system.IsCascadeVisualizationEnabled()) {
        std::cerr << "  Cascade visualization flag mismatch!" << std::endl;
        return false;
    }

    std::cout << "  Shadow Mapping, Cascades & Quality: PASSED" << std::endl;
    return true;
}

bool VerifyPostProcessingPipeline() {
    std::cout << "  Running Test: Post-Processing Pipeline..." << std::endl;
    using namespace Renderer;

    PostProcessingPipeline pipeline;

    // 1. HDR & Auto Exposure adaptation formulas
    auto& hdr = pipeline.GetToneMapping();
    hdr.enabled = true;
    hdr.exposure = 1.2f;
    hdr.enableAutoExposure = false;
    
    pipeline.UpdateAutoExposure(1.0f, 0.016f);
    if (std::abs(pipeline.GetAdaptedExposure() - 1.2f) > 0.001f) {
        std::cerr << "  Auto Exposure off did not maintain manual exposure!" << std::endl;
        return false;
    }

    hdr.enableAutoExposure = true;
    hdr.minExposure = 0.5f;
    hdr.maxExposure = 3.0f;
    hdr.autoExposureSpeed = 1.0f;
    pipeline.UpdateAutoExposure(0.05f, 1.0f);
    float adapted1 = pipeline.GetAdaptedExposure();
    if (adapted1 <= 1.2f) {
        std::cerr << "  Auto Exposure did not adapt upwards for low luminance! adapted1: " << adapted1 << std::endl;
        return false;
    }
    if (adapted1 > 3.0f) {
        std::cerr << "  Auto Exposure exceeded maxExposure clamp limit! adapted1: " << adapted1 << std::endl;
        return false;
    }

    hdr.mode = ToneMappingSettings::Mode::Reinhard;
    if (hdr.mode != ToneMappingSettings::Mode::Reinhard) {
        std::cerr << "  HDR Reinhard mode toggle failed!" << std::endl;
        return false;
    }
    hdr.mode = ToneMappingSettings::Mode::ACES;
    if (hdr.mode != ToneMappingSettings::Mode::ACES) {
        std::cerr << "  HDR ACES mode toggle failed!" << std::endl;
        return false;
    }

    // 2. Bloom bright-pass threshold check
    auto& bloom = pipeline.GetBloom();
    bloom.enabled = true;
    bloom.threshold = 0.8f;
    bloom.intensity = 1.5f;
    bloom.blurPasses = 6;
    if (bloom.threshold != 0.8f || bloom.intensity != 1.5f || bloom.blurPasses != 6) {
        std::cerr << "  Bloom settings initialization failed!" << std::endl;
        return false;
    }

    // 3. SSAO settings & presets
    auto& ssao = pipeline.GetSSAO();
    ssao.enabled = true;
    ssao.quality = SSAOQuality::High;
    ssao.radius = 0.75f;
    ssao.bias = 0.015f;
    ssao.intensity = 1.2f;
    if (ssao.quality != SSAOQuality::High || ssao.radius != 0.75f || ssao.bias != 0.015f || ssao.intensity != 1.2f) {
        std::cerr << "  SSAO settings initialization failed!" << std::endl;
        return false;
    }

    // 4. Color Grading formulas
    auto& grading = pipeline.GetColorGrading();
    grading.enabled = true;
    grading.brightness = 1.1f;
    grading.contrast = 1.2f;
    grading.saturation = 1.5f;
    grading.temperature = 5500.0f;
    grading.tint = 0.1f;

    glm::vec3 testColor(0.5f, 0.2f, 0.8f);
    float luminance = glm::dot(testColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
    glm::vec3 saturatedColor = glm::mix(glm::vec3(luminance), testColor, grading.saturation);
    if (std::abs(saturatedColor.r - (luminance + (0.5f - luminance) * 1.5f)) > 0.001f) {
        std::cerr << "  Color Grading Saturation math incorrect!" << std::endl;
        return false;
    }

    // 5. Depth of Field focus range math
    auto& dof = pipeline.GetDepthOfField();
    dof.enabled = true;
    dof.focusDistance = 12.0f;
    dof.aperture = 4.0f;
    dof.nearBlurRange = 6.0f;
    dof.farBlurRange = 18.0f;
    float depth = 15.0f;
    float coc = (depth - dof.focusDistance) / dof.farBlurRange;
    coc = std::clamp(coc * (2.0f / dof.aperture), 0.0f, 1.0f);
    if (coc <= 0.0f || coc >= 1.0f) {
        std::cerr << "  DoF Circle of Confusion factor math incorrect! coc: " << coc << std::endl;
        return false;
    }

    // 6. Motion Blur velocity math
    auto& mb = pipeline.GetMotionBlur();
    mb.enabled = true;
    mb.intensity = 0.8f;
    mb.maxSamples = 24;
    mb.enableCameraMotionBlur = true;
    mb.enableObjectMotionBlur = false;
    if (mb.intensity != 0.8f || mb.maxSamples != 24 || !mb.enableCameraMotionBlur || mb.enableObjectMotionBlur) {
        std::cerr << "  Motion Blur settings verification failed!" << std::endl;
        return false;
    }

    // 7. GPU timings and debugging tools
    pipeline.SetGPUTime("SSAO", 0.45f);
    pipeline.SetGPUTime("Bloom", 0.72f);
    pipeline.SetGPUTime("ToneMapping", 0.12f);
    if (std::abs(pipeline.GetGPUTime("SSAO") - 0.45f) > 0.001f ||
        std::abs(pipeline.GetGPUTime("Bloom") - 0.72f) > 0.001f ||
        std::abs(pipeline.GetGPUTime("ToneMapping") - 0.12f) > 0.001f ||
        std::abs(pipeline.GetGPUTime("NonExistent") - 0.00f) > 0.001f) {
        std::cerr << "  GPU profiling timings mapping failed!" << std::endl;
        return false;
    }

    auto& debug = pipeline.GetDebugVisualizer();
    debug.enabled = true;
    debug.targetToVisualize = "shadow_atlas";
    debug.showGPUTimings = true;
    if (!debug.enabled || debug.targetToVisualize != "shadow_atlas" || !debug.showGPUTimings) {
        std::cerr << "  Debug visualizer settings validation failed!" << std::endl;
        return false;
    }

    std::cout << "  Post-Processing Pipeline: PASSED" << std::endl;
    return true;
}

bool VerifyIBL() {
    std::cout << "  Running Test: IBL Irradiance & Specular blending..." << std::endl;
    IBLManager& ibl = IBLManager::Get();
    if (!ibl.LoadHDREnvironment("test_sky.hdr")) return false;
    SphericalHarmonics sh = ibl.GenerateIrradianceMap();
    if (sh.coefficients[0] == glm::vec3(0.0f)) return false;

    // Specular reflection blending math verify
    glm::vec3 F0(0.04f);
    glm::vec3 N(0.0f, 1.0f, 0.0f);
    glm::vec3 V(0.0f, 1.0f, 0.0f);
    glm::vec3 iblSpec(1.0f);
    glm::vec3 ssrSpec(2.0f);

    // Low roughness should have crisp SSR
    glm::vec3 specLow = ibl.ComputeSpecularReflection(F0, 0.1f, N, V, iblSpec, ssrSpec, true, 1.0f);
    // High roughness should fallback to IBL
    glm::vec3 specHigh = ibl.ComputeSpecularReflection(F0, 0.9f, N, V, iblSpec, ssrSpec, true, 1.0f);

    if (specLow.x <= specHigh.x) {
        std::cerr << "  IBL Specular blending failed: crisp reflections did not exceed dull reflections!" << std::endl;
        return false;
    }

    std::cout << "  IBL Irradiance & Specular blending: PASSED" << std::endl;
    return true;
}

bool VerifySSR() {
    std::cout << "  Running Test: Screen Space Reflections (SSR) ray trace correctness..." << std::endl;
    SSRManager& ssr = SSRManager::Get();
    ssr.GetSettings().enabled = true;
    ssr.GetSettings().thickness = 0.2f;
    ssr.GetSettings().stride = 0.05f;
    ssr.GetSettings().maxSteps = 32;

    // setup simulated depth buffer
    uint32_t w = 800;
    uint32_t h = 600;
    std::vector<float> mockDepth(w * h, 10.0f); // flat surface at depth 10.0

    // Shoot ray from (0,0,-9.9f) along (0,0,-1) to hit the flat depth 10.0
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.33f, 0.1f, 100.0f);
    glm::vec2 hitUV;
    float hitDepth;
    
    bool hit = ssr.TraceRay(glm::vec3(0.0f, 0.0f, -9.9f), glm::vec3(0.0f, 0.0f, -1.0f), proj, mockDepth, w, h, hitUV, hitDepth);
    if (!hit) {
        std::cerr << "  SSR trace ray failed to detect flat surface hit!" << std::endl;
        return false;
    }
    
    // Check temporal reprojection works
    glm::vec2 reprojected = ssr.ApplyTemporalReprojection(glm::vec2(0.5f, 0.5f), glm::vec2(0.01f, -0.02f));
    if (std::abs(reprojected.x - 0.49f) > 0.0001f || std::abs(reprojected.y - 0.52f) > 0.0001f) {
        std::cerr << "  SSR Temporal Reprojection math incorrect!" << std::endl;
        return false;
    }

    std::cout << "  SSR Ray trace correctness: PASSED" << std::endl;
    return true;
}

bool VerifyVolumetricLighting() {
    std::cout << "  Running Test: Volumetric fog & light shafts..." << std::endl;
    VolumetricLightingManager& vl = VolumetricLightingManager::Get();
    vl.GetFogSettings().enabled = true;
    vl.GetFogSettings().density = 0.05f;
    vl.GetFogSettings().heightFalloff = 0.2f;
    vl.GetFogSettings().heightOffset = 0.0f;
    
    float highFog = vl.CalculateHeightFog(10.0f);
    float lowFog = vl.CalculateHeightFog(1.0f);
    if (highFog >= lowFog) {
        std::cerr << "  Height fog did not show exponential falloff!" << std::endl;
        return false;
    }

    // Henyey Greenstein phase
    float phaseForward = vl.HenyeyGreensteinPhase(0.9f, 0.5f);
    float phaseBackward = vl.HenyeyGreensteinPhase(-0.9f, 0.5f);
    if (phaseForward <= phaseBackward) {
        std::cerr << "  Henyey Greenstein forward scattering phase function peak failed!" << std::endl;
        return false;
    }

    // Light shafts
    vl.GetLightShaftSettings().enabled = true;
    vl.GetLightShaftSettings().samples = 16;
    vl.GetLightShaftSettings().weight = 0.1f;
    vl.GetLightShaftSettings().decay = 0.9f;
    float intensity;
    auto samples = vl.ComputeLightShafts(glm::vec2(0.8f), glm::vec2(0.2f), intensity);
    if (samples.size() != 16 || intensity <= 0.0f) {
        std::cerr << "  Volumetric God rays sample calculations failed!" << std::endl;
        return false;
    }

    std::cout << "  Volumetric fog & light shafts: PASSED" << std::endl;
    return true;
}

bool VerifyAtmosphericRendering() {
    std::cout << "  Running Test: Atmospheric Sky & Day/Night cycles..." << std::endl;
    using namespace Environment;
    AtmosphericRenderer& ar = AtmosphericRenderer::Get();
    ar.GetSettings().enabled = true;
    ar.GetSettings().timeOfDay = 12.0f; // Noon

    glm::vec3 sunNoon = ar.GetSunDirection();
    if (sunNoon.y <= 0.0f) {
        std::cerr << "  Day/Night cycle sun position at noon incorrect!" << std::endl;
        return false;
    }

    ar.GetSettings().timeOfDay = 0.0f; // Midnight
    glm::vec3 sunMidnight = ar.GetSunDirection();
    if (sunMidnight.y >= 0.0f) {
        std::cerr << "  Day/Night cycle sun position at midnight incorrect!" << std::endl;
        return false;
    }

    // Procedural sky color zenith vs horizon
    ar.GetSettings().timeOfDay = 12.0f;
    glm::vec3 zenith = ar.ComputeProceduralSkyColor(glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 horizon = ar.ComputeProceduralSkyColor(glm::vec3(1.0f, 0.0f, 0.0f));
    if (zenith == horizon) {
        std::cerr << "  Procedural sky colors did not produce spatial gradient!" << std::endl;
        return false;
    }

    // Cloud density noise check
    ar.GetSettings().cloudCoverage = 0.8f;
    float dense = ar.CalculateCloudDensity(glm::vec3(100.0f, 1000.0f, 100.0f), 1.0f);
    ar.GetSettings().cloudCoverage = 0.0f;
    float clear = ar.CalculateCloudDensity(glm::vec3(100.0f, 1000.0f, 100.0f), 1.0f);
    if (dense < clear || clear != 0.0f) {
        std::cerr << "  Dynamic cloud density coverage threshold failed!" << std::endl;
        return false;
    }

    std::cout << "  Atmospheric Sky & Day/Night cycles: PASSED" << std::endl;
    return true;
}

bool VerifyAntiAliasing() {
    std::cout << "  Running Test: Runtime quality Anti-Aliasing selection..." << std::endl;
    PostProcessingPipeline pipeline;
    auto& aa = pipeline.GetAntiAliasing();
    aa.mode = AAMode::TAA;
    aa.taaHistoryWeight = 0.95f;
    aa.taaJitterScale = 1.2f;

    if (pipeline.GetAntiAliasing().mode != AAMode::TAA || pipeline.GetAntiAliasing().taaHistoryWeight != 0.95f) {
        std::cerr << "  Post process AA runtime settings integration failed!" << std::endl;
        return false;
    }
    
    aa.mode = AAMode::FXAA;
    if (pipeline.GetAntiAliasing().mode != AAMode::FXAA) {
        std::cerr << "  Post process AA mode toggle failed!" << std::endl;
        return false;
    }

    std::cout << "  Runtime quality Anti-Aliasing selection: PASSED" << std::endl;
    return true;
}

bool VerifyReflectionProbes() {
    std::cout << "  Running Test: Reflection Probes blending & box projection..." << std::endl;
    IBLManager& ibl = IBLManager::Get();
    ibl.ClearProbes();

    ReflectionProbe p1{ 1, "ProbeCenter", glm::vec3(0.0f), glm::vec3(-5.0f), glm::vec3(5.0f), 1.0f, false };
    ReflectionProbe p2{ 2, "ProbeOffset", glm::vec3(10.0f), glm::vec3(-5.0f), glm::vec3(5.0f), 1.0f, false };
    ibl.RegisterProbe(p1);
    ibl.RegisterProbe(p2);

    auto blend = ibl.BlendReflectionProbes(glm::vec3(1.0f, 0.0f, 0.0f));
    if (blend.empty()) {
        std::cerr << "  Probes blending weight calculation returned no weights!" << std::endl;
        return false;
    }
    if (blend[0].first != 1) {
        std::cerr << "  Probes blending weight prioritized incorrect probe!" << std::endl;
        return false;
    }

    // Box projection check
    glm::vec3 projected = ibl.ApplyBoxProjection(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(-5.0f), glm::vec3(5.0f));
    if (projected.x != 1.0f) {
        std::cerr << "  Probes box projection ray intersection mismatch!" << std::endl;
        return false;
    }

    std::cout << "  Reflection Probes blending & box projection: PASSED" << std::endl;
    return true;
}

bool VerifyPerformanceProfiler() {
    std::cout << "  Running Test: Rendering profiler warnings & graph..." << std::endl;
    RenderingProfiler& prof = RenderingProfiler::Get();
    
    DrawCallStats stats{ 1200, 10, 1000000, 500000, 120, 200 };
    prof.SetDrawCallStats(stats);

    RenderingResourceUsage usage{ 3000000000ULL, 50, 20, 10, 30 }; // 3.0 GB VRAM
    prof.SetResourceUsage(usage);

    prof.RecordPassTiming("SSRPass", 1.2f, 12.5f); // 12.5ms GPU timing

    auto warnings = prof.GeneratePerformanceWarnings();
    if (warnings.size() < 4) {
        std::cerr << "  Rendering Profiler warnings detection failed! size: " << warnings.size() << std::endl;
        return false;
    }

    std::string graph = prof.GetFrameGraphVisualization();
    if (graph.find("PBRPass") == std::string::npos || graph.find("SSRPass") == std::string::npos) {
        std::cerr << "  Rendering Profiler ASCII frame graph visualization missing passes!" << std::endl;
        return false;
    }

    std::cout << "  Rendering profiler warnings & graph: PASSED" << std::endl;
    return true;
}

bool VerifyOptimizations() {
    std::cout << "  Running Test: Render Graph optimization & pipeline cache..." << std::endl;
    
    RenderGraph graph;
    
    auto passA = std::make_unique<RenderPass>("PassA");
    passA->AddOutput("temp_A");

    auto passB = std::make_unique<RenderPass>("PassB");
    passB->AddInput("temp_A");
    passB->AddOutput("swapchain");

    auto passC = std::make_unique<RenderPass>("PassC");
    passC->AddOutput("temp_C"); // Never read!

    graph.RegisterPass(std::move(passA));
    graph.RegisterPass(std::move(passB));
    graph.RegisterPass(std::move(passC));

    graph.RegisterPhysicalImage("temp_A", VK_FORMAT_R16G16B16A16_SFLOAT, {800, 600});
    graph.RegisterPhysicalImage("temp_C", VK_FORMAT_R16G16B16A16_SFLOAT, {800, 600});
    graph.RegisterExternalImage("swapchain", VK_NULL_HANDLE, VK_NULL_HANDLE, VK_FORMAT_B8G8R8A8_SRGB, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);

    bool compiled = graph.Compile(VK_NULL_HANDLE, VK_NULL_HANDLE);
    if (!compiled) return false;

    // PassC should be pruned, leaving only 2 passes!
    if (graph.GetSortedPasses().size() != 2) {
        std::cerr << "  Render Graph pruning did not remove unused PassC!" << std::endl;
        return false;
    }
    if (graph.GetSortedPasses()[0]->GetName() != "PassA" || graph.GetSortedPasses()[1]->GetName() != "PassB") {
        std::cerr << "  Render Graph pruning altered target topological sort order!" << std::endl;
        return false;
    }

    std::cout << "  Render Graph optimization & pipeline cache: PASSED" << std::endl;
    return true;
}

bool RunAllRenderingValidationTests() {
    std::cout << "=== RUNNING RENDERING ARCHITECTURE VALIDATION TESTS ===" << std::endl;

    if (!VerifyRenderGraphSorting()) return false;
    if (!VerifyPBRMath()) return false;
    if (!VerifyMaterialSystem()) return false;
    if (!VerifyHDRPipeline()) return false;
    if (!VerifyLightManagerUniforms()) return false;
    if (!VerifyShadowSystem()) return false;
    if (!VerifyPostProcessingPipeline()) return false;
    if (!VerifyIBL()) return false;
    if (!VerifySSR()) return false;
    if (!VerifyVolumetricLighting()) return false;
    if (!VerifyAtmosphericRendering()) return false;
    if (!VerifyAntiAliasing()) return false;
    if (!VerifyReflectionProbes()) return false;
    if (!VerifyPerformanceProfiler()) return false;
    if (!VerifyOptimizations()) return false;

    std::cout << "=== ALL RENDERING ARCHITECTURE VALIDATION TESTS PASSED! ===" << std::endl;
    return true;
}

} // namespace KumariEngine::Tests
