#pragma once
#include <string>
#include <string_view>
#include <unordered_set>

namespace KumariEngine::Core {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void Log(LogLevel level, std::string_view category, std::string_view message);

    template<typename... Args>
    static void Info(std::string_view category, std::string_view format, Args&&... args) {
        if (IsCategoryEnabled(category)) {
            Log(LogLevel::Info, category, FormatString(format, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    static void Warning(std::string_view category, std::string_view format, Args&&... args) {
        if (IsCategoryEnabled(category)) {
            Log(LogLevel::Warning, category, FormatString(format, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    static void Error(std::string_view category, std::string_view format, Args&&... args) {
        if (IsCategoryEnabled(category)) {
            Log(LogLevel::Error, category, FormatString(format, std::forward<Args>(args)...));
        }
    }

    // Configure which categories emit logs (debug only). Call once at start.
    static void EnableCategories(const std::unordered_set<std::string>& categories) {
        GetEnabledCategories() = categories;
    }

private:
    static bool IsCategoryEnabled(std::string_view category) {
        const auto& set = GetEnabledCategories();
        return set.empty() || set.find(std::string(category)) != set.end();
    }
    static std::unordered_set<std::string>& GetEnabledCategories() {
        static std::unordered_set<std::string> enabledCategories;
        return enabledCategories;
    }
    static std::string FormatString(std::string_view format, ...);
};

} // namespace KumariEngine::Core
