#pragma once
#include <string>

namespace KumariEngine::Renderer {

struct MeshRendererComponent {
    std::string meshPath;
    std::string materialPath;
    bool visible = true;
    bool castShadows = true;

    MeshRendererComponent() = default;
    MeshRendererComponent(const std::string& mesh, const std::string& mat = "")
        : meshPath(mesh), materialPath(mat), visible(true), castShadows(true) {}
};

} // namespace KumariEngine::Renderer
