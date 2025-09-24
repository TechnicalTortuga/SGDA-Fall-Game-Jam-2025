#pragma once

#include "raylib.h"
#include "core/Engine.h"

class Game {
public:
    Game();
    ~Game();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    void Update(float deltaTime);
    void Render();

    // Core engine (singleton)
    Engine& engine_;

    // Window settings
    const int screenWidth_ = 1280;
    const int screenHeight_ = 720;
    const int targetFPS_ = 60;

    bool initialized_;
};
