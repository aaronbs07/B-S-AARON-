cmake_minimum_required(VERSION 3.20)
project(KumariKandam CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific flags and compiler options
if(MSVC)
    add_compile_options(/W4 /WX /EHsc /FS)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Fetch Content Configurations
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)


# GLFW Setup
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# GLM Setup
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# Vulkan-Headers Setup
FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG main
)
FetchContent_MakeAvailable(vulkan_headers)

# Volk Setup (Dynamic Vulkan Loader)
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG master
)
FetchContent_MakeAvailable(volk)

# STB Setup
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Lua Setup
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/walterschell/Lua.git
    GIT_TAG master
)
FetchContent_MakeAvailable(lua)

if(TARGET lua_static)
    if(MSVC)
        target_compile_options(lua_static PRIVATE /WX- /W0)
    else()
        target_compile_options(lua_static PRIVATE -w)
    endif()
endif()

# ENet Setup
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
)
FetchContent_MakeAvailable(enet)

if(TARGET enet)
    if(MSVC)
        target_compile_options(enet PRIVATE /WX- /W0)
        target_compile_definitions(enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(enet PRIVATE -w)
    endif()
endif()

# Add Engine, Editor and Game Modules
add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(game)
