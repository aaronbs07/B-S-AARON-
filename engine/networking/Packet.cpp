#include "networking/Packet.hpp"
#include "save/Endian.hpp"
#include <cassert>
#include <cstring>
#include <type_traits>

namespace KumariEngine::Networking {

PacketWriter::PacketWriter(PacketId packetId) {
    // Write Protocol Version and Packet ID in header
    WriteUint32(PROTOCOL_VERSION);
    WriteUint16(static_cast<uint16_t>(packetId));
}

template<typename T>
void PacketWriter::WriteType(T value) {
    static_assert(std::is_arithmetic_v<T>, "WriteType only supports arithmetic types");
    T leValue = Save::NativeToLittle(value);
    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(&leValue);
    m_buffer.insert(m_buffer.end(), bytePtr, bytePtr + sizeof(T));
}

void PacketWriter::WriteInt8(int8_t value) { WriteType(value); }
void PacketWriter::WriteUint8(uint8_t value) { WriteType(value); }
void PacketWriter::WriteInt16(int16_t value) { WriteType(value); }
void PacketWriter::WriteUint16(uint16_t value) { WriteType(value); }
void PacketWriter::WriteInt32(int32_t value) { WriteType(value); }
void PacketWriter::WriteUint32(uint32_t value) { WriteType(value); }
void PacketWriter::WriteInt64(int64_t value) { WriteType(value); }
void PacketWriter::WriteUint64(uint64_t value) { WriteType(value); }
void PacketWriter::WriteFloat(float value) { WriteType(value); }
void PacketWriter::WriteDouble(double value) { WriteType(value); }

void PacketWriter::WriteBool(bool value) {
    WriteUint8(value ? 1 : 0);
}

void PacketWriter::WriteString(const std::string& value) {
    uint32_t length = static_cast<uint32_t>(value.size());
    WriteUint32(length);
    if (length > 0) {
        WriteBytes(value.data(), length);
    }
}

void PacketWriter::WriteBytes(const void* data, size_t size) {
    if (size == 0) return;
    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(data);
    m_buffer.insert(m_buffer.end(), bytePtr, bytePtr + size);
}

PacketReader::PacketReader(const uint8_t* data, size_t size)
    : m_data(data), m_size(size), m_offset(0), m_hasError(false) {
    if (size < sizeof(uint32_t) + sizeof(uint16_t)) {
        m_hasError = true;
        return;
    }
    
    // Read protocol version
    if (!ReadUint32(m_protocolVersion)) {
        m_hasError = true;
        return;
    }
    
    // Validate protocol version
    if (m_protocolVersion != PROTOCOL_VERSION) {
        m_hasError = true;
        return;
    }
    
    // Read packet ID
    uint16_t rawPacketId = 0;
    if (!ReadUint16(rawPacketId)) {
        m_hasError = true;
        return;
    }
    m_packetId = static_cast<PacketId>(rawPacketId);
}

template<typename T>
bool PacketReader::ReadType(T& outValue) {
    static_assert(std::is_arithmetic_v<T>, "ReadType only supports arithmetic types");
    if (m_hasError) {
        outValue = T{};
        return false;
    }
    constexpr size_t size = sizeof(T);
    if (m_offset + size > m_size) {
        m_hasError = true;
        outValue = T{};
        return false;
    }
    T leValue;
    std::memcpy(&leValue, &m_data[m_offset], size);
    m_offset += size;
    outValue = Save::LittleToNative(leValue);
    return true;
}

bool PacketReader::ReadInt8(int8_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadUint8(uint8_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadInt16(int16_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadUint16(uint16_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadInt32(int32_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadUint32(uint32_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadInt64(int64_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadUint64(uint64_t& outValue) { return ReadType(outValue); }
bool PacketReader::ReadFloat(float& outValue) { return ReadType(outValue); }
bool PacketReader::ReadDouble(double& outValue) { return ReadType(outValue); }

bool PacketReader::ReadBool(bool& outValue) {
    uint8_t byteValue = 0;
    if (!ReadUint8(byteValue)) {
        outValue = false;
        return false;
    }
    outValue = (byteValue != 0);
    return true;
}

bool PacketReader::ReadString(std::string& outValue) {
    if (m_hasError) {
        outValue.clear();
        return false;
    }
    uint32_t length = 0;
    if (!ReadUint32(length)) {
        outValue.clear();
        return false;
    }
    if (length == 0) {
        outValue.clear();
        return true;
    }
    if (m_offset + length > m_size) {
        m_hasError = true;
        outValue.clear();
        return false;
    }
    outValue.assign(reinterpret_cast<const char*>(&m_data[m_offset]), length);
    m_offset += length;
    return true;
}

bool PacketReader::ReadBytes(void* dest, size_t size) {
    if (m_hasError) return false;
    if (size == 0) return true;
    if (m_offset + size > m_size) {
        m_hasError = true;
        return false;
    }
    std::memcpy(dest, &m_data[m_offset], size);
    m_offset += size;
    return true;
}

} // namespace KumariEngine::Networking
