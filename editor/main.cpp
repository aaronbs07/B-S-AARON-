#include "core/engine.hpp"
#include "editor.hpp"
#include "core/logger.hpp"
#include <iostream>
#include <exception>

int main() {
    try {
        KumariEngine::Core::Engine engine;
        
        if (!engine.Initialize("Kumari Editor App", 1280, 720)) {
            KumariEngine::Core::Logger::Error("Main", "Failed to start engine for Editor.");
            return -1;
        }

        KumariEngine::Editor::Editor editor;
        if (!editor.Initialize(engine.GetRegistry())) {
            KumariEngine::Core::Logger::Error("Main", "Failed to start Editor module.");
            return -1;
        }

        engine.SetUpdateCallback([&editor](float dt) {
            editor.Update(dt);
        });

        engine.SetRenderCallback([&editor]() {
            editor.Render();
        });

        engine.Run();

        editor.Shutdown();
        engine.Shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "Unhandled fatal exception in Editor: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
