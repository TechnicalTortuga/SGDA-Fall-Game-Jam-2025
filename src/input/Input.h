#pragma once

#include "raylib.h"

/**
 * Input Module - High-level wrapper for raylib input functions
 * Provides centralized input polling and state management
 */
class Input {
public:
    // Mouse input functions
    static Vector2 GetMousePosition();
    static Vector2 GetMouseDelta();
    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonDown(int button);
    static bool IsMouseButtonReleased(int button);
    static float GetMouseWheelMove();
    
    // Enhanced mouse functions
    static void SetMouseSensitivity(float sensitivity);
    static float GetMouseSensitivity();
    static void ResetMousePosition(); // Centers mouse for FPS controls
    static Vector2 GetSmoothedMouseDelta(); // Smoothed delta for less jittery movement
    
    // Keyboard input functions
    static bool IsKeyPressed(int key);
    static bool IsKeyDown(int key);
    static bool IsKeyReleased(int key);
    static bool IsKeyUp(int key);
    
    // Gamepad input functions (for future expansion)
    static bool IsGamepadAvailable(int gamepad);
    static Vector2 GetGamepadAxisMovement(int gamepad, int axis);
    static bool IsGamepadButtonPressed(int gamepad, int button);
    static bool IsGamepadButtonDown(int gamepad, int button);
    
    // Input state management
    static void Initialize();
    static void Update(float deltaTime);
    static void Shutdown();
    
    // Window/cursor management
    static void EnableCursor();
    static void DisableCursor();
    static bool IsCursorHidden();
    static void SetCursorPosition(int x, int y);
    
    // Input filtering and processing
    static void SetDeadzone(float deadzone); // For analog inputs
    static float GetDeadzone();
    
private:
    static float mouseSensitivity_;
    static Vector2 previousMouseDelta_;
    static float smoothingFactor_;
    static float deadzone_;
    static bool initialized_;
    static bool cursorHidden_;
    
    // Helper functions
    static Vector2 ApplyDeadzone(Vector2 input);
    static Vector2 SmoothMouseDelta(Vector2 currentDelta);
};