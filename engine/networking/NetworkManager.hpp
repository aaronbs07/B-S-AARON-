#pragma once
#include "networking/Packet.hpp"
#include "networking/ThreadSafeQueue.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <chrono>

// Forward declarations for ENet types to avoid header exposure
struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace KumariEngine::Networking {

enum class NetworkState {
    Inactive,
    Server,
    ClientConnecting,
    ClientConnected,
    ClientFailedToConnect,
    ClientReconnecting
};

struct IncomingNetworkEvent {
    enum class Type {
        Connect,
        Disconnect,
        Data
    };

    Type type;
    uint32_t peerId = 0;
    std::string address;
    uint16_t port = 0;
    std::vector<uint8_t> payload;
};

struct OutgoingNetworkEvent {
    enum class Type {
        Send,
        Broadcast,
        DisconnectPeer
    };

    Type type;
    uint32_t peerId = 0;
    std::vector<uint8_t> payload;
    bool reliable = true;
};

class NetworkManager {
public:
    static NetworkManager& Get() {
        static NetworkManager instance;
        return instance;
    }

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Lifecycle
    bool Initialize();
    void Shutdown();

    // Host Management
    bool StartServer(uint16_t port, size_t maxClients = 32);
    bool Connect(const std::string& hostName, uint16_t port, uint32_t timeoutMs = 5000);
    void Disconnect();
    void DisconnectPeer(uint32_t peerId);

    // Data Transfer (thread-safe, queues outgoing messages)
    void Send(uint32_t peerId, const PacketWriter& writer, bool reliable = true);
    void Broadcast(const PacketWriter& writer, bool reliable = true);

    // Event Polling (thread-safe, pulls from incoming queue)
    bool PollEvent(IncomingNetworkEvent& outEvent);

    // Testing helper to intercept outgoing packets
    bool PopOutgoingEventForTest(OutgoingNetworkEvent& outEvent) {
        return m_outgoingQueue.Pop(outEvent);
    }

    // State & Stats
    NetworkState GetState() const { return m_state.load(); }
    size_t GetConnectedPeerCount() const { return m_peerCount.load(); }

    uint64_t GetPacketsSent() const { return m_packetsSent.load(); }
    uint64_t GetPacketsReceived() const { return m_packetsReceived.load(); }
    uint64_t GetBytesSent() const { return m_bytesSent.load(); }
    uint64_t GetBytesReceived() const { return m_bytesReceived.load(); }
    
    double GetBytesSentRate() const { return m_bytesSentRate.load(); }
    double GetBytesReceivedRate() const { return m_bytesReceivedRate.load(); }
    double GetPacketsSentRate() const { return m_packetsSentRate.load(); }
    double GetPacketsReceivedRate() const { return m_packetsReceivedRate.load(); }
    
    void ResetStats();

private:
    NetworkManager() = default;
    ~NetworkManager();

    // Networking thread entry point
    void NetworkThreadUpdate();
    void ProcessOutgoingQueue();

    // Helper: Map unique Peer ID to ENetPeer*
    ENetPeer* GetPeerByUniqueId(uint32_t peerId) const;

    // ENet State
    ENetHost* m_enetHost = nullptr;
    ENetPeer* m_clientPeer = nullptr; // For client connection tracking
    
    // Address of client peer for logging
    std::string m_clientAddressStr;
    uint16_t m_clientPort = 0;

    // Threading State
    std::thread m_networkThread;
    std::atomic<bool> m_threadRunning{false};
    mutable std::mutex m_peersMutex; // Protects m_peers map if accessed/queried

    // Connection Mapping (Network thread only, except under mutex lock if queried)
    std::unordered_map<uint32_t, ENetPeer*> m_peers;
    uint32_t m_nextPeerId = 0;

    // Queues
    ThreadSafeQueue<IncomingNetworkEvent> m_incomingQueue;
    ThreadSafeQueue<OutgoingNetworkEvent> m_outgoingQueue;

    // Thread-safe statistics
    std::atomic<NetworkState> m_state{NetworkState::Inactive};
    std::atomic<size_t> m_peerCount{0};

    std::atomic<uint64_t> m_packetsSent{0};
    std::atomic<uint64_t> m_packetsReceived{0};
    std::atomic<uint64_t> m_bytesSent{0};
    std::atomic<uint64_t> m_bytesReceived{0};

    // Client Timeout Handling variables (used only on Network Thread)
    std::chrono::steady_clock::time_point m_connectionTimer;
    std::chrono::milliseconds m_timeoutDuration{5000};

    // Client Auto-Reconnection Configuration (used only on Network Thread)
    std::string m_lastHostName;
    uint16_t m_lastPort = 0;
    uint32_t m_lastTimeoutMs = 5000;
    int m_reconnectAttempts = 0;
    std::chrono::steady_clock::time_point m_lastReconnectAttemptTime;

    // Heartbeat & Timeout Tracking (used only on Network Thread)
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_peerLastRecvTimes;

    // Rate calculations
    std::atomic<double> m_bytesSentRate{0.0};
    std::atomic<double> m_bytesReceivedRate{0.0};
    std::atomic<double> m_packetsSentRate{0.0};
    std::atomic<double> m_packetsReceivedRate{0.0};
};

} // namespace KumariEngine::Networking
