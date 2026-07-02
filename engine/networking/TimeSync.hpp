#pragma once
#include <cstdint>
#include <chrono>
#include <vector>

namespace KumariEngine::Networking {

class PacketReader;

class TimeSyncManager {
public:
    static TimeSyncManager& Get() {
        static TimeSyncManager instance;
        return instance;
    }

    TimeSyncManager(const TimeSyncManager&) = delete;
    TimeSyncManager& operator=(const TimeSyncManager&) = delete;

    // Initialize or Reset clock synchronization stats
    void Reset();

    // Client-side: initiates a ping request to the server
    void ClientSendPing();

    // Server-side: processes ping from a client and responds with a pong
    void ServerProcessPing(uint32_t peerId, PacketReader& reader);

    // Client-side: processes pong from the server to update time synchronization parameters
    void ClientProcessPong(PacketReader& reader);

    // Client-side: returns the current synchronized server time in milliseconds
    double GetSyncedServerTimeMs() const;

    // Accessors for statistics
    double GetLatencyMs() const { return m_latencyMs; }
    double GetJitterMs() const { return m_jitterMs; }
    double GetClockOffsetMs() const { return m_clockOffsetMs; }

    // Test-only mock time control
    void SetMockLocalTimeMs(double timeMs) { m_mockLocalTimeMs = timeMs; }

private:
    TimeSyncManager();
    ~TimeSyncManager() = default;

    // Helper to get local time in milliseconds
    double GetLocalTimeMs() const;

    double m_latencyMs = 0.0;
    double m_jitterMs = 0.0;
    double m_clockOffsetMs = 0.0;
    bool m_hasOffset = false;

    // Alpha/Beta parameters for smoothing (EMA filters)
    const double m_alpha = 0.1; // Weight for offset smoothing
    const double m_beta = 0.1;  // Weight for jitter smoothing

    // For jitter calculations
    double m_avgRttMs = 0.0;
    double m_mockLocalTimeMs = -1.0;
};

} // namespace KumariEngine::Networking
