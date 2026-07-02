#include "logger.hpp"
#include <iostream>
#include <cstdarg>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace KumariEngine::Core {

static Logger::LogCallback s_logCallback = nullptr;

void Logger::RegisterLogCallback(Logger::LogCallback callback) {
    s_logCallback = callback;
}

void Logger::UnregisterLogCallback() {
    s_logCallback = nullptr;
}

void Logger::Log(LogLevel level, std::string_view category, std::string_view message) {
    if (s_logCallback) {
        s_logCallback(level, category, message);
    }
    auto t = std::time(nullptr);
    struct tm timeinfo;
#if defined(_MSC_VER)
    localtime_s(&timeinfo, &t);
#else
    localtime_r(&t, &timeinfo);
#endif

    std::string_view levelStr;
    std::string_view colorCode;

    switch (level) {
        case LogLevel::Info:
            levelStr = "INFO";
            colorCode = "\033[32m"; // Green
            break;
        case LogLevel::Warning:
            levelStr = "WARN";
            colorCode = "\033[33m"; // Yellow
            break;
        case LogLevel::Error:
            levelStr = "ERROR";
            colorCode = "\033[31m"; // Red
            break;
    }

    std::ostringstream timeStream;
    timeStream << std::put_time(&timeinfo, "%H:%M:%S");

    // Output formatted log to console
    std::cout << colorCode << "[" << timeStream.str() << "] [" 
              << levelStr << "] [" << category << "] " << message << "\033[0m\n";
}

std::string Logger::FormatString(std::string_view format, ...) {
    va_list args;
    va_start(args, format);
    
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format.data(), argsCopy);
    va_end(argsCopy);

    if (size <= 0) {
        va_end(args);
        return "";
    }

    std::vector<char> buf(size + 1);
    std::vsnprintf(buf.data(), buf.size(), format.data(), args);
    va_end(args);

    return std::string(buf.data(), static_cast<size_t>(size));
}

} // namespace KumariEngine::Core
