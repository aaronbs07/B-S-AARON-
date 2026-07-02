#pragma once
#include <string>
#include <memory>
#include <vector>

namespace KumariEngine::Platform {

enum class TargetPlatform {
    Windows,
    Android,
    Linux,
    macOS,
    iOS
};

class IDeploymentPlatform {
public:
    virtual ~IDeploymentPlatform() = default;
    virtual std::string GetName() const = 0;
    virtual bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) = 0;
};

class WindowsDeployer : public IDeploymentPlatform {
public:
    std::string GetName() const override { return "Windows"; }
    bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) override;
};

class AndroidDeployer : public IDeploymentPlatform {
public:
    std::string GetName() const override { return "Android"; }
    bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) override;
};

class LinuxDeployer : public IDeploymentPlatform {
public:
    std::string GetName() const override { return "Linux"; }
    bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) override;
};

class macOSDeployer : public IDeploymentPlatform {
public:
    std::string GetName() const override { return "macOS"; }
    bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) override;
};

class iOSDeployer : public IDeploymentPlatform {
public:
    std::string GetName() const override { return "iOS"; }
    bool Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) override;
};

class DeploymentManager {
public:
    static std::unique_ptr<IDeploymentPlatform> CreateDeployer(TargetPlatform platform);
};

} // namespace KumariEngine::Platform
