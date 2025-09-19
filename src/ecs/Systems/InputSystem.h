#pragma once

#include "../System.h"
#include "../../input/Input.h"
#include "raylib.h"
#include <unordered_map>
#include <functional>

enum class InputAction {
    // 3D Movement
    MOVE_FORWARD,    // W key - move forward
    MOVE_BACKWARD,   // S key - move backward  
    STRAFE_LEFT,     // A key - strafe left
    STRAFE_RIGHT,    // D key - strafe right
    MOVE_UP,         // Space - move up (fly up)
    MOVE_DOWN,       // Cmd/Alt - move down (fly down)
    
    // Legacy 2D movement (kept for compatibility)
    MOVE_2D_UP,
    MOVE_2D_DOWN,
    MOVE_2D_LEFT,
    MOVE_2D_RIGHT,
    
    // Look controls
    LOOK_UP,
    LOOK_DOWN,
    LOOK_LEFT,
    LOOK_RIGHT,
    
    // Actions
    JUMP,
    SHOOT,
    PAUSE,
    CONFIRM,
    CANCEL,
    RUN,             // Shift - run/sprint
    CROUCH,          // Ctrl - crouch
    
    CUSTOM_START = 1000
};

struct InputState {
    // 3D Movement
    bool moveForward = false;
    bool moveBackward = false;
    bool strafeLeft = false;
    bool strafeRight = false;
    bool moveUp = false;
    bool moveDown = false;
    
    // Legacy 2D movement (kept for compatibility)
    bool move2DUp = false;
    bool move2DDown = false;
    bool move2DLeft = false;
    bool move2DRight = false;

    // Look (mouse)
    bool lookUp = false;
    bool lookDown = false;
    bool lookLeft = false;
    bool lookRight = false;

    // Actions
    bool jump = false;
    bool shoot = false;
    bool pause = false;
    bool confirm = false;
    bool cancel = false;
    bool run = false;
    bool crouch = false;

    // Mouse
    Vector2 mousePosition = {0, 0};
    Vector2 mouseDelta = {0, 0};
    bool leftMousePressed = false;
    bool rightMousePressed = false;
    bool leftMouseDown = false;
    bool rightMouseDown = false;

    // Clear all states
    void Clear() {
        moveForward = moveBackward = strafeLeft = strafeRight = false;
        moveUp = moveDown = false;
        move2DUp = move2DDown = move2DLeft = move2DRight = false;
        lookUp = lookDown = lookLeft = lookRight = false;
        jump = shoot = pause = confirm = cancel = run = crouch = false;
        mouseDelta = {0, 0};
        leftMousePressed = rightMousePressed = false;
    }
};

class InputSystem : public System {
public:
    InputSystem();
    ~InputSystem();

    void Update(float deltaTime) override;

    // Input state access
    const InputState& GetInputState() const { return currentState_; }
    const InputState& GetPreviousState() const { return previousState_; }

    // Key mapping
    void MapKey(int key, InputAction action);
    void UnmapKey(int key);

    // Mouse sensitivity and smoothing
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }

    void SetMouseSmoothingFactor(float factor) { mouseSmoothingFactor_ = std::max(0.0f, std::min(1.0f, factor)); }
    float GetMouseSmoothingFactor() const { return mouseSmoothingFactor_; }

    void SetMouseAcceleration(float acceleration) { mouseAcceleration_ = std::max(0.1f, acceleration); }
    float GetMouseAcceleration() const { return mouseAcceleration_; }

    // Enhanced mouse functions
    void EnableMouseSmoothing() { useMouseSmoothing_ = true; }
    void DisableMouseSmoothing() { useMouseSmoothing_ = false; }
    bool IsMouseSmoothingEnabled() const { return useMouseSmoothing_; }

    bool IsMouseInitialized() const { return mouseInitialized_; }

    // Input queries (convenience methods)
    bool IsActionPressed(InputAction action) const;
    bool IsActionDown(InputAction action) const;
    bool IsActionReleased(InputAction action) const;

    Vector2 GetMovementVector() const; // Legacy 2D movement vector
    Vector3 Get3DMovementVector() const; // 3D movement vector (forward/back, strafe, up/down)
    Vector2 GetLookVector() const; // Mouse look vector
    Vector2 GetMousePosition() const { return currentState_.mousePosition; }
    Vector2 GetMouseDelta() const { return currentState_.mouseDelta; }

private:
    InputState currentState_;
    InputState previousState_;

    std::unordered_map<int, InputAction> keyMappings_;

    // Mouse control variables
    float mouseSensitivity_;
    bool useMouseSmoothing_;
    float mouseSmoothingFactor_;    // Exponential smoothing factor (0-1)
    float mouseAcceleration_;       // Acceleration curve power

    // Smoothing state
    Vector2 previousSmoothedDelta_;
    Vector2 accumulatedMouseDelta_;
    int framesSinceLastInput_;
    bool mouseInitialized_;

    void UpdateKeyboard();
    void UpdateMouse();
    void SetupDefaultMappings();

    // Helper functions for mouse processing
    Vector2 ApplyMouseSmoothing(Vector2 rawDelta, float deltaTime);
    Vector2 ApplyMouseAcceleration(Vector2 delta);
    void InitializeMouseState();
};
