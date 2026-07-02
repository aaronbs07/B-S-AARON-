#pragma once
#include "editor/window_system.hpp"
#include "core/logger.hpp"
#include <vector>
#include <string>
#include <mutex>

namespace KumariEngine::Editor {

struct ConsoleLogEntry {
    Core::LogLevel level;
    std::string category;
    std::string message;
};

class ConsoleWindow : public EditorWindow {
public:
    ConsoleWindow() : EditorWindow("Console") {}

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

    static void OnLogReceived(Core::LogLevel level, std::string_view category, std::string_view message);

    void Clear();
    void SetSearchQuery(const std::string& query);
    void SetLevelFilter(Core::LogLevel level, bool enabled);

    const std::vector<ConsoleLogEntry>& GetLogs() const { return m_logs; }
    std::vector<ConsoleLogEntry> GetFilteredLogs() const;

private:
    static ConsoleWindow* s_instance;

    std::vector<ConsoleLogEntry> m_logs;
    std::string m_searchQuery = "";
    bool m_infoEnabled = true;
    bool m_warnEnabled = true;
    bool m_errorEnabled = true;
    mutable std::mutex m_mutex;
};

} // namespace KumariEngine::Editor
