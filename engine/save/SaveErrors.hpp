#pragma once
#include <stdexcept>
#include <string>

namespace KumariEngine::Save {

class SerializationError : public std::runtime_error {
public:
    explicit SerializationError(const std::string& message) : std::runtime_error(message) {}
};

class DeserializationError : public std::runtime_error {
public:
    explicit DeserializationError(const std::string& message) : std::runtime_error(message) {}
};

class ChecksumError : public DeserializationError {
public:
    explicit ChecksumError(const std::string& message) : DeserializationError(message) {}
};

class VersionMismatchError : public DeserializationError {
public:
    explicit VersionMismatchError(const std::string& message) : DeserializationError(message) {}
};

} // namespace KumariEngine::Save
