#pragma once
#include "resource/resource.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace KumariEngine::Renderer {

enum class ShaderParamType {
    Float,
    Vec3,
    Texture
};

struct ShaderParameter {
    std::string name;
    ShaderParamType type;
    std::string description;
};

class Material : public Resource::Resource {
public:
    Material(const std::string& name, std::shared_ptr<Material> parent = nullptr);
    ~Material() override = default;

    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetTexture(const std::string& name, const std::string& textureGuid);

    float GetFloat(const std::string& name) const;
    glm::vec3 GetVec3(const std::string& name) const;
    std::string GetTexture(const std::string& name) const;

    bool HasFloat(const std::string& name) const;
    bool HasVec3(const std::string& name) const;
    bool HasTexture(const std::string& name) const;

    std::shared_ptr<Material> GetParent() const { return m_parent; }
    const std::string& GetName() const { return m_name; }

    std::vector<ShaderParameter> GetParametersReflection() const;

private:
    std::string m_name;
    std::shared_ptr<Material> m_parent;

    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, glm::vec3> m_vec3s;
    std::unordered_map<std::string, std::string> m_textures;
};

} // namespace KumariEngine::Renderer
