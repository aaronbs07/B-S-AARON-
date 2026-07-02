#pragma once
#include "ecs/ecs.hpp"
#include "networking/Packet.hpp"
#include "save/EntityGUID.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>

struct lua_State;

namespace KumariEngine::Networking {

using RPCHandler = std::function<void(uint32_t peerId, const Save::EntityGUID& entityGuid, PacketReader& reader)>;

class RPCManager {
public:
    static RPCManager& Get() {
        static RPCManager instance;
        return instance;
    }

    RPCManager(const RPCManager&) = delete;
    RPCManager& operator=(const RPCManager&) = delete;

    void Initialize(ECS::Registry* registry);
    void Shutdown();

    // Handler Registration
    void RegisterHandler(const std::string& name, RPCHandler handler);
    void UnregisterHandler(const std::string& name);

    // Incoming Packet Dispatcher
    void ProcessIncomingPacket(uint32_t peerId, PacketReader& reader);

    // Outgoing RPC senders
    void SendRPC(uint32_t peerId, const std::string& name, const Save::EntityGUID& entityGuid, const PacketWriter& argsWriter, bool reliable = true);
    void BroadcastRPC(const std::string& name, const Save::EntityGUID& entityGuid, const PacketWriter& argsWriter, bool reliable = true);

    // Lua integration
    void RegisterLuaHandler(const std::string& name, int callbackRef);
    void ClearLuaHandlers();

    // Registry access
    ECS::Registry* GetRegistry() const { return m_registry; }

    // Helpers to serialize dynamic typed arguments
    static void WriteInt(PacketWriter& writer, int32_t val);
    static void WriteFloat(PacketWriter& writer, float val);
    static void WriteDouble(PacketWriter& writer, double val);
    static void WriteBool(PacketWriter& writer, bool val);
    static void WriteString(PacketWriter& writer, const std::string& val);
    static void WriteGUID(PacketWriter& writer, const Save::EntityGUID& val);
    static void WriteVec3(PacketWriter& writer, const glm::vec3& val);

    enum class ArgType : uint8_t {
        Int = 1,
        Float = 2,
        Double = 3,
        Bool = 4,
        String = 5,
        GUID = 6,
        Vec3 = 7
    };

private:
    RPCManager() = default;
    ~RPCManager() = default;

    bool ValidateOwnership(uint32_t peerId, const Save::EntityGUID& entityGuid);
    void DispatchLuaRPC(const std::string& name, uint32_t peerId, const Save::EntityGUID& entityGuid, PacketReader& reader);

    ECS::Registry* m_registry = nullptr;
    std::mutex m_mutex;

    std::unordered_map<std::string, RPCHandler> m_handlers;
    std::unordered_map<std::string, int> m_luaHandlers; // rpcName -> callbackRef in Lua registry

    // Rate Limiting Tracking
    std::unordered_map<uint32_t, uint32_t> m_rpcCounts;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_rpcResetTimes;
};

} // namespace KumariEngine::Networking
