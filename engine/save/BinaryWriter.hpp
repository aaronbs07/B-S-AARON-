#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include "Endian.hpp"

namespace KumariEngine::Save {

class BinaryWriter {
public:
    explicit BinaryWriter(std::ostream& stream) : m_stream(stream) {}

    // Prevent copy/assignment
    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;

    void WriteInt8(int8_t value) { WriteType(value); }
    void WriteUint8(uint8_t value) { WriteType(value); }
    void WriteInt16(int16_t value) { WriteType(value); }
    void WriteUint16(uint16_t value) { WriteType(value); }
    void WriteInt32(int32_t value) { WriteType(value); }
    void WriteUint32(uint32_t value) { WriteType(value); }
    void WriteInt64(int64_t value) { WriteType(value); }
    void WriteUint64(uint64_t value) { WriteType(value); }
    void WriteFloat(float value) { WriteType(value); }
    void WriteDouble(double value) { WriteType(value); }

    void WriteBool(bool value) {
        uint8_t byteValue = value ? 1 : 0;
        WriteType(byteValue);
    }

    void WriteString(const std::string& value) {
        uint32_t length = static_cast<uint32_t>(value.size());
        WriteUint32(length);
        if (length > 0) {
            WriteRaw(value.data(), length);
        }
    }

    template<typename T>
    void WriteArray(const T* data, size_t count) {
        if (count == 0) return;
        if constexpr (sizeof(T) == 1) {
            WriteRaw(data, count);
        } else {
            for (size_t i = 0; i < count; ++i) {
                WriteType(data[i]);
            }
        }
    }

    template<typename T>
    void WriteVector(const std::vector<T>& vec) {
        uint32_t count = static_cast<uint32_t>(vec.size());
        WriteUint32(count);
        WriteArray(vec.data(), count);
    }

    void WriteBytes(const void* data, size_t size) {
        WriteRaw(data, size);
    }

private:
    template<typename T>
    void WriteType(T value) {
        static_assert(std::is_arithmetic_v<T>, "WriteType only supports arithmetic types");
        T leValue = NativeToLittle(value);
        WriteRaw(&leValue, sizeof(T));
    }

    void WriteRaw(const void* data, size_t size) {
        if (size == 0) return;
        m_stream.write(reinterpret_cast<const char*>(data), size);
        if (!m_stream) {
            throw std::runtime_error("BinaryWriter: Failed to write data to output stream.");
        }
    }

    std::ostream& m_stream;
};

} // namespace KumariEngine::Save
