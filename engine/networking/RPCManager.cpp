#include "networking/RPCManager.hpp"
#include "core/logger.hpp"
#include "scripting/script_engine.hpp"
#include "networking/NetworkManager.hpp"
#include "networking/NetworkOwnershipComponent.hpp"
#include <enet/enet.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace KumariEngine::Networking {

void RPCManager::Initialize(ECS::Registry* registry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry = registry;
    if (m_registry) {
        m_registry->RegisterComponent<NetworkOwnershipComponent>();
    }
    m_handlers.clear();
    m_luaHandlers.clear();
    Core::Logger::Info("Networking", "RPCManager initialized successfully.");
}

void RPCManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.clear();
    ClearLuaHandlers();
    m_registry = nullptr;
    Core::Logger::Info("Networking", "RPCManager shut down.");
}

void RPCManager::RegisterHandler(const std::string& name, RPCHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers[name] = handler;
}

void RPCManager::UnregisterHandler(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.erase(name);
}

void RPCManager::RegisterLuaHandler(const std::string& name, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // If there is an existing handler, unref it first
    auto it = m_luaHandlers.find(name);
    if (it != m_luaHandlers.end()) {
        auto* se = &Scripting::ScriptEngine::Get();
        lua_State* L = se->GetLuaState();
        if (L) {
            luaL_unref(L, LUA_REGISTRYINDEX, it->second);
        }
    }
    m_luaHandlers[name] = callbackRef;
}

void RPCManager::ClearLuaHandlers() {
    auto* se = &Scripting::ScriptEngine::Get();
    lua_State* L = se->GetLuaState();
    if (L) {
        for (auto& [name, ref] : m_luaHandlers) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }
    }
    m_luaHandlers.clear();
}

bool RPCManager::ValidateOwnership(uint32_t peerId, const Save::EntityGUID& entityGuid) {
    if (!m_registry) return false;
    if (entityGuid.IsNull()) {
        // Global RPC (e.g. matchmaking, global chat, login, spawn).
        // Server-side handlers will perform any authorization.
        return true;
    }

    ECS::Entity ent = m_registry->GetEntityByGUID(entityGuid);
    if (ent == ECS::NULL_ENTITY) {
        Core::Logger::Warning("Networking", "RPC ownership check failed: Entity GUID %s not found.", entityGuid.ToString().c_str());
        return false;
    }

    if (m_registry->HasComponent<NetworkOwnershipComponent>(ent)) {
        const auto& ownership = m_registry->GetComponent<NetworkOwnershipComponent>(ent);
        if (ownership.ownerPeerId == peerId) {
            return true;
        }
        Core::Logger::Warning("Security", "Suspicious Activity: Peer %u attempted to invoke RPC on entity %s owned by Peer %u.",
            peerId, entityGuid.ToString().c_str(), ownership.ownerPeerId);
        return false;
    }

    // No ownership component: it's a server-owned entity. No client is allowed to call RPCs on it.
    Core::Logger::Warning("Security", "Suspicious Activity: Peer %u attempted to invoke RPC on server-owned entity %s.",
        peerId, entityGuid.ToString().c_str());
    return false;
}

void RPCManager::ProcessIncomingPacket(uint32_t peerId, PacketReader& reader) {
    if (reader.HasError()) {
        Core::Logger::Warning("Networking", "Rejected malformed packet header from peer %u.", peerId);
        return;
    }

    // Check packet size limit
    size_t packetSize = reader.GetBytesRemaining() + reader.GetOffset();
    if (packetSize > 65536) {
        Core::Logger::Warning("Security", "Suspicious Activity: Rejected oversized packet from peer %u. Size: %zu bytes.", peerId, packetSize);
        return;
    }

    if (reader.GetPacketId() != PacketId::RPC) {
        return;
    }

    // Rate Limiting Security Check
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        auto itReset = m_rpcResetTimes.find(peerId);
        if (itReset == m_rpcResetTimes.end() || now - itReset->second >= std::chrono::seconds(1)) {
            m_rpcResetTimes[peerId] = now;
            m_rpcCounts[peerId] = 0;
        }

        m_rpcCounts[peerId]++;
        if (m_rpcCounts[peerId] > 100) {
            Core::Logger::Warning("Security", "Suspicious Activity: Peer %u exceeded RPC rate limit (100/sec). Disconnecting peer.", peerId);
            NetworkManager::Get().DisconnectPeer(peerId);
            return;
        }
    }

    std::string name;
    Save::EntityGUID entityGuid;
    if (!reader.ReadString(name) || !reader.ReadUint64(entityGuid.high) || !reader.ReadUint64(entityGuid.low)) {
        Core::Logger::Warning("Networking", "Rejected malformed RPC packet from peer %u (failed to parse header).", peerId);
        return;
    }

    // Validate ownership if we are server
    if (NetworkManager::Get().GetState() == NetworkState::Server) {
        if (!ValidateOwnership(peerId, entityGuid)) {
            return;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Dispatch C++ handler
    auto it = m_handlers.find(name);
    if (it != m_handlers.end()) {
        size_t paramOffset = reader.GetOffset();
        it->second(peerId, entityGuid, reader);
        reader.Seek(paramOffset); // restore offset for Lua dispatching if needed
    }

    // Dispatch Lua handler
    DispatchLuaRPC(name, peerId, entityGuid, reader);
}

void RPCManager::SendRPC(uint32_t peerId, const std::string& name, const Save::EntityGUID& entityGuid, const PacketWriter& argsWriter, bool reliable) {
    PacketWriter writer(PacketId::RPC);
    writer.WriteString(name);
    writer.WriteUint64(entityGuid.high);
    writer.WriteUint64(entityGuid.low);
    if (argsWriter.GetSize() > 6) {
        writer.WriteBytes(argsWriter.GetData() + 6, argsWriter.GetSize() - 6);
    }

    if (writer.GetSize() > 65536) {
        Core::Logger::Error("Networking", "Attempted to send oversized RPC '%s' (%zu bytes). Blocked.", name.c_str(), writer.GetSize());
        return;
    }

    NetworkManager::Get().Send(peerId, writer, reliable);
}

void RPCManager::BroadcastRPC(const std::string& name, const Save::EntityGUID& entityGuid, const PacketWriter& argsWriter, bool reliable) {
    PacketWriter writer(PacketId::RPC);
    writer.WriteString(name);
    writer.WriteUint64(entityGuid.high);
    writer.WriteUint64(entityGuid.low);
    if (argsWriter.GetSize() > 6) {
        writer.WriteBytes(argsWriter.GetData() + 6, argsWriter.GetSize() - 6);
    }

    if (writer.GetSize() > 65536) {
        Core::Logger::Error("Networking", "Attempted to broadcast oversized RPC '%s' (%zu bytes). Blocked.", name.c_str(), writer.GetSize());
        return;
    }

    NetworkManager::Get().Broadcast(writer, reliable);
}

void RPCManager::DispatchLuaRPC(const std::string& name, uint32_t peerId, const Save::EntityGUID& entityGuid, PacketReader& reader) {
    auto it = m_luaHandlers.find(name);
    if (it == m_luaHandlers.end()) return;
    int callbackRef = it->second;

    auto* se = &Scripting::ScriptEngine::Get();
    lua_State* L = se->GetLuaState();
    if (!L) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // Push fixed arguments: senderPeerId, entityGuid (as string representation)
    lua_pushinteger(L, peerId);
    lua_pushstring(L, entityGuid.ToString().c_str());

    int numArgs = 2;

    // Read remaining parameters dynamically until end of packet or error
    while (reader.GetBytesRemaining() > 0 && !reader.HasError()) {
        uint8_t rawType = 0;
        if (!reader.ReadUint8(rawType)) break;

        ArgType type = static_cast<ArgType>(rawType);
        if (type == ArgType::Int) {
            int32_t val = 0;
            if (reader.ReadInt32(val)) {
                lua_pushinteger(L, val);
                numArgs++;
            }
        } else if (type == ArgType::Float) {
            float val = 0.0f;
            if (reader.ReadFloat(val)) {
                lua_pushnumber(L, val);
                numArgs++;
            }
        } else if (type == ArgType::Double) {
            double val = 0.0;
            if (reader.ReadDouble(val)) {
                lua_pushnumber(L, val);
                numArgs++;
            }
        } else if (type == ArgType::Bool) {
            bool val = false;
            if (reader.ReadBool(val)) {
                lua_pushboolean(L, val);
                numArgs++;
            }
        } else if (type == ArgType::String) {
            std::string val;
            if (reader.ReadString(val)) {
                lua_pushlstring(L, val.data(), val.size());
                numArgs++;
            }
        } else if (type == ArgType::GUID) {
            Save::EntityGUID val;
            if (reader.ReadUint64(val.high) && reader.ReadUint64(val.low)) {
                lua_pushstring(L, val.ToString().c_str());
                numArgs++;
            }
        } else if (type == ArgType::Vec3) {
            glm::vec3 val;
            if (reader.ReadFloat(val.x) && reader.ReadFloat(val.y) && reader.ReadFloat(val.z)) {
                lua_newtable(L);
                lua_pushnumber(L, val.x);
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y);
                lua_setfield(L, -2, "y");
                lua_pushnumber(L, val.z);
                lua_setfield(L, -2, "z");
                numArgs++;
            }
        } else {
            Core::Logger::Warning("Networking", "Unknown dynamic argument type tag %u. Aborting Lua RPC dispatch.", rawType);
            break;
        }
    }

    if (lua_pcall(L, numArgs, 0, 0) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 1);
        Core::Logger::Error("ScriptEngine", "Error executing Lua RPC callback '%s': %s", name.c_str(), err.c_str());
    }
}

void RPCManager::WriteInt(PacketWriter& writer, int32_t val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::Int));
    writer.WriteInt32(val);
}

void RPCManager::WriteFloat(PacketWriter& writer, float val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::Float));
    writer.WriteFloat(val);
}

void RPCManager::WriteDouble(PacketWriter& writer, double val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::Double));
    writer.WriteDouble(val);
}

void RPCManager::WriteBool(PacketWriter& writer, bool val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::Bool));
    writer.WriteBool(val);
}

void RPCManager::WriteString(PacketWriter& writer, const std::string& val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::String));
    writer.WriteString(val);
}

void RPCManager::WriteGUID(PacketWriter& writer, const Save::EntityGUID& val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::GUID));
    writer.WriteUint64(val.high);
    writer.WriteUint64(val.low);
}

void RPCManager::WriteVec3(PacketWriter& writer, const glm::vec3& val) {
    writer.WriteUint8(static_cast<uint8_t>(ArgType::Vec3));
    writer.WriteFloat(val.x);
    writer.WriteFloat(val.y);
    writer.WriteFloat(val.z);
}

} // namespace KumariEngine::Networking
