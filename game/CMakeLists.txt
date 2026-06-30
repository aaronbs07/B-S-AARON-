add_executable(KumariKandamGame
    src/main.cpp
)

# Link the game executable with our custom game engine library
target_link_libraries(KumariKandamGame PRIVATE
    KumariEngine
)

add_executable(KumariEngineSmokeTest
    src/smoke_test.cpp
)

# Link smoke test executable with the engine
target_link_libraries(KumariEngineSmokeTest PRIVATE
    KumariEngine
)
