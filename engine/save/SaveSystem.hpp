#pragma once

#include <string>
#include <functional>
#include "BinaryWriter.hpp"
#include "BinaryReader.hpp"

namespace KumariEngine::Save {

class SaveSystem {
public:
    /**
     * @brief Writes a save file with a versioned header.
     * 
     * @param filepath Target path on disk.
     * @param serializeCallback Callback receiving a BinaryWriter to serialize custom data.
     * @return true if successful, false otherwise.
     */
    static bool WriteSave(const std::string& filepath, 
                          const std::function<bool(BinaryWriter&)>& serializeCallback);

    /**
     * @brief Reads a save file, validating its magic header and version, then invokes callback.
     * 
     * @param filepath Path on disk.
     * @param deserializeCallback Callback receiving BinaryReader and the save version.
     * @return true if successful, false otherwise.
     */
    static bool ReadSave(const std::string& filepath, 
                         const std::function<bool(BinaryReader&, uint32_t)>& deserializeCallback);
};

} // namespace KumariEngine::Save
