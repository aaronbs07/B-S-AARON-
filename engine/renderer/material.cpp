#include "material.hpp"
#include <stdexcept>

namespace KumariEngine::Renderer {

Material::Material(const std::string& name, std::shared_ptr<Material> parent)
    : m_name(name), m_parent(parent) {
    if (m_parent == nullptr) {
        // Base PBR material defaults
        m_floats["metallic"] = 0.0f;
        m_floats["roughness"] = 0.5f;
        m_floats["ao"] = 1.0f;
        m_vec3s["albedo"] = glm::vec3(1.0f);
        m_vec3s["emissive"] = glm::vec3(0.0f);
        m_textures["albedoMap"] = "";
        m_textures["metallicMap"] = "";
        m_textures["roughnessMap"] = "";
        m_textures["normalMap"] = "";
    }
}

void Material::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
}

void Material::SetVec3(const std::string& name, const glm::vec3& value) {
    m_vec3s[name] = value;
}

void Material::SetTexture(const std::string& name, const std::string& textureGuid) {
    m_textures[name] = textureGuid;
}

float Material::GetFloat(const std::string& name) const {
    auto it = m_floats.find(name);
    if (it != m_floats.end()) {
        return it->second;
    }
    if (m_parent != nullptr) {
        return m_parent->GetFloat(name);
    }
    return 0.0f;
}

glm::vec3 Material::GetVec3(const std::string& name) const {
    auto it = m_vec3s.find(name);
    if (it != m_vec3s.end()) {
        return it->second;
    }
    if (m_parent != nullptr) {
        return m_parent->GetVec3(name);
    }
    return glm::vec3(0.0f);
}

std::string Material::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    if (m_parent != nullptr) {
        return m_parent->GetTexture(name);
    }
    return "";
}

bool Material::HasFloat(const std::string& name) const {
    if (m_floats.find(name) != m_floats.end()) return true;
    return m_parent ? m_parent->HasFloat(name) : false;
}

bool Material::HasVec3(const std::string& name) const {
    if (m_vec3s.find(name) != m_vec3s.end()) return true;
    return m_parent ? m_parent->HasVec3(name) : false;
}

bool Material::HasTexture(const std::string& name) const {
    if (m_textures.find(name) != m_textures.end()) return true;
    return m_parent ? m_parent->HasTexture(name) : false;
}

std::vector<ShaderParameter> Material::GetParametersReflection() const {
    std::vector<ShaderParameter> reflection;

    // Collate parameters from parent first (base settings)
    if (m_parent != nullptr) {
        reflection = m_parent->GetParametersReflection();
    }

    // Add/override local parameters
    auto addOrOverride = [&](const std::string& name, ShaderParamType type, const std::string& desc) {
        for (auto& param : reflection) {
            if (param.name == name) {
                param.type = type;
                return;
            }
        }
        reflection.push_back({name, type, desc});
    };

    for (const auto& [name, _] : m_floats) {
        addOrOverride(name, ShaderParamType::Float, "Float scalar parameter");
    }
    for (const auto& [name, _] : m_vec3s) {
        addOrOverride(name, ShaderParamType::Vec3, "Vector3 parameter");
    }
    for (const auto& [name, _] : m_textures) {
        addOrOverride(name, ShaderParamType::Texture, "Texture slot / GUID binding");
    }

    return reflection;
}

} // namespace KumariEngine::Renderer
