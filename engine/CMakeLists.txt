add_library(KumariEngine STATIC
    core/logger.hpp
    core/logger.cpp
    core/engine.hpp
    core/engine.cpp
    core/debug_overlay.hpp
    core/debug_overlay.cpp
    window/window.hpp
    window/window.cpp
    input/input.hpp
    input/input.cpp
    renderer/renderer.hpp
    renderer/renderer.cpp
    renderer/vulkan/vulkan_context.hpp
    renderer/vulkan/vulkan_context.cpp
    renderer/vulkan/vulkan_renderer.hpp
    renderer/vulkan/vulkan_renderer.cpp
    ecs/ecs.hpp
    ecs/ecs.cpp
    resource/resource.hpp
    resource/resource_manager.hpp
    resource/resource_manager.cpp
    asset_pipeline/asset_manager.hpp
    asset_pipeline/asset_manager.cpp
    scene/scene_node.hpp
    scene/scene_node.cpp
    scene/scene_manager.hpp
    scene/scene_manager.cpp
    camera/camera.hpp
    camera/camera.cpp
    camera/camera_manager.hpp
    camera/camera_manager.cpp
    terrain/noise_generator.hpp
    terrain/noise_generator.cpp
    terrain/terrain_chunk.hpp
    terrain/terrain_chunk.cpp
    terrain/terrain_manager.hpp
    terrain/terrain_manager.cpp
    terrain/terrain_renderer.hpp
    terrain/terrain_renderer.cpp
    terrain/terrain_shader_bytecode.hpp
    terrain/vegetation_system.hpp
    terrain/vegetation_system.cpp
    physics/physics_types.hpp
    physics/physics_components.hpp
    physics/spatial_hash_grid.hpp
    physics/spatial_hash_grid.cpp
    physics/physics_world.hpp
    physics/physics_world.cpp
    physics/character_controller.hpp
    physics/character_controller.cpp
    physics/raycast.hpp
    physics/raycast.cpp
    physics/debug_shader_bytecode.hpp
    physics/debug_renderer.hpp
    physics/debug_renderer.cpp
    physics/physics_system.hpp
    physics/physics_system.cpp
)

# Expose engine source root and stb root for clean includes
target_include_directories(KumariEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${stb_SOURCE_DIR}
)

# Link third-party fetched libraries
target_link_libraries(KumariEngine PUBLIC
    glfw
    glm
    volk
    Vulkan-Headers
)

# Enable experimental GLM extensions
target_compile_definitions(KumariEngine PUBLIC GLM_ENABLE_EXPERIMENTAL)
