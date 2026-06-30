#include "core/engine.hpp"
#include "core/logger.hpp"
#include <iostream>
#include <exception>

int main() {
    try {
        KumariEngine::Core::Engine engine;
        
        // Initialize engine with window configurations
        if (!engine.Initialize("Kumari Kandam Engine - Milestone 1", 1024, 768)) {
            KumariEngine::Core::Logger::Error("Main", "Failed to start the engine.");
            return -1;
        }

        // Enter application loop (runs until window is closed or Escape is pressed)
        engine.Run();
        
        // Teardown modules
        engine.Shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "Unhandled fatal exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
