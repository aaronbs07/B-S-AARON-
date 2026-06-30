#pragma once
#include <string>

namespace KumariEngine::Resource {

class Resource {
public:
    Resource() = default;
    virtual ~Resource() = default;

    void SetPath(const std::string& path) { m_path = path; }
    const std::string& GetPath() const { return m_path; }

private:
    std::string m_path;
};

} // namespace KumariEngine::Resource
