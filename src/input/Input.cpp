#include "Input.h"
#include "../utils/Logger.h"
#include <cmath>

// Static member initialization
float Input::mouseSensitivity_ = 0.15f;
Vector2 Input::previousMouseDelta_ = {0.0f, 0.0f};
float Input::smoothingFactor_ = 0.15f; // Higher = more smoothing
float Input::deadzone_ = 0.1f;
bool Input::initialized_ = false;
bool Input::cursorHidden_ = false;

void Input::Initialize() {
    if (initialized_) {
        LOG_WARNING("Input module already initialized");
        return;
    }
    
    mouseSensitivity_ = 0.15f;
    previousMouseDelta_ = {0.0f, 0.0f};
    smoothingFactor_ = 0.15f;
    deadzone_ = 0.1f;
    cursorHidden_ = false;
    
    LOG_INFO("Input module initialized");
    initialized_ = true;
}

void Input::Update(float deltaTime) {
    if (!initialized_) {
        LOG_ERROR("Input module not initialized");
        return;
    }
    
    // Update internal state if needed
    // For now, this is mainly used for smoothing calculations
}

void Input::Shutdown() {
    if (!initialized_) return;
    
    // Restore cursor if hidden
    if (cursorHidden_) {
        EnableCursor();
    }
    
    LOG_INFO("Input module shutdown");
    initialized_ = false;
}

// Mouse input functions
Vector2 Input::GetMousePosition() {
    return ::GetMousePosition(); // Use raylib function
}

Vector2 Input::GetMouseDelta() {
    return ::GetMouseDelta(); // Use raylib function
}

bool Input::IsMouseButtonPressed(int button) {
    return ::IsMouseButtonPressed(button);
}

bool Input::IsMouseButtonDown(int button) {
    return ::IsMouseButtonDown(button);
}

bool Input::IsMouseButtonReleased(int button) {
    return ::IsMouseButtonReleased(button);
}

float Input::GetMouseWheelMove() {
    return ::GetMouseWheelMove();
}

// Enhanced mouse functions
void Input::SetMouseSensitivity(float sensitivity) {
    mouseSensitivity_ = sensitivity;
    LOG_DEBUG("Mouse sensitivity set to: " + std::to_string(sensitivity));
}

float Input::GetMouseSensitivity() {
    return mouseSensitivity_;
}

void Input::ResetMousePosition() {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    
    ::SetMousePosition(centerX, centerY);
    LOG_DEBUG("Mouse position reset to center: (" + std::to_string(centerX) + ", " + std::to_string(centerY) + ")");
}

Vector2 Input::GetSmoothedMouseDelta() {
    Vector2 currentDelta = GetMouseDelta();
    
    // Apply sensitivity
    currentDelta.x *= mouseSensitivity_;
    currentDelta.y *= mouseSensitivity_;
    
    // Smooth the delta using linear interpolation
    Vector2 smoothedDelta = SmoothMouseDelta(currentDelta);
    
    // Update previous delta for next frame
    previousMouseDelta_ = smoothedDelta;
    
    return smoothedDelta;
}

// Keyboard input functions
bool Input::IsKeyPressed(int key) {
    return ::IsKeyPressed(key);
}

bool Input::IsKeyDown(int key) {
    return ::IsKeyDown(key);
}

bool Input::IsKeyReleased(int key) {
    return ::IsKeyReleased(key);
}

bool Input::IsKeyUp(int key) {
    return ::IsKeyUp(key);
}

// Gamepad input functions
bool Input::IsGamepadAvailable(int gamepad) {
    return ::IsGamepadAvailable(gamepad);
}

Vector2 Input::GetGamepadAxisMovement(int gamepad, int axis) {
    Vector2 movement = {0.0f, 0.0f};
    
    if (IsGamepadAvailable(gamepad)) {
        movement.x = ::GetGamepadAxisMovement(gamepad, axis);
        movement.y = ::GetGamepadAxisMovement(gamepad, axis + 1); // Assuming Y is next axis
        
        // Apply deadzone
        movement = ApplyDeadzone(movement);
    }
    
    return movement;
}

bool Input::IsGamepadButtonPressed(int gamepad, int button) {
    return ::IsGamepadButtonPressed(gamepad, button);
}

bool Input::IsGamepadButtonDown(int gamepad, int button) {
    return ::IsGamepadButtonDown(gamepad, button);
}

// Window/cursor management
void Input::EnableCursor() {
    ::EnableCursor();
    cursorHidden_ = false;
    LOG_DEBUG("Cursor enabled");
}

void Input::DisableCursor() {
    ::DisableCursor();
    cursorHidden_ = true;
    LOG_DEBUG("Cursor disabled for FPS controls");
}

bool Input::IsCursorHidden() {
    return cursorHidden_;
}

void Input::SetCursorPosition(int x, int y) {
    ::SetMousePosition(x, y);
}

// Input filtering and processing
void Input::SetDeadzone(float deadzone) {
    deadzone_ = std::max(0.0f, std::min(1.0f, deadzone)); // Clamp between 0 and 1
    LOG_DEBUG("Deadzone set to: " + std::to_string(deadzone_));
}

float Input::GetDeadzone() {
    return deadzone_;
}

// Helper functions
Vector2 Input::ApplyDeadzone(Vector2 input) {
    float magnitude = std::sqrt(input.x * input.x + input.y * input.y);
    
    if (magnitude < deadzone_) {
        return {0.0f, 0.0f}; // Inside deadzone, return zero
    }
    
    // Scale the input to start at the deadzone boundary
    float scaledMagnitude = (magnitude - deadzone_) / (1.0f - deadzone_);
    Vector2 normalized = {input.x / magnitude, input.y / magnitude};
    
    return {normalized.x * scaledMagnitude, normalized.y * scaledMagnitude};
}

Vector2 Input::SmoothMouseDelta(Vector2 currentDelta) {
    // Linear interpolation for smoothing
    Vector2 smoothed;
    smoothed.x = previousMouseDelta_.x + (currentDelta.x - previousMouseDelta_.x) * smoothingFactor_;
    smoothed.y = previousMouseDelta_.y + (currentDelta.y - previousMouseDelta_.y) * smoothingFactor_;
    
    return smoothed;
}