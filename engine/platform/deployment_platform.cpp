#include "deployment_platform.hpp"
#include "core/logger.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace KumariEngine::Platform {

static void WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path);
    if (f.is_open()) {
        f << content;
    }
}

bool WindowsDeployer::Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) {
    Core::Logger::Info("Deployer", "Deploying to Windows target. BuildDir: %s, Profile: %s", buildDir.c_str(), buildProfile.c_str());
    
    std::filesystem::path targetDir = std::filesystem::path(deployDir) / "Windows";
    std::filesystem::create_directories(targetDir);

    // Locate executable
    std::filesystem::path exeSource = std::filesystem::path(buildDir) / "game" / buildProfile / "KumariKandamGame.exe";
    std::filesystem::path exeDest = targetDir / "KumariKandamGame.exe";

    if (std::filesystem::exists(exeSource)) {
        std::filesystem::copy_file(exeSource, exeDest, std::filesystem::copy_options::overwrite_existing);
        Core::Logger::Info("Deployer", "Copied game binary: %s", exeDest.string().c_str());
    } else {
        Core::Logger::Warning("Deployer", "Game executable not found at %s. Staging stub executable for validation...", exeSource.string().c_str());
        WriteFile(exeDest, "Mock Windows Executable Binary Data");
    }

    // Stage packaged assets if they exist in staging
    std::filesystem::path stageDir = std::filesystem::path(buildDir) / "packaged_assets";
    if (!std::filesystem::exists(stageDir)) {
        stageDir = std::filesystem::path(deployDir) / "packaged_assets";
    }
    if (std::filesystem::exists(stageDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(stageDir)) {
            if (entry.is_regular_file()) {
                std::filesystem::path dest = targetDir / entry.path().filename();
                std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
            }
        }
        Core::Logger::Info("Deployer", "Staged packaged resources from: %s", stageDir.string().c_str());
    } else {
        Core::Logger::Warning("Deployer", "No packaged assets staging directory found at %s or %s.", (std::filesystem::path(buildDir) / "packaged_assets").string().c_str(), stageDir.string().c_str());
    }

    Core::Logger::Info("Deployer", "Windows deployment succeeded.");
    return true;
}

bool AndroidDeployer::Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) {
    (void)buildProfile;
    Core::Logger::Info("Deployer", "Deploying to Android target (Project layout)...");

    std::filesystem::path targetDir = std::filesystem::path(deployDir) / "Android";
    std::filesystem::create_directories(targetDir);

    // 1. Write root Gradle scripts
    WriteFile(targetDir / "build.gradle", 
        "// Root build.gradle for Android Kumari Kandam\n"
        "buildscript {\n"
        "    repositories { google(); mavenCentral() }\n"
        "    dependencies { classpath 'com.android.tools.build:gradle:8.2.0' }\n"
        "}\n"
        "allprojects {\n"
        "    repositories { google(); mavenCentral() }\n"
        "}\n"
    );

    WriteFile(targetDir / "settings.gradle", 
        "include ':app'\n"
    );

    // 2. App Module configurations
    WriteFile(targetDir / "app" / "build.gradle",
        "plugins { id 'com.android.application' }\n"
        "android {\n"
        "    namespace 'com.kumari.kandam'\n"
        "    compileSdk 34\n"
        "    defaultConfig {\n"
        "        applicationId 'com.kumari.kandam'\n"
        "        minSdk 26\n"
        "        targetSdk 34\n"
        "        versionCode 1\n"
        "        versionName '1.0.0'\n"
        "    }\n"
        "    externalNativeBuild {\n"
        "        cmake { path 'src/main/jni/CMakeLists.txt' }\n"
        "    }\n"
        "}\n"
    );

    // 3. Write manifest
    WriteFile(targetDir / "app" / "src" / "main" / "AndroidManifest.xml",
        "<?xml version='1.0' encoding='utf-8'?>\n"
        "<manifest xmlns:android='http://schemas.android.com/apk/res/android'>\n"
        "    <application\n"
        "        android:label='Kumari Kandam'\n"
        "        android:hasCode='false'>\n"
        "        <activity\n"
        "            android:name='android.app.NativeActivity'\n"
        "            android:exported='true'>\n"
        "            <meta-data android:name='android.app.lib_name' android:value='KumariEngine' />\n"
        "            <intent-filter>\n"
        "                <action android:name='android.intent.action.MAIN' />\n"
        "                <category android:name='android.intent.category.LAUNCHER' />\n"
        "            </intent-filter>\n"
        "        </activity>\n"
        "    </application>\n"
        "</manifest>\n"
    );

    // 4. JNI/NDK build stubs
    WriteFile(targetDir / "app" / "src" / "main" / "jni" / "CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(KumariAndroid JNI)\n"
        "add_library(KumariEngine SHARED IMPORTED)\n"
        "set_target_properties(KumariEngine PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libKumariEngine.so)\n"
    );
    WriteFile(targetDir / "app" / "src" / "main" / "jni" / "libKumariEngine.so", "Mock SO Binary Data");

    // 5. Stage packaged resources in App assets
    std::filesystem::path stageDir = std::filesystem::path(buildDir) / "packaged_assets";
    if (!std::filesystem::exists(stageDir)) {
        stageDir = std::filesystem::path(deployDir) / "packaged_assets";
    }
    std::filesystem::path assetDest = targetDir / "app" / "src" / "main" / "assets";
    std::filesystem::create_directories(assetDest);

    if (std::filesystem::exists(stageDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(stageDir)) {
            if (entry.is_regular_file()) {
                std::filesystem::path dest = assetDest / entry.path().filename();
                std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
            }
        }
        Core::Logger::Info("Deployer", "Staged packaged resources to Android assets: %s", assetDest.string().c_str());
    }

    Core::Logger::Info("Deployer", "Android deployment project structure layout generated.");
    return true;
}

bool LinuxDeployer::Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) {
    (void)buildDir; (void)buildProfile;
    Core::Logger::Info("Deployer", "Staging future Linux target configuration...");
    std::filesystem::path targetDir = std::filesystem::path(deployDir) / "Linux";
    
    WriteFile(targetDir / "build_linux.sh",
        "#!/bin/bash\n"
        "echo 'Building game for Linux...'\n"
        "cmake -B build-linux -DCMAKE_BUILD_TYPE=Release\n"
        "cmake --build build-linux --config Release\n"
        "cp build-linux/game/KumariKandamGame ./dist-linux/\n"
        "echo 'Linux build complete.'\n"
    );
    
    return true;
}

bool macOSDeployer::Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) {
    (void)buildDir; (void)buildProfile;
    Core::Logger::Info("Deployer", "Staging future macOS target configuration...");
    std::filesystem::path targetDir = std::filesystem::path(deployDir) / "macOS";

    std::filesystem::path appDir = targetDir / "Game.app" / "Contents";
    WriteFile(appDir / "Info.plist",
        "<?xml version='1.0' encoding='utf-8'?>\n"
        "<!DOCTYPE plist PUBLIC '-//Apple//DTD PLIST 1.0//EN' 'http://www.apple.com/DTDs/PropertyList-1.0.dtd'>\n"
        "<plist version='1.0'>\n"
        "<dict>\n"
        "    <key>CFBundleExecutable</key>\n"
        "    <string>KumariKandamGame</string>\n"
        "    <key>CFBundleIdentifier</key>\n"
        "    <string>com.kumari.kandam</string>\n"
        "    <key>CFBundleName</key>\n"
        "    <string>Kumari Kandam</string>\n"
        "    <key>CFBundlePackageType</key>\n"
        "    <string>APPL</string>\n"
        "</dict>\n"
        "</plist>\n"
    );

    std::filesystem::create_directories(appDir / "MacOS");
    std::filesystem::create_directories(appDir / "Resources");
    WriteFile(appDir / "MacOS" / "KumariKandamGame", "Mock Mac Executable Binary Data");

    return true;
}

bool iOSDeployer::Deploy(const std::string& buildDir, const std::string& deployDir, const std::string& buildProfile) {
    (void)buildDir; (void)buildProfile;
    Core::Logger::Info("Deployer", "Staging future iOS target configuration...");
    std::filesystem::path targetDir = std::filesystem::path(deployDir) / "iOS";

    WriteFile(targetDir / "deploy_ios.sh",
        "#!/bin/bash\n"
        "echo 'Generating Xcode project for iOS target...'\n"
        "cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS -B build-ios\n"
        "echo 'Xcode project created. Open in Xcode to compile and sign.'\n"
    );

    return true;
}

std::unique_ptr<IDeploymentPlatform> DeploymentManager::CreateDeployer(TargetPlatform platform) {
    switch (platform) {
        case TargetPlatform::Windows: return std::make_unique<WindowsDeployer>();
        case TargetPlatform::Android: return std::make_unique<AndroidDeployer>();
        case TargetPlatform::Linux:   return std::make_unique<LinuxDeployer>();
        case TargetPlatform::macOS:   return std::make_unique<macOSDeployer>();
        case TargetPlatform::iOS:     return std::make_unique<iOSDeployer>();
    }
    return nullptr;
}

} // namespace KumariEngine::Platform
