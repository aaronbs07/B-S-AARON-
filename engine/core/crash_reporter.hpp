#pragma once
#include <string>

namespace KumariEngine::ECS { class Registry; }

namespace KumariEngine::Core {

class CrashReporter {
public:
    static void Initialize(const std::string& reportDir = "crash_reports");
    static void Shutdown();

    static void RegisterRegistry(ECS::Registry* registry);
    static void AppendLog(const std::string& logLine);
    static void TriggerMockCrash();
};

} // namespace KumariEngine::Core
