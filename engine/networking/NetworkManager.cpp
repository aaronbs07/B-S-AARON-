#include "networking/NetworkManager.hpp"
#include "core/logger.hpp"
#include <enet/enet.h>
#include <cassert>
#include <sstream>

namespace KumariEngine::Networking {

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize() {
    Core::Logger::Info("Networking", "Initializing ENet transport layer...");
    if (enet_initialize() != 0) {
        Core::Logger::Error("Networking", "Failed to initialize ENet.");
        return false;
    }
    ResetStats();
    m_state = NetworkState::Inactive;
    return true;
}

void NetworkManager::Shutdown() {
    if (m_state.load() == NetworkState::Inactive && !m_networkThread.joinable()) {
        return;
    }
    
    Core::Logger::Info("Networking", "Shutting down networking...");
    
    // Stop thread first
    m_threadRunning = false;
    if (m_networkThread.joinable()) {
        m_networkThread.join();
    }
    
    // Disconnect peers and destroy host
    if (m_enetHost) {
        m_clientPeer = nullptr;
        
        // Clean disconnect active peers
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto& [id, peer] : m_peers) {
                enet_peer_disconnect_now(peer, 0);
            }
            m_peers.clear();
        }
        
        enet_host_destroy(m_enetHost);
        m_enetHost = nullptr;
    }
    
    // Deinitialize queues
    m_incomingQueue.Clear();
    m_outgoingQueue.Clear();
    
    m_state = NetworkState::Inactive;
    m_peerCount = 0;
    
    enet_deinitialize();
    Core::Logger::Info("Networking", "Networking shutdown cleanly.");
}

bool NetworkManager::StartServer(uint16_t port, size_t maxClients) {
    if (m_state.load() != NetworkState::Inactive) {
        Core::Logger::Warning("Networking", "NetworkManager already active, cannot start server.");
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    m_enetHost = enet_host_create(&address, maxClients, 2, 0, 0);
    if (!m_enetHost) {
        Core::Logger::Error("Networking", "Failed to create ENet server host on port %u.", port);
        return false;
    }

    m_nextPeerId = 0;
    m_state = NetworkState::Server;
    m_peerCount = 0;
    m_threadRunning = true;
    
    m_networkThread = std::thread(&NetworkManager::NetworkThreadUpdate, this);
    
    Core::Logger::Info("Networking", "Server started. Listening on port %u.", port);
    return true;
}

bool NetworkManager::Connect(const std::string& hostName, uint16_t port, uint32_t timeoutMs) {
    if (m_state.load() != NetworkState::Inactive) {
        Core::Logger::Warning("Networking", "NetworkManager already active, cannot connect client.");
        return false;
    }

    m_enetHost = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!m_enetHost) {
        Core::Logger::Error("Networking", "Failed to create ENet client host.");
        return false;
    }

    ENetAddress address;
    if (enet_address_set_host(&address, hostName.c_str()) < 0) {
        Core::Logger::Error("Networking", "Failed to resolve address: %s", hostName.c_str());
        enet_host_destroy(m_enetHost);
        m_enetHost = nullptr;
        return false;
    }
    address.port = port;

    m_clientAddressStr = hostName;
    m_clientPort = port;

    ENetPeer* peer = enet_host_connect(m_enetHost, &address, 2, 0);
    if (!peer) {
        Core::Logger::Error("Networking", "Failed to initiate ENet connection to %s:%u", hostName.c_str(), port);
        enet_host_destroy(m_enetHost);
        m_enetHost = nullptr;
        return false;
    }

    m_lastHostName = hostName;
    m_lastPort = port;
    m_lastTimeoutMs = timeoutMs;
    m_reconnectAttempts = 0;

    m_nextPeerId = 0;
    m_clientPeer = peer;
    m_state = NetworkState::ClientConnecting;
    m_peerCount = 0;
    
    // Set timeout timer
    m_connectionTimer = std::chrono::steady_clock::now();
    m_timeoutDuration = std::chrono::milliseconds(timeoutMs);

    m_threadRunning = true;
    m_networkThread = std::thread(&NetworkManager::NetworkThreadUpdate, this);

    Core::Logger::Info("Networking", "Connecting to server %s:%u (timeout: %u ms)...", hostName.c_str(), port, timeoutMs);
    return true;
}

void NetworkManager::Disconnect() {
    NetworkState state = m_state.load();
    if (state == NetworkState::Inactive) return;

    Core::Logger::Info("Networking", "Requesting network disconnection...");
    
    if (state == NetworkState::ClientConnected || state == NetworkState::ClientConnecting) {
        OutgoingNetworkEvent ev;
        ev.type = OutgoingNetworkEvent::Type::DisconnectPeer;
        ev.peerId = 1; // client peer is always ID 1
        m_outgoingQueue.Push(std::move(ev));
    } else if (state == NetworkState::Server) {
        // Disconnect all clients
        std::lock_guard<std::mutex> lock(m_peersMutex);
        for (auto& [id, peer] : m_peers) {
            OutgoingNetworkEvent ev;
            ev.type = OutgoingNetworkEvent::Type::DisconnectPeer;
            ev.peerId = id;
            m_outgoingQueue.Push(std::move(ev));
        }
    }
}
void NetworkManager::DisconnectPeer(uint32_t peerId) {
    OutgoingNetworkEvent ev;
    ev.type = OutgoingNetworkEvent::Type::DisconnectPeer;
    ev.peerId = peerId;
    m_outgoingQueue.Push(std::move(ev));
}

void NetworkManager::Send(uint32_t peerId, const PacketWriter& writer, bool reliable) {
    OutgoingNetworkEvent ev;
    ev.type = OutgoingNetworkEvent::Type::Send;
    ev.peerId = peerId;
    ev.reliable = reliable;
    ev.payload = writer.GetBuffer();
    m_outgoingQueue.Push(std::move(ev));
}

void NetworkManager::Broadcast(const PacketWriter& writer, bool reliable) {
    OutgoingNetworkEvent ev;
    ev.type = OutgoingNetworkEvent::Type::Broadcast;
    ev.peerId = 0;
    ev.reliable = reliable;
    ev.payload = writer.GetBuffer();
    m_outgoingQueue.Push(std::move(ev));
}

bool NetworkManager::PollEvent(IncomingNetworkEvent& outEvent) {
    return m_incomingQueue.Pop(outEvent);
}

void NetworkManager::ResetStats() {
    m_packetsSent = 0;
    m_packetsReceived = 0;
    m_bytesSent = 0;
    m_bytesReceived = 0;
    m_bytesSentRate = 0.0;
    m_bytesReceivedRate = 0.0;
    m_packetsSentRate = 0.0;
    m_packetsReceivedRate = 0.0;
}

ENetPeer* NetworkManager::GetPeerByUniqueId(uint32_t peerId) const {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    auto it = m_peers.find(peerId);
    if (it != m_peers.end()) {
        return it->second;
    }
    return nullptr;
}

void NetworkManager::ProcessOutgoingQueue() {
    OutgoingNetworkEvent outEv;
    while (m_outgoingQueue.Pop(outEv)) {
        if (!m_enetHost) continue;

        if (outEv.type == OutgoingNetworkEvent::Type::Broadcast) {
            ENetPacket* packet = enet_packet_create(outEv.payload.data(), outEv.payload.size(),
                outEv.reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            if (packet) {
                m_packetsSent++;
                m_bytesSent += outEv.payload.size();
                enet_host_broadcast(m_enetHost, 0, packet);
            }
        } else if (outEv.type == OutgoingNetworkEvent::Type::Send) {
            ENetPeer* peer = GetPeerByUniqueId(outEv.peerId);
            if (peer) {
                ENetPacket* packet = enet_packet_create(outEv.payload.data(), outEv.payload.size(),
                    outEv.reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
                if (packet) {
                    m_packetsSent++;
                    m_bytesSent += outEv.payload.size();
                    enet_peer_send(peer, 0, packet);
                }
            }
        } else if (outEv.type == OutgoingNetworkEvent::Type::DisconnectPeer) {
            ENetPeer* peer = GetPeerByUniqueId(outEv.peerId);
            if (peer) {
                enet_peer_disconnect(peer, 0);
            }
        }
    }
}

void NetworkManager::NetworkThreadUpdate() {
    ENetEvent event;
    auto lastRateTime = std::chrono::steady_clock::now();
    uint64_t lastSentBytes = 0;
    uint64_t lastRecvBytes = 0;
    uint64_t lastSentPackets = 0;
    uint64_t lastRecvPackets = 0;

    auto lastKeepAliveCheck = std::chrono::steady_clock::now();

    while (m_threadRunning.load()) {
        bool serviced = false;
        
        // 1. Calculate transmission rates once per second
        auto rateNow = std::chrono::steady_clock::now();
        if (rateNow - lastRateTime >= std::chrono::seconds(1)) {
            double elapsed = std::chrono::duration<double>(rateNow - lastRateTime).count();
            if (elapsed > 0.0) {
                uint64_t sentBytes = m_bytesSent.load();
                uint64_t recvBytes = m_bytesReceived.load();
                uint64_t sentPackets = m_packetsSent.load();
                uint64_t recvPackets = m_packetsReceived.load();

                m_bytesSentRate.store(static_cast<double>(sentBytes - lastSentBytes) / elapsed);
                m_bytesReceivedRate.store(static_cast<double>(recvBytes - lastRecvBytes) / elapsed);
                m_packetsSentRate.store(static_cast<double>(sentPackets - lastSentPackets) / elapsed);
                m_packetsReceivedRate.store(static_cast<double>(recvPackets - lastRecvPackets) / elapsed);

                lastSentBytes = sentBytes;
                lastRecvBytes = recvBytes;
                lastSentPackets = sentPackets;
                lastRecvPackets = recvPackets;
            }
            lastRateTime = rateNow;
        }

        // 2. Client Auto-Reconnection state logic
        if (m_state.load() == NetworkState::ClientReconnecting) {
            auto reconnectNow = std::chrono::steady_clock::now();
            if (reconnectNow - m_lastReconnectAttemptTime >= std::chrono::seconds(2)) {
                m_lastReconnectAttemptTime = reconnectNow;
                if (m_reconnectAttempts >= 3) {
                    Core::Logger::Error("Networking", "Auto-reconnect failed after 3 attempts. Aborting.");
                    m_state = NetworkState::ClientFailedToConnect;
                    
                    IncomingNetworkEvent incEv;
                    incEv.type = IncomingNetworkEvent::Type::Disconnect;
                    incEv.peerId = 1;
                    m_incomingQueue.Push(std::move(incEv));
                } else {
                    m_reconnectAttempts++;
                    Core::Logger::Info("Networking", "Auto-reconnect attempt %d/3...", m_reconnectAttempts);
                    
                    if (m_clientPeer) {
                        enet_peer_reset(m_clientPeer);
                        m_clientPeer = nullptr;
                    }
                    if (m_enetHost) {
                        enet_host_destroy(m_enetHost);
                    }
                    
                    m_enetHost = enet_host_create(nullptr, 1, 2, 0, 0);
                    if (m_enetHost) {
                        ENetAddress address;
                        if (enet_address_set_host(&address, m_lastHostName.c_str()) >= 0) {
                            address.port = m_lastPort;
                            m_clientPeer = enet_host_connect(m_enetHost, &address, 2, 0);
                            if (m_clientPeer) {
                                m_clientPeer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(1));
                                m_connectionTimer = reconnectNow;
                                m_timeoutDuration = std::chrono::milliseconds(m_lastTimeoutMs);
                            }
                        }
                    }
                }
            }
        }

        // 3. Service ENet events
        if (m_enetHost) {
            int serviceResult = enet_host_service(m_enetHost, &event, 10);
            if (serviceResult > 0) {
                serviced = true;
                
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT: {
                        uint32_t assignedId = 0;
                        {
                            std::lock_guard<std::mutex> lock(m_peersMutex);
                            if (m_state.load() == NetworkState::ClientConnecting || m_state.load() == NetworkState::ClientReconnecting) {
                                bool wasReconnecting = (m_state.load() == NetworkState::ClientReconnecting);
                                m_state = NetworkState::ClientConnected;
                                m_clientPeer = event.peer;
                                assignedId = 1;
                                m_peers[1] = event.peer;
                                event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(1));
                                m_peerCount = 1;
                                m_peerLastRecvTimes[1] = std::chrono::steady_clock::now();
                                if (wasReconnecting) {
                                    Core::Logger::Info("Networking", "Auto-reconnect successful!");
                                } else {
                                    Core::Logger::Info("Networking", "Client connection established with server.");
                                }
                            } else if (m_state.load() == NetworkState::Server) {
                                assignedId = ++m_nextPeerId;
                                m_peers[assignedId] = event.peer;
                                event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(assignedId));
                                m_peerCount = m_peers.size();
                                m_peerLastRecvTimes[assignedId] = std::chrono::steady_clock::now();
                                char ip[64];
                                enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip));
                                Core::Logger::Info("Networking", "Server accepted client connection from %s:%u. Assigned Peer ID: %u", 
                                    ip, event.peer->address.port, assignedId);
                            }
                        }
                        
                        IncomingNetworkEvent incEv;
                        incEv.type = IncomingNetworkEvent::Type::Connect;
                        incEv.peerId = assignedId;
                        char ip[64] = "unknown";
                        if (event.peer) {
                            enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip));
                            incEv.port = event.peer->address.port;
                        }
                        incEv.address = ip;
                        m_incomingQueue.Push(std::move(incEv));
                        break;
                    }
                    
                    case ENET_EVENT_TYPE_DISCONNECT: {
                        uint32_t disconnectedId = 0;
                        if (event.peer && event.peer->data) {
                            disconnectedId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
                        }
                        
                        bool initiateReconnecting = false;
                        {
                            std::lock_guard<std::mutex> lock(m_peersMutex);
                            m_peers.erase(disconnectedId);
                            m_peerLastRecvTimes.erase(disconnectedId);
                            if (m_state.load() == NetworkState::ClientConnected) {
                                if (disconnectedId == 1 || m_clientPeer == event.peer) {
                                    initiateReconnecting = true;
                                    m_state = NetworkState::ClientReconnecting;
                                    m_clientPeer = nullptr;
                                    m_reconnectAttempts = 0;
                                    m_lastReconnectAttemptTime = std::chrono::steady_clock::now() - std::chrono::seconds(5); // retry immediately
                                    Core::Logger::Warning("Networking", "Connection to server lost. Entering auto-reconnect mode...");
                                }
                            } else if (m_state.load() == NetworkState::ClientConnecting || m_state.load() == NetworkState::ClientReconnecting) {
                                if (m_clientPeer == event.peer) {
                                    m_clientPeer = nullptr;
                                }
                            } else if (m_state.load() == NetworkState::Server) {
                                m_peerCount = m_peers.size();
                            }
                        }
                        
                        if (!initiateReconnecting) {
                            Core::Logger::Info("Networking", "Peer %u disconnected.", disconnectedId);
                            
                            IncomingNetworkEvent incEv;
                            incEv.type = IncomingNetworkEvent::Type::Disconnect;
                            incEv.peerId = disconnectedId;
                            m_incomingQueue.Push(std::move(incEv));
                        }
                        
                        if (event.peer) {
                            event.peer->data = nullptr;
                        }
                        break;
                    }
                    
                    case ENET_EVENT_TYPE_RECEIVE: {
                        uint32_t senderId = 0;
                        if (event.peer && event.peer->data) {
                            senderId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
                        }
                        
                        m_packetsReceived++;
                        m_peerLastRecvTimes[senderId] = std::chrono::steady_clock::now();
                        if (event.packet) {
                            m_bytesReceived += event.packet->dataLength;
                            
                            // Oversized packet security guard
                            if (event.packet->dataLength > 65536) {
                                Core::Logger::Warning("Security", "Suspicious Activity: Rejected oversized packet from peer %u. Size: %zu bytes.", senderId, event.packet->dataLength);
                            } else {
                                IncomingNetworkEvent incEv;
                                incEv.type = IncomingNetworkEvent::Type::Data;
                                incEv.peerId = senderId;
                                char ip[64] = "unknown";
                                if (event.peer) {
                                    enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip));
                                    incEv.port = event.peer->address.port;
                                }
                                incEv.address = ip;
                                incEv.payload.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                                m_incomingQueue.Push(std::move(incEv));
                            }
                            
                            enet_packet_destroy(event.packet);
                        }
                        break;
                    }
                    
                    case ENET_EVENT_TYPE_NONE:
                        break;
                }
            } else if (serviceResult < 0) {
                Core::Logger::Error("Networking", "Error servicing ENet host.");
            }
        }
        
        // 4. Heartbeat (Ping) and Connection Stall/Timeout Check
        auto keepAliveNow = std::chrono::steady_clock::now();
        if (keepAliveNow - lastKeepAliveCheck >= std::chrono::milliseconds(500)) {
            lastKeepAliveCheck = keepAliveNow;
            std::vector<uint32_t> peersToDisconnect;
            
            for (auto& [peerId, lastRecvTime] : m_peerLastRecvTimes) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(keepAliveNow - lastRecvTime).count();
                if (elapsed >= 10) {
                    Core::Logger::Warning("Networking", "Stall: Peer %u timed out (no packets for 10s).", peerId);
                    peersToDisconnect.push_back(peerId);
                } else if (elapsed >= 5) {
                    // Send Keep-Alive Ping
                    ENetPeer* peer = GetPeerByUniqueId(peerId);
                    if (peer) {
                        PacketWriter ping(PacketId::Ping);
                        ENetPacket* packet = enet_packet_create(ping.GetData(), ping.GetSize(), 0);
                        if (packet) {
                            m_packetsSent++;
                            m_bytesSent += ping.GetSize();
                            enet_peer_send(peer, 0, packet);
                        }
                    }
                    lastRecvTime = keepAliveNow - std::chrono::seconds(2); // reset slightly to avoid spamming
                }
            }
            
            for (uint32_t peerId : peersToDisconnect) {
                ENetPeer* peer = GetPeerByUniqueId(peerId);
                if (peer) {
                    enet_peer_disconnect_later(peer, 0);
                }
                
                m_peerLastRecvTimes.erase(peerId);
                
                bool initiateReconnecting = false;
                {
                    std::lock_guard<std::mutex> lock(m_peersMutex);
                    m_peers.erase(peerId);
                    if (m_state.load() == NetworkState::ClientConnected) {
                        initiateReconnecting = true;
                        m_state = NetworkState::ClientReconnecting;
                        m_clientPeer = nullptr;
                        m_reconnectAttempts = 0;
                        m_lastReconnectAttemptTime = keepAliveNow - std::chrono::seconds(5);
                        Core::Logger::Warning("Networking", "Connection to server stalled. Entering auto-reconnect mode...");
                    } else if (m_state.load() == NetworkState::Server) {
                        m_peerCount = m_peers.size();
                    }
                }
                
                if (!initiateReconnecting) {
                    IncomingNetworkEvent incEv;
                    incEv.type = IncomingNetworkEvent::Type::Disconnect;
                    incEv.peerId = peerId;
                    m_incomingQueue.Push(std::move(incEv));
                }
            }
        }

        // 5. Client connecting/reconnecting timeout check
        if (m_state.load() == NetworkState::ClientConnecting || m_state.load() == NetworkState::ClientReconnecting) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_connectionTimer);
            if (elapsed >= m_timeoutDuration) {
                if (m_state.load() == NetworkState::ClientConnecting) {
                    Core::Logger::Warning("Networking", "Connection timed out after %u ms", m_timeoutDuration.count());
                    
                    {
                        std::lock_guard<std::mutex> lock(m_peersMutex);
                        if (m_clientPeer) {
                            enet_peer_reset(m_clientPeer);
                            m_clientPeer = nullptr;
                        }
                        m_peers.erase(1);
                        m_state = NetworkState::ClientFailedToConnect;
                        m_peerCount = 0;
                    }
                    
                    IncomingNetworkEvent timeoutEv;
                    timeoutEv.type = IncomingNetworkEvent::Type::Disconnect;
                    timeoutEv.peerId = 1;
                    timeoutEv.address = m_clientAddressStr;
                    timeoutEv.port = m_clientPort;
                    m_incomingQueue.Push(std::move(timeoutEv));
                } else {
                    Core::Logger::Warning("Networking", "Reconnection attempt timed out.");
                    if (m_clientPeer) {
                        enet_peer_reset(m_clientPeer);
                        m_clientPeer = nullptr;
                    }
                }
            }
        }
        
        // 6. Process outgoing packets from queue & Flush once
        ProcessOutgoingQueue();
        if (m_enetHost) {
            enet_host_flush(m_enetHost);
        }
        
        if (!serviced) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

} // namespace KumariEngine::Networking
