#include "SaveSystem.hpp"
#include "SaveVersion.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <exception>

namespace KumariEngine::Save {

bool SaveSystem::WriteSave(const std::string& filepath, 
                          const std::function<bool(BinaryWriter&)>& serializeCallback) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        Core::Logger::Error("SaveSystem", "Failed to open save file for writing: %s", filepath.c_str());
        return false;
    }

    // Write magic and version
    file.write(SAVE_MAGIC.data(), SAVE_MAGIC.size());
    file.write(reinterpret_cast<const char*>(&CURRENT_SAVE_VERSION), sizeof(CURRENT_SAVE_VERSION));

    if (!file) {
        Core::Logger::Error("SaveSystem", "Failed to write header to save file: %s", filepath.c_str());
        return false;
    }

    BinaryWriter writer(file);
    try {
        if (!serializeCallback(writer)) {
            Core::Logger::Error("SaveSystem", "Serialization callback returned false for: %s", filepath.c_str());
            return false;
        }
    } catch (const std::exception& e) {
        Core::Logger::Error("SaveSystem", "Exception during serialization for %s: %s", filepath.c_str(), e.what());
        return false;
    }

    file.close();
    return true;
}

bool SaveSystem::ReadSave(const std::string& filepath, 
                         const std::function<bool(BinaryReader&, uint32_t)>& deserializeCallback) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        Core::Logger::Error("SaveSystem", "Failed to open save file for reading: %s", filepath.c_str());
        return false;
    }

    // Read and verify magic
    std::array<char, 4> magic{};
    file.read(magic.data(), magic.size());
    if (!file || magic != SAVE_MAGIC) {
        Core::Logger::Error("SaveSystem", "Invalid save file magic header in: %s", filepath.c_str());
        return false;
    }

    // Read and verify version
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!file) {
        Core::Logger::Error("SaveSystem", "Failed to read version from save file: %s", filepath.c_str());
        return false;
    }

    if (version > CURRENT_SAVE_VERSION) {
        Core::Logger::Error("SaveSystem", "Save file version (%u) is newer than current supported version (%u): %s",
                            version, CURRENT_SAVE_VERSION, filepath.c_str());
        return false;
    }

    BinaryReader reader(file);
    if (reader.HasError()) {
        Core::Logger::Error("SaveSystem", "Failed to initialize BinaryReader (empty or truncated file data): %s", filepath.c_str());
        return false;
    }

    try {
        if (!deserializeCallback(reader, version)) {
            Core::Logger::Error("SaveSystem", "Deserialization callback returned false for: %s", filepath.c_str());
            return false;
        }
    } catch (const std::exception& e) {
        Core::Logger::Error("SaveSystem", "Exception during deserialization for %s: %s", filepath.c_str(), e.what());
        return false;
    }

    if (reader.HasError()) {
        Core::Logger::Error("SaveSystem", "BinaryReader encountered a bounds/read error while deserializing: %s", filepath.c_str());
        return false;
    }

    return true;
}

} // namespace KumariEngine::Save
