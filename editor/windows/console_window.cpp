#include "console_window.hpp"
#include <algorithm>

namespace KumariEngine::Editor {

ConsoleWindow* ConsoleWindow::s_instance = nullptr;

void ConsoleWindow::Initialize() {
    s_instance = this;
    Core::Logger::RegisterLogCallback(ConsoleWindow::OnLogReceived);
    Core::Logger::Info("Editor", "Console Window Initialized and subscribed to Logger.");
}

void ConsoleWindow::Shutdown() {
    Core::Logger::UnregisterLogCallback();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void ConsoleWindow::Update(float deltaTime) {
    (void)deltaTime;
}

void ConsoleWindow::RenderUI() {
    Core::Logger::Info("EditorUI", "=== [Console Window] ===");
    auto filtered = GetFilteredLogs();
    for (const auto& log : filtered) {
        std::string lvlStr = "INFO";
        if (log.level == Core::LogLevel::Warning) lvlStr = "WARN";
        else if (log.level == Core::LogLevel::Error) lvlStr = "ERR ";
        Core::Logger::Info("EditorUI", "  [%s][%s] %s", lvlStr.c_str(), log.category.c_str(), log.message.c_str());
    }
}

void ConsoleWindow::OnLogReceived(Core::LogLevel level, std::string_view category, std::string_view message) {
    if (s_instance) {
        std::lock_guard<std::mutex> lock(s_instance->m_mutex);
        s_instance->m_logs.push_back({level, std::string(category), std::string(message)});
    }
}

void ConsoleWindow::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}

void ConsoleWindow::SetSearchQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchQuery = query;
}

void ConsoleWindow::SetLevelFilter(Core::LogLevel level, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (level) {
        case Core::LogLevel::Info: m_infoEnabled = enabled; break;
        case Core::LogLevel::Warning: m_warnEnabled = enabled; break;
        case Core::LogLevel::Error: m_errorEnabled = enabled; break;
    }
}

std::vector<ConsoleLogEntry> ConsoleWindow::GetFilteredLogs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConsoleLogEntry> filtered;
    
    // Simple helper for case-insensitive search
    auto searchLower = m_searchQuery;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& log : m_logs) {
        // 1. Severity filter
        if (log.level == Core::LogLevel::Info && !m_infoEnabled) continue;
        if (log.level == Core::LogLevel::Warning && !m_warnEnabled) continue;
        if (log.level == Core::LogLevel::Error && !m_errorEnabled) continue;

        // 2. Search query filter
        if (!searchLower.empty()) {
            auto msgLower = log.message;
            std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto catLower = log.category;
            std::transform(catLower.begin(), catLower.end(), catLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (msgLower.find(searchLower) == std::string::npos && 
                catLower.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        filtered.push_back(log);
    }
    return filtered;
}

} // namespace KumariEngine::Editor
