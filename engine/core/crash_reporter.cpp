#include "crash_reporter.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "ecs/ecs.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <sstream>
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace KumariEngine::Core {

static std::string s_reportDir = "crash_reports";
static std::vector<std::string> s_logHistory;
static ECS::Registry* s_activeRegistry = nullptr;

#ifdef _WIN32
static LPTOP_LEVEL_EXCEPTION_FILTER s_previousFilter = nullptr;
#endif

void CrashReporter::RegisterRegistry(ECS::Registry* registry) {
    s_activeRegistry = registry;
}

void CrashReporter::Initialize(const std::string& reportDir) {
    s_reportDir = reportDir;
    std::filesystem::create_directories(s_reportDir);

    Logger::RegisterLogCallback([](LogLevel level, std::string_view category, std::string_view message) {
        std::string lvlStr;
        switch (level) {
            case LogLevel::Info: lvlStr = "INFO"; break;
            case LogLevel::Warning: lvlStr = "WARN"; break;
            case LogLevel::Error: lvlStr = "ERROR"; break;
        }
        std::stringstream ss;
        ss << "[" << category << "] " << lvlStr << ": " << message;
        AppendLog(ss.str());
    });

#ifdef _WIN32
    s_previousFilter = SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        std::filesystem::path reportPath = std::filesystem::path(s_reportDir) / 
            ("crash_report_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json");
        
        std::ofstream file(reportPath);
        if (file.is_open()) {
            file << "{\n";
            file << "  \"timestamp\": " << std::chrono::system_clock::now().time_since_epoch().count() << ",\n";
            file << "  \"exception_code\": \"0x" << std::hex << ep->ExceptionRecord->ExceptionCode << std::dec << "\",\n";
            file << "  \"exception_address\": \"0x" << std::hex << reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress) << std::dec << "\",\n";
            
            file << "  \"stack_trace\": [\n";
            void* stack[64];
            unsigned short frames = CaptureStackBackTrace(0, 64, stack, NULL);
            HANDLE process = GetCurrentProcess();
            SymInitialize(process, NULL, TRUE);

            char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)];
            SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolBuffer;
            symbol->MaxNameLen = MAX_SYM_NAME;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

            for (unsigned short i = 0; i < frames; i++) {
                DWORD64 address = (DWORD64)(stack[i]);
                file << "    \"";
                if (SymFromAddr(process, address, 0, symbol)) {
                    file << symbol->Name << " [0x" << std::hex << address << std::dec << "]";
                } else {
                    file << "UnknownAddress [0x" << std::hex << address << std::dec << "]";
                }
                file << (i == frames - 1 ? "\"\n" : "\",\n");
            }
            file << "  ],\n";

            size_t entityCount = 0;
            if (s_activeRegistry) {
                entityCount = s_activeRegistry->GetAliveEntities().size();
            }
            file << "  \"active_entities\": " << entityCount << ",\n";

            file << "  \"memory_allocations\": {\n";
            file << "    \"ECS\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::ECS) << ",\n";
            file << "    \"Renderer\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Renderer) << ",\n";
            file << "    \"Audio\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Audio) << ",\n";
            file << "    \"Physics\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Physics) << ",\n";
            file << "    \"Script\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Script) << ",\n";
            file << "    \"Network\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::Network) << ",\n";
            file << "    \"General\": " << Profiler::Get().GetMemoryUsage(MemoryCategory::General) << "\n";
            file << "  },\n";

            file << "  \"log_history\": [\n";
            for (size_t i = 0; i < s_logHistory.size(); ++i) {
                std::string logLine = s_logHistory[i];
                std::string escaped;
                for (char c : logLine) {
                    if (c == '"') escaped += "\\\"";
                    else if (c == '\\') escaped += "\\\\";
                    else if (c == '\n') escaped += "\\n";
                    else if (c == '\r') escaped += "";
                    else escaped += c;
                }
                file << "    \"" << escaped << (i == s_logHistory.size() - 1 ? "\"\n" : "\",\n");
            }
            file << "  ]\n";
            file << "}\n";
            file.close();
            
            std::cerr << "Crash report written to: " << reportPath.string() << std::endl;
        }

        std::cerr << "Kumari Engine FATAL CRASH: Exception Code 0x" 
                  << std::hex << ep->ExceptionRecord->ExceptionCode << std::dec << std::endl;

        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif

    Logger::Info("CrashReporter", "Crash Reporter initialized.");
}

void CrashReporter::Shutdown() {
#ifdef _WIN32
    if (s_previousFilter) {
        SetUnhandledExceptionFilter(s_previousFilter);
        s_previousFilter = nullptr;
    }
#endif
    s_logHistory.clear();
    Logger::UnregisterLogCallback();
    Logger::Info("CrashReporter", "Crash Reporter shut down.");
}

void CrashReporter::AppendLog(const std::string& logLine) {
    s_logHistory.push_back(logLine);
    if (s_logHistory.size() > 100) {
        s_logHistory.erase(s_logHistory.begin());
    }
}

void CrashReporter::TriggerMockCrash() {
    Core::Logger::Info("CrashReporter", "Triggering mock crash for validation...");
    volatile int* ptr = nullptr;
    int val = *ptr;
    (void)val;
}

} // namespace KumariEngine::Core
