#pragma once
#include "resource/resource.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace KumariEngine::Resource {

class TextureAsset : public Resource {
public:
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixelData;
};

class ModelAsset : public Resource {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
    };
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class MaterialAsset : public Resource {
public:
    glm::vec3 albedoColor{1.0f};
    std::string albedoTextureGuid;
    float metallic = 0.0f;
    float roughness = 0.5f;
};

class AudioAsset : public Resource {
public:
    float duration = 0.0f;
    int sampleRate = 0;
    int channels = 0;
    std::vector<uint8_t> audioData;
};

class FontAsset : public Resource {
public:
    std::string fontName;
    int size = 12;
};

class LuaScriptAsset : public Resource {
public:
    std::string sourceCode;
};

class SceneAsset : public Resource {
public:
    std::string sceneName;
    std::string serializedData;
};

} // namespace KumariEngine::Resource
