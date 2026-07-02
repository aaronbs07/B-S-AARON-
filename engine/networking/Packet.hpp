#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace KumariEngine::Networking {

enum class PacketId : uint16_t {
    Invalid = 0,
    HandshakeRequest = 1,
    HandshakeResponse = 2,
    Ping = 3,
    Pong = 4,
    TestData = 5,
    ReplicationSnapshot = 6,
    ReplicationAck = 7,
    RPC = 8,
    InputSync = 9
};

constexpr uint32_t PROTOCOL_VERSION = 1;

class PacketWriter {
public:
    explicit PacketWriter(PacketId packetId);

    void WriteInt8(int8_t value);
    void WriteUint8(uint8_t value);
    void WriteInt16(int16_t value);
    void WriteUint16(uint16_t value);
    void WriteInt32(int32_t value);
    void WriteUint32(uint32_t value);
    void WriteInt64(int64_t value);
    void WriteUint64(uint64_t value);
    void WriteFloat(float value);
    void WriteDouble(double value);
    void WriteBool(bool value);
    void WriteString(const std::string& value);
    void WriteBytes(const void* data, size_t size);

    const uint8_t* GetData() const { return m_buffer.data(); }
    size_t GetSize() const { return m_buffer.size(); }
    const std::vector<uint8_t>& GetBuffer() const { return m_buffer; }

private:
    template<typename T>
    void WriteType(T value);

    std::vector<uint8_t> m_buffer;
};

class PacketReader {
public:
    PacketReader(const uint8_t* data, size_t size);

    bool HasError() const { return m_hasError; }
    bool IsValid() const { return !m_hasError; }
    
    PacketId GetPacketId() const { return m_packetId; }
    uint32_t GetProtocolVersion() const { return m_protocolVersion; }

    bool ReadInt8(int8_t& outValue);
    bool ReadUint8(uint8_t& outValue);
    bool ReadInt16(int16_t& outValue);
    bool ReadUint16(uint16_t& outValue);
    bool ReadInt32(int32_t& outValue);
    bool ReadUint32(uint32_t& outValue);
    bool ReadInt64(int64_t& outValue);
    bool ReadUint64(uint64_t& outValue);
    bool ReadFloat(float& outValue);
    bool ReadDouble(double& outValue);
    bool ReadBool(bool& outValue);
    bool ReadString(std::string& outValue);
    bool ReadBytes(void* dest, size_t size);

    size_t GetBytesRemaining() const {
        if (m_offset >= m_size) return 0;
        return m_size - m_offset;
    }

    size_t GetOffset() const { return m_offset; }
    void ClearError() { m_hasError = false; }
    
    bool Seek(size_t offset) {
        if (offset > m_size) {
            m_hasError = true;
            return false;
        }
        m_offset = offset;
        return true;
    }
    
    bool Skip(size_t bytes) {
        if (m_offset + bytes > m_size) {
            m_hasError = true;
            return false;
        }
        m_offset += bytes;
        return true;
    }

private:
    template<typename T>
    bool ReadType(T& outValue);

    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_offset = 0;
    bool m_hasError = false;

    uint32_t m_protocolVersion = 0;
    PacketId m_packetId = PacketId::Invalid;
};

} // namespace KumariEngine::Networking
