#include "StateManager.h"
#include "events/EventManager.h"
#include "events/Event.h"
#include "utils/Logger.h"
#include "raylib.h"

StateManager::StateManager(EventManager* eventManager)
    : eventManager_(eventManager), currentState_(GameState::NONE), previousState_(GameState::NONE)
{
    LOG_INFO("StateManager initialized");
}

StateManager::~StateManager()
{
    LOG_INFO("StateManager destroyed");
}

void StateManager::SwitchState(GameState newState)
{
    if (newState == currentState_) {
        LOG_DEBUG("Attempted to switch to same state: " + GetStateName(newState));
        return;
    }

    LOG_INFO("Switching from " + GetStateName(currentState_) + " to " + GetStateName(newState));

    // Store previous state for resume functionality
    if (currentState_ != GameState::PAUSED) {
        previousState_ = currentState_;
    }

    GameState oldState = currentState_;
    currentState_ = newState;

    // Post state change event
    if (eventManager_) {
        eventManager_->PostEvent(EventType::GAME_START); // TODO: Add specific state change events
    }
}

std::string StateManager::GetStateName(GameState state) const
{
    switch (state) {
        case GameState::NONE: return "NONE";
        case GameState::MENU: return "MENU";
        case GameState::LOBBY: return "LOBBY";
        case GameState::LOADING: return "LOADING";
        case GameState::GAME: return "GAME";
        case GameState::PAUSED: return "PAUSED";
        default: return "UNKNOWN";
    }
}

void StateManager::Update(float deltaTime)
{
    switch (currentState_) {
        case GameState::MENU:
            UpdateMenu(deltaTime);
            break;
        case GameState::LOBBY:
            UpdateLobby(deltaTime);
            break;
        case GameState::LOADING:
            UpdateLoading(deltaTime);
            break;
        case GameState::GAME:
            UpdateGame(deltaTime);
            break;
        case GameState::PAUSED:
            UpdatePaused(deltaTime);
            break;
        default:
            LOG_WARNING("Unknown state in Update: " + GetStateName(currentState_));
            break;
    }
}

void StateManager::Render()
{
    switch (currentState_) {
        case GameState::MENU:
            RenderMenu();
            break;
        case GameState::LOBBY:
            RenderLobby();
            break;
        case GameState::LOADING:
            RenderLoading();
            break;
        case GameState::GAME:
            RenderGame();
            break;
        case GameState::PAUSED:
            RenderPaused();
            break;
        default:
            LOG_WARNING("Unknown state in Render: " + GetStateName(currentState_));
            break;
    }
}

void StateManager::StartGame()
{
    SwitchState(GameState::GAME);
}

void StateManager::PauseGame()
{
    if (currentState_ == GameState::GAME) {
        SwitchState(GameState::PAUSED);
    }
}

void StateManager::ResumeGame()
{
    if (currentState_ == GameState::PAUSED && previousState_ == GameState::GAME) {
        SwitchState(GameState::GAME);
    }
}

void StateManager::EndGame()
{
    SwitchState(GameState::MENU);
}

void StateManager::ShowMenu()
{
    SwitchState(GameState::MENU);
}

void StateManager::ShowLobby()
{
    SwitchState(GameState::LOBBY);
}

// State-specific update methods
void StateManager::UpdateMenu(float deltaTime)
{
    // Handle menu input
    if (IsKeyPressed(KEY_ENTER)) {
        StartGame();
    }
    if (IsKeyPressed(KEY_L)) {
        ShowLobby();
    }
}

void StateManager::UpdateLobby(float deltaTime)
{
    // Handle lobby input
    if (IsKeyPressed(KEY_ESCAPE)) {
        ShowMenu();
    }
    // TODO: Add lobby-specific logic
}

void StateManager::UpdateLoading(float deltaTime)
{
    // Handle loading logic
    // TODO: Add loading progress
}

void StateManager::UpdateGame(float deltaTime)
{
    // Handle game input
    if (IsKeyPressed(KEY_ESCAPE)) {
        PauseGame();
    }
    // TODO: Game logic will be handled by ECS systems
}

void StateManager::UpdatePaused(float deltaTime)
{
    // Handle pause input
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        ResumeGame();
    }
    if (IsKeyPressed(KEY_Q)) {
        EndGame();
    }
}

// State-specific render methods
void StateManager::RenderMenu()
{
    ClearBackground(DARKBLUE);

    DrawText("PaintSplash", GetScreenWidth()/2 - 150, GetScreenHeight()/2 - 100, 40, WHITE);
    DrawText("Press ENTER to Start Game", GetScreenWidth()/2 - 150, GetScreenHeight()/2 - 20, 20, WHITE);
    DrawText("Press L for Lobby", GetScreenWidth()/2 - 100, GetScreenHeight()/2 + 20, 20, WHITE);
    DrawText("Press ESC to Quit", GetScreenWidth()/2 - 100, GetScreenHeight()/2 + 60, 20, WHITE);
}

void StateManager::RenderLobby()
{
    ClearBackground(DARKGREEN);

    DrawText("Lobby", GetScreenWidth()/2 - 50, GetScreenHeight()/2 - 50, 30, WHITE);
    DrawText("Waiting for players...", GetScreenWidth()/2 - 100, GetScreenHeight()/2, 20, WHITE);
    DrawText("Press ESC to return to Menu", GetScreenWidth()/2 - 120, GetScreenHeight()/2 + 50, 20, WHITE);
}

void StateManager::RenderLoading()
{
    ClearBackground(BLACK);

    DrawText("Loading...", GetScreenWidth()/2 - 60, GetScreenHeight()/2, 30, WHITE);
    // TODO: Add loading progress bar
}

void StateManager::RenderGame()
{
    // Don't clear background here - let the ECS renderer handle it
    // The ECS RenderSystem will handle all 3D rendering
}

void StateManager::RenderPaused()
{
    // First render the game underneath
    RenderGame();

    // Then overlay pause menu
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));

    DrawText("PAUSED", GetScreenWidth()/2 - 60, GetScreenHeight()/2 - 50, 30, WHITE);
    DrawText("Press ESC or P to Resume", GetScreenWidth()/2 - 120, GetScreenHeight()/2, 20, WHITE);
    DrawText("Press Q to Quit to Menu", GetScreenWidth()/2 - 110, GetScreenHeight()/2 + 30, 20, WHITE);
}
