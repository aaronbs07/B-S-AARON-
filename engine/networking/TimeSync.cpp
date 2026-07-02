#include "networking/TimeSync.hpp"
#include "networking/Packet.hpp"
#include "networking/NetworkManager.hpp"
#include "core/logger.hpp"
#include <cmath>

namespace KumariEngine::Networking {

TimeSyncManager::TimeSyncManager() {
    Reset();
}

void TimeSyncManager::Reset() {
    m_latencyMs = 0.0;
    m_jitterMs = 0.0;
    m_clockOffsetMs = 0.0;
    m_avgRttMs = 0.0;
    m_hasOffset = false;
    m_mockLocalTimeMs = -1.0;
}

double TimeSyncManager::GetLocalTimeMs() const {
    if (m_mockLocalTimeMs >= 0.0) {
        return m_mockLocalTimeMs;
    }
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

void TimeSyncManager::ClientSendPing() {
    PacketWriter writer(PacketId::Ping);
    writer.WriteDouble(GetLocalTimeMs());
    NetworkManager::Get().Send(1, writer, false); // Unreliable channel is fine for ping/pong
}

void TimeSyncManager::ServerProcessPing(uint32_t peerId, PacketReader& reader) {
    double clientTimestamp = 0.0;
    if (!reader.ReadDouble(clientTimestamp)) {
        Core::Logger::Warning("TimeSync", "Failed to read client timestamp in ServerProcessPing.");
        return;
    }

    PacketWriter writer(PacketId::Pong);
    writer.WriteDouble(clientTimestamp);
    writer.WriteDouble(GetLocalTimeMs());
    NetworkManager::Get().Send(peerId, writer, false);
}

void TimeSyncManager::ClientProcessPong(PacketReader& reader) {
    double clientTimestamp = 0.0;
    double serverTimestamp = 0.0;
    if (!reader.ReadDouble(clientTimestamp) || !reader.ReadDouble(serverTimestamp)) {
        Core::Logger::Warning("TimeSync", "Failed to read timestamps in ClientProcessPong.");
        return;
    }

    double now = GetLocalTimeMs();
    double rtt = now - clientTimestamp;
    if (rtt < 0.0) rtt = 0.0;

    double latency = rtt / 2.0;
    double offset = serverTimestamp - (clientTimestamp + latency);

    if (!m_hasOffset) {
        m_clockOffsetMs = offset;
        m_avgRttMs = rtt;
        m_latencyMs = latency;
        m_jitterMs = 0.0;
        m_hasOffset = true;
    } else {
        m_clockOffsetMs = (1.0 - m_alpha) * m_clockOffsetMs + m_alpha * offset;
        m_avgRttMs = (1.0 - m_alpha) * m_avgRttMs + m_alpha * rtt;
        double currentJitter = std::abs(rtt - m_avgRttMs);
        m_jitterMs = (1.0 - m_beta) * m_jitterMs + m_beta * currentJitter;
        m_latencyMs = latency;
    }
}

double TimeSyncManager::GetSyncedServerTimeMs() const {
    return GetLocalTimeMs() + m_clockOffsetMs;
}

} // namespace KumariEngine::Networking
