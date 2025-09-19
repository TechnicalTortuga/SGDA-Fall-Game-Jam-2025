#pragma once

#include <memory>
#include <unordered_map>
#include <string>

class EventManager;

enum class GameState {
    NONE,
    MENU,
    LOBBY,
    LOADING,
    GAME,
    PAUSED
};

class StateManager {
public:
    StateManager(EventManager* eventManager);
    ~StateManager();

    // State management
    void SwitchState(GameState newState);
    GameState GetCurrentState() const { return currentState_; }

    // State information
    std::string GetStateName(GameState state) const;
    bool IsInGame() const { return currentState_ == GameState::GAME; }
    bool IsPaused() const { return currentState_ == GameState::PAUSED; }

    // Update and render (delegated to current state)
    void Update(float deltaTime);
    void Render();

    // State-specific methods
    void StartGame();
    void PauseGame();
    void ResumeGame();
    void EndGame();
    void ShowMenu();
    void ShowLobby();

private:
    EventManager* eventManager_;
    GameState currentState_;
    GameState previousState_; // For resume functionality

    // State-specific update/render functions
    void UpdateMenu(float deltaTime);
    void UpdateLobby(float deltaTime);
    void UpdateLoading(float deltaTime);
    void UpdateGame(float deltaTime);
    void UpdatePaused(float deltaTime);

    void RenderMenu();
    void RenderLobby();
    void RenderLoading();
    void RenderGame();
    void RenderPaused();
};
