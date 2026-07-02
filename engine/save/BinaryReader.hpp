#pragma once

#include <string>
#include <vector>
#include <istream>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "Endian.hpp"

namespace KumariEngine::Save {

class BinaryReader {
public:
    explicit BinaryReader(std::istream& stream) : m_offset(0), m_hasError(false) {
        if (!stream) {
            m_hasError = true;
            return;
        }
        
        // Find stream size from current position to end
        std::streampos startPos = stream.tellg();
        stream.seekg(0, std::ios::end);
        std::streampos endPos = stream.tellg();
        
        if (endPos >= startPos) {
            std::streamsize size = endPos - startPos;
            if (size > 0) {
                m_buffer.resize(static_cast<size_t>(size));
                stream.seekg(startPos, std::ios::beg);
                stream.read(reinterpret_cast<char*>(m_buffer.data()), size);
                if (!stream) {
                    m_hasError = true;
                }
            }
        } else {
            m_hasError = true;
        }
    }

    // Prevent copy/assignment
    BinaryReader(const BinaryReader&) = delete;
    BinaryReader& operator=(const BinaryReader&) = delete;

    bool HasError() const { return m_hasError; }
    
    size_t GetBytesRemaining() const {
        if (m_offset >= m_buffer.size()) return 0;
        return m_buffer.size() - m_offset;
    }

    size_t GetOffset() const { return m_offset; }
    void ClearError() { m_hasError = false; }

    bool Skip(size_t bytes) {
        if (m_offset + bytes > m_buffer.size()) {
            m_hasError = true;
            return false;
        }
        m_offset += bytes;
        return true;
    }

    bool Seek(size_t offset) {
        if (offset > m_buffer.size()) {
            m_hasError = true;
            return false;
        }
        m_offset = offset;
        return true;
    }

    bool ReadInt8(int8_t& outValue) { return ReadType(outValue); }
    bool ReadUint8(uint8_t& outValue) { return ReadType(outValue); }
    bool ReadInt16(int16_t& outValue) { return ReadType(outValue); }
    bool ReadUint16(uint16_t& outValue) { return ReadType(outValue); }
    bool ReadInt32(int32_t& outValue) { return ReadType(outValue); }
    bool ReadUint32(uint32_t& outValue) { return ReadType(outValue); }
    bool ReadInt64(int64_t& outValue) { return ReadType(outValue); }
    bool ReadUint64(uint64_t& outValue) { return ReadType(outValue); }
    bool ReadFloat(float& outValue) { return ReadType(outValue); }
    bool ReadDouble(double& outValue) { return ReadType(outValue); }

    bool ReadBool(bool& outValue) {
        uint8_t byteValue = 0;
        if (!ReadType(byteValue)) {
            outValue = false;
            return false;
        }
        outValue = (byteValue != 0);
        return true;
    }

    bool ReadString(std::string& outValue) {
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
        if (m_offset + length > m_buffer.size()) {
            m_hasError = true;
            outValue.clear();
            return false;
        }
        outValue.assign(reinterpret_cast<const char*>(&m_buffer[m_offset]), length);
        m_offset += length;
        return true;
    }

    template<typename T>
    bool ReadArray(T* dest, size_t count) {
        if (count == 0) return true;
        if (m_hasError) return false;
        if constexpr (sizeof(T) == 1) {
            return ReadBytes(dest, count);
        } else {
            for (size_t i = 0; i < count; ++i) {
                if (!ReadType(dest[i])) {
                    return false;
                }
            }
            return true;
        }
    }

    template<typename T>
    bool ReadVector(std::vector<T>& vec) {
        uint32_t count = 0;
        if (!ReadUint32(count)) {
            vec.clear();
            return false;
        }
        vec.resize(count);
        if (!ReadArray(vec.data(), count)) {
            vec.clear();
            return false;
        }
        return true;
    }

    bool ReadBytes(void* dest, size_t size) {
        if (m_hasError) return false;
        if (size == 0) return true;
        if (m_offset + size > m_buffer.size()) {
            m_hasError = true;
            return false;
        }
        std::memcpy(dest, &m_buffer[m_offset], size);
        m_offset += size;
        return true;
    }

private:
    template<typename T>
    bool ReadType(T& outValue) {
        static_assert(std::is_arithmetic_v<T>, "ReadType only supports arithmetic types");
        if (m_hasError) {
            outValue = T{};
            return false;
        }
        constexpr size_t size = sizeof(T);
        if (m_offset + size > m_buffer.size()) {
            m_hasError = true;
            outValue = T{};
            return false;
        }
        T leValue;
        std::memcpy(&leValue, &m_buffer[m_offset], size);
        m_offset += size;
        outValue = LittleToNative(leValue);
        return true;
    }

    std::vector<uint8_t> m_buffer;
    size_t m_offset = 0;
    bool m_hasError = false;
};

} // namespace KumariEngine::Save
