add_library(KumariEditor STATIC
    editor.hpp
    editor.cpp
    editor_config.hpp
    editor_config.cpp
    window_system.hpp
    window_system.cpp
    selection_system.hpp
    selection_system.cpp
    undo_redo.hpp
    undo_redo.cpp
    transform_gizmo.hpp
    transform_gizmo.cpp
    windows/scene_view_window.hpp
    windows/scene_view_window.cpp
    windows/game_view_window.hpp
    windows/game_view_window.cpp
    windows/hierarchy_window.hpp
    windows/hierarchy_window.cpp
    windows/inspector_window.hpp
    windows/inspector_window.cpp
    windows/console_window.hpp
    windows/console_window.cpp
    windows/asset_browser_window.hpp
    windows/asset_browser_window.cpp
    windows/statistics_window.hpp
    windows/statistics_window.cpp
    windows/terrain_panels.hpp
    windows/terrain_panels.cpp
    windows/world_streaming_window.hpp
    windows/world_streaming_window.cpp
    windows/advanced_tools_windows.hpp
    windows/advanced_tools_windows.cpp
    windows/camera_preview_window.hpp
    windows/camera_preview_window.cpp
    windows/timeline_editor_window.hpp
    windows/timeline_editor_window.cpp
    windows/cinematic_preview_window.hpp
    windows/cinematic_preview_window.cpp
    windows/gameplay_inspector_window.hpp
    windows/gameplay_inspector_window.cpp
)

target_include_directories(KumariEditor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(KumariEditor PUBLIC KumariEngine)

add_executable(KumariEditorApp main.cpp)
target_link_libraries(KumariEditorApp PRIVATE KumariEditor)
