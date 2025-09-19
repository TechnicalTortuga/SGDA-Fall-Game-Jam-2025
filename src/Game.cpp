#include "Game.h"
#include "core/StateManager.h"
#include "utils/Logger.h"

Game::Game()
    : engine_(nullptr)
    , initialized_(false)
{
}

Game::~Game()
{
    Shutdown();
}

bool Game::Initialize()
{
    if (initialized_) {
        LOG_WARNING("Game already initialized");
        return true;
    }

    LOG_INFO("Initializing PaintSplash Game");

    // Initialize raylib
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenWidth_, screenHeight_, "PaintSplash - P2P Paint Shooter");
    SetTargetFPS(targetFPS_);

    // Initialize audio device
    InitAudioDevice();

    // Create the engine (it will handle all internal systems)
    engine_ = new Engine();

    if (!engine_) {
        LOG_ERROR("Failed to create engine");
        Shutdown();
        return false;
    }

        // Initialize the engine (this creates and initializes all systems)
        if (!engine_->Initialize()) {
            LOG_ERROR("Failed to initialize engine");
            Shutdown();
            return false;
        }

        // Start the game (set state to GAME)
        engine_->GetStateManager()->StartGame();

    // Enable FPS-style mouse capture
    SetWindowFocused();
    DisableCursor();

    LOG_INFO("PaintSplash Game initialized successfully");
    initialized_ = true;
    return true;
}


void Game::Run()
{
    if (!initialized_) {
        LOG_ERROR("Cannot run game: not initialized");
        return;
    }

    LOG_INFO("Starting game loop");

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        float deltaTime = GetFrameTime();
        Update(deltaTime);

        // Render
        BeginDrawing();
        ClearBackground(DARKGRAY);
        Render();

        // Draw current FPS counter (top right)
        DrawFPS(screenWidth_ - 80, 10);

        EndDrawing();
    }

    LOG_INFO("Game loop ended");
}

void Game::Shutdown()
{
    if (!initialized_) return;

    LOG_INFO("Shutting down PaintSplash");

    // Shutdown the engine (it will handle all internal cleanup)
    if (engine_) {
        engine_->Shutdown();
        delete engine_;
        engine_ = nullptr;
    }

    // Cleanup raylib
    CloseAudioDevice();
    CloseWindow();

    initialized_ = false;
    LOG_INFO("PaintSplash shutdown complete");
}

void Game::Update(float deltaTime)
{
    // Update the engine (it handles all systems, events, and state management)
    if (engine_) {
        engine_->Update(deltaTime);
    }
}

void Game::Render()
{
    // Render through the engine (it handles all rendering, including state-specific overlays)
    if (engine_) {
        engine_->Render();
    }
}
