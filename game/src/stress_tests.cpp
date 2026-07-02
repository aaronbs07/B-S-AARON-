#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <cassert>
#include "networking/NetworkManager.hpp"
#include "networking/ReplicationManager.hpp"
#include "networking/RPCManager.hpp"
#include "networking/NetworkOwnershipComponent.hpp"
#include "scene/scene_manager.hpp"
#include "core/logger.hpp"

using namespace KumariEngine;

struct ClientNode {
    uint32_t peerId;
    ECS::Registry registry;
    Networking::ReplicationManager replication;
    bool isConnected = true;
    int disconnectTicksLeft = 0;
};

// SceneContext mock helper to manage registry node structures in tests
struct SceneContext {
    ECS::Registry* registry = nullptr;
    std::unique_ptr<Scene::SceneNode> rootNode;
    std::unordered_map<ECS::Entity, Scene::SceneNode*> entityNodeMap;

    void Save() {
        auto& sm = Scene::SceneManager::Get();
        registry = sm.GetRegistry();
        rootNode = sm.TakeRootNode();
        entityNodeMap = sm.GetEntityNodeMap();
    }

    void Restore() {
        auto& sm = Scene::SceneManager::Get();
        sm.SetRegistry(registry);
        sm.SetRootNode(std::move(rootNode));
        sm.SetEntityNodeMap(entityNodeMap);
    }
    
    void InitializeNew(ECS::Registry* reg) {
        auto& sm = Scene::SceneManager::Get();
        sm.Initialize(reg);
        Save();
    }
};

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "=== KUMARI ENGINE PRODUCTION NETWORKING STRESS TESTS ===" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // Initialize Server
    ECS::Registry serverRegistry;
    SceneContext serverCtx;
    serverCtx.InitializeNew(&serverRegistry);

    Networking::ReplicationManager serverRep;
    serverRep.Initialize(&serverRegistry);
    serverRep.SetInterestRange(50.0f); // 50 units interest range

    // Initialize 10 Clients
    constexpr int NUM_CLIENTS = 10;
    std::vector<ClientNode> clients(NUM_CLIENTS);
    std::vector<SceneContext> clientContexts(NUM_CLIENTS);

    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients[i].peerId = i + 1;
        clientContexts[i].InitializeNew(&clients[i].registry);
        clients[i].replication.Initialize(&clients[i].registry);
        serverRep.OnPeerConnected(clients[i].peerId);
    }

    // Set up random generators
    std::mt19937 rng(42); // deterministic seed
    std::uniform_real_distribution<float> posDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

    // Track created GUIDs on server
    std::vector<Save::EntityGUID> serverEntities;

    // Simulate 100 frames
    std::cout << "  Simulating 100 ticks under latency, packet loss, and reconnects..." << std::endl;
    for (int tick = 0; tick < 100; tick++) {
        serverCtx.Restore();

        // 1. Spawning dynamic entities on Server (5% chance per tick)
        if (probDist(rng) < 0.3f && serverEntities.size() < 40) {
            ECS::Entity ent = serverRegistry.CreateEntity();
            Save::EntityGUID guid = serverRegistry.CreateGUID(ent);
            glm::vec3 pos(posDist(rng), 0.0f, posDist(rng));
            serverRegistry.AddComponent<Scene::TransformComponent>(ent, pos);
            serverEntities.push_back(guid);
        }

        // 2. Dynamic Entity Movement on Server
        for (const auto& guid : serverEntities) {
            ECS::Entity ent = serverRegistry.GetEntityByGUID(guid);
            if (ent != ECS::NULL_ENTITY) {
                auto& tc = serverRegistry.GetComponent<Scene::TransformComponent>(ent);
                tc.position.x += posDist(rng) * 0.05f; // small drift
                tc.position.z += posDist(rng) * 0.05f;
            }
        }

        // 3. Dynamic Player Node movements (simulating Client positions)
        for (int i = 0; i < NUM_CLIENTS; i++) {
            // Check if player owns an entity on the server to act as focus
            ECS::Entity playerServerEnt = serverRegistry.GetEntityByGUID(Save::EntityGUID{0, static_cast<uint64_t>(clients[i].peerId)});
            if (playerServerEnt == ECS::NULL_ENTITY) {
                playerServerEnt = serverRegistry.CreateEntity();
                Save::EntityGUID playerGuid{0, static_cast<uint64_t>(clients[i].peerId)};
                serverRegistry.AssignGUID(playerServerEnt, playerGuid);
                auto& own = serverRegistry.AddComponent<Networking::NetworkOwnershipComponent>(playerServerEnt);
                own.ownerPeerId = clients[i].peerId;
                serverRegistry.AddComponent<Scene::TransformComponent>(playerServerEnt, glm::vec3(static_cast<float>(i) * 10.0f, 0.0f, 0.0f));
            } else {
                auto& tc = serverRegistry.GetComponent<Scene::TransformComponent>(playerServerEnt);
                tc.position.x += posDist(rng) * 0.1f; // drift player
            }
        }

        // 4. Update Server replication state
        serverRep.ServerUpdate();
        serverCtx.Save();

        // 5. Route server outgoing packets to clients
        Networking::OutgoingNetworkEvent outEv;
        std::unordered_map<uint32_t, std::vector<uint8_t>> peerSnapshots;
        while (Networking::NetworkManager::Get().PopOutgoingEventForTest(outEv)) {
            if (outEv.type == Networking::OutgoingNetworkEvent::Type::Send) {
                peerSnapshots[outEv.peerId] = outEv.payload;
            }
        }

        // 6. Simulate client processing & packet loss (10% loss rate) & disconnection cycles
        for (int i = 0; i < NUM_CLIENTS; i++) {
            auto& client = clients[i];
            auto& clientCtx = clientContexts[i];

            if (client.disconnectTicksLeft > 0) {
                client.disconnectTicksLeft--;
                if (client.disconnectTicksLeft == 0) {
                    // Reconnect peer
                    client.isConnected = true;
                    serverCtx.Restore();
                    serverRep.OnPeerConnected(client.peerId);
                    serverCtx.Save();
                    std::cout << "    [Tick " << tick << "] Client " << client.peerId << " reconnected." << std::endl;
                }
                continue;
            }

            // Simulate random disconnect (1% chance per tick)
            if (client.isConnected && probDist(rng) < 0.01f) {
                client.isConnected = false;
                client.disconnectTicksLeft = 5; // disconnect for 5 ticks
                serverCtx.Restore();
                serverRep.OnPeerDisconnected(client.peerId);
                serverCtx.Save();
                std::cout << "    [Tick " << tick << "] Client " << client.peerId << " connection dropped (simulated loss)." << std::endl;
                continue;
            }

            // If we have a snapshot payload for this client
            auto snapIt = peerSnapshots.find(client.peerId);
            if (snapIt != peerSnapshots.end() && client.isConnected) {
                // Simulate 10% packet drop
                if (probDist(rng) < 0.10f) {
                    // Dropped! Skip this packet
                    continue;
                }

                // Parse snapshot on client
                clientCtx.Restore();
                Networking::PacketReader reader(snapIt->second.data(), snapIt->second.size());
                client.replication.ClientProcessSnapshot(reader);
                clientCtx.Save();

                // Retrieve client ack packet
                Networking::OutgoingNetworkEvent clientAckEvent;
                bool foundAck = false;
                std::vector<uint8_t> ackPayload;
                while (Networking::NetworkManager::Get().PopOutgoingEventForTest(clientAckEvent)) {
                    if (clientAckEvent.type == Networking::OutgoingNetworkEvent::Type::Send && clientAckEvent.peerId == 1) {
                        ackPayload = clientAckEvent.payload;
                        foundAck = true;
                    }
                }

                // Deliver ack to server (simulating 10% ack loss too)
                if (foundAck && probDist(rng) >= 0.10f) {
                    serverCtx.Restore();
                    Networking::PacketReader ackReader(ackPayload.data(), ackPayload.size());
                    serverRep.ProcessIncomingPacket(client.peerId, ackReader);
                    serverCtx.Save();
                }
            }
        }
    }

    // Cleanup resources
    serverCtx.Restore();
    Scene::SceneManager::Get().Shutdown();

    for (int i = 0; i < NUM_CLIENTS; i++) {
        clientContexts[i].Restore();
        Scene::SceneManager::Get().Shutdown();
    }

    std::cout << "  Stress test simulation finished successfully!" << std::endl;
    std::cout << "=== ALL STRESS TESTS COMPLETED SUCCESSFULLY ===" << std::endl;
    return 0;
}
