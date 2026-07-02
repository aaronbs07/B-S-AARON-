#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "save/EntityGUID.hpp"
#include "save/SaveVersion.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Save {

class SaveManager {
public:
    static SaveManager& Get() {
        static SaveManager instance;
        return instance;
    }

    // Prevent copy/assignment
    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    bool Initialize();
    void Shutdown();

    bool SaveGame(const std::string& filepath, ECS::Registry* registry);
    bool LoadGame(const std::string& filepath, ECS::Registry* registry);

private:
    SaveManager() = default;
    ~SaveManager() = default;
};

} // namespace KumariEngine::Save
