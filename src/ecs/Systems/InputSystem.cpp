#include "InputSystem.h"
#include "../../utils/Logger.h"
#include <cmath>
// PI is already defined by raylib, no need to redefine

InputSystem::InputSystem()
    : mouseSensitivity_(0.008f)   // Higher sensitivity for more responsive feel
    , useMouseSmoothing_(true)    // Enable smoothing for better FPS feel
    , mouseSmoothingFactor_(0.75f) // Slightly less smoothing for better responsiveness
    , mouseAcceleration_(1.0f)     // Linear acceleration (can be adjusted)
    , previousSmoothedDelta_({0.0f, 0.0f})
    , accumulatedMouseDelta_({0.0f, 0.0f})
    , framesSinceLastInput_(0)
    , mouseInitialized_(false)
{
    // Initialize the Input module
    Input::Initialize();
    Input::SetMouseSensitivity(1.0f); // Let us handle sensitivity ourselves

    SetupDefaultMappings();

    // Wait a couple frames and reset mouse to avoid initial movement artifacts
    LOG_INFO("InputSystem initialized - waiting for mouse stabilization...");
}

InputSystem::~InputSystem()
{
    Input::Shutdown();
    LOG_INFO("InputSystem destroyed");
}

void InputSystem::Update(float deltaTime)
{
    LOG_DEBUG("InputSystem::Update called with deltaTime: " + std::to_string(deltaTime));

    // Store previous state
    previousState_ = currentState_;

    // Clear current state (except for continuous inputs)
    currentState_.Clear();

    // Update input states
    LOG_DEBUG("About to call UpdateKeyboard");
    UpdateKeyboard();
    LOG_DEBUG("Keyboard updated, about to call UpdateMouse");
    UpdateMouse();
    LOG_DEBUG("Mouse updated");

    LOG_DEBUG("InputSystem::Update completed");
}

void InputSystem::UpdateKeyboard()
{
    // Check mapped keys
    for (const auto& mapping : keyMappings_) {
        int key = mapping.first;
        InputAction action = mapping.second;

        bool isPressed = IsKeyPressed(key);
        bool isDown = IsKeyDown(key);

        switch (action) {
            // 3D Movement
            case InputAction::MOVE_FORWARD:
                currentState_.moveForward = isDown;
                break;
            case InputAction::MOVE_BACKWARD:
                currentState_.moveBackward = isDown;
                break;
            case InputAction::STRAFE_LEFT:
                currentState_.strafeLeft = isDown;
                break;
            case InputAction::STRAFE_RIGHT:
                currentState_.strafeRight = isDown;
                break;
            case InputAction::MOVE_UP:
                currentState_.moveUp = isDown;
                break;
            case InputAction::MOVE_DOWN:
                currentState_.moveDown = isDown;
                break;
                
            // Legacy 2D movement
            case InputAction::MOVE_2D_UP:
                currentState_.move2DUp = isDown;
                break;
            case InputAction::MOVE_2D_DOWN:
                currentState_.move2DDown = isDown;
                break;
            case InputAction::MOVE_2D_LEFT:
                currentState_.move2DLeft = isDown;
                break;
            case InputAction::MOVE_2D_RIGHT:
                currentState_.move2DRight = isDown;
                break;
                
            // Actions
            case InputAction::JUMP:
                currentState_.jump = isPressed;
                break;
            case InputAction::SHOOT:
                currentState_.shoot = isPressed;
                break;
            case InputAction::PAUSE:
                currentState_.pause = isPressed;
                break;
            case InputAction::CONFIRM:
                currentState_.confirm = isPressed;
                break;
            case InputAction::CANCEL:
                currentState_.cancel = isPressed;
                break;
            case InputAction::RUN:
                currentState_.run = isDown;
                break;
            case InputAction::CROUCH:
                currentState_.crouch = isDown;
                break;
            default:
                break;
        }
    }
}

void InputSystem::UpdateMouse()
{
    LOG_DEBUG("UpdateMouse() STARTED");

    // Initialize mouse state if not done yet
    if (!mouseInitialized_) {
        InitializeMouseState();
        currentState_.mouseDelta = {0.0f, 0.0f};
        LOG_DEBUG("Mouse not yet initialized, skipping update");
        return;
    }

    // Update mouse position using our Input module
    currentState_.mousePosition = Input::GetMousePosition();
    LOG_DEBUG("Mouse position: (" + std::to_string(currentState_.mousePosition.x) + ", " +
              std::to_string(currentState_.mousePosition.y) + ")");

    // Get raw mouse delta from raylib
    Vector2 rawMouseDelta = Input::GetMouseDelta();
    LOG_DEBUG("Raw raylib mouse delta: (" + std::to_string(rawMouseDelta.x) + ", " +
              std::to_string(rawMouseDelta.y) + ")");

    // Process mouse delta with smoothing and sensitivity
    Vector2 processedDelta = rawMouseDelta;

    // Apply sensitivity first (reduce default sensitivity for better control)
    processedDelta.x *= mouseSensitivity_ * 0.3f;
    processedDelta.y *= mouseSensitivity_ * 0.3f;

    // Apply acceleration curve (makes small movements more precise, large movements more responsive)
    processedDelta = ApplyMouseAcceleration(processedDelta);

    // Apply exponential smoothing for fluid movement
    if (useMouseSmoothing_) {
        processedDelta = ApplyMouseSmoothing(processedDelta, 1.0f/60.0f); // Assume 60 FPS for smoothing
    }

    // Store the processed delta
    currentState_.mouseDelta = processedDelta;

    // Clamp extreme values to prevent camera glitches (but allow reasonable fast movements)
    const float maxMouseDelta = 2.0f;  // Much more reasonable limit
    if (currentState_.mouseDelta.x > maxMouseDelta) currentState_.mouseDelta.x = maxMouseDelta;
    if (currentState_.mouseDelta.x < -maxMouseDelta) currentState_.mouseDelta.x = -maxMouseDelta;
    if (currentState_.mouseDelta.y > maxMouseDelta) currentState_.mouseDelta.y = maxMouseDelta;
    if (currentState_.mouseDelta.y < -maxMouseDelta) currentState_.mouseDelta.y = -maxMouseDelta;

    LOG_DEBUG("Processed mouse delta: (" + std::to_string(currentState_.mouseDelta.x) + ", " +
              std::to_string(currentState_.mouseDelta.y) + ")");

    // Update mouse button states using our Input module
    currentState_.leftMousePressed = Input::IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    currentState_.rightMousePressed = Input::IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    currentState_.leftMouseDown = Input::IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    currentState_.rightMouseDown = Input::IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    LOG_DEBUG("Mouse buttons - Left down: " + std::to_string(currentState_.leftMouseDown) +
              ", Right down: " + std::to_string(currentState_.rightMouseDown));

    // Log significant mouse movement
    if (abs(currentState_.mouseDelta.x) > 0.001f || abs(currentState_.mouseDelta.y) > 0.001f) {
        LOG_DEBUG("*** MOUSE MOVEMENT DETECTED *** Raw: (" + std::to_string(rawMouseDelta.x) + ", " +
                  std::to_string(rawMouseDelta.y) + ") Processed: (" +
                  std::to_string(currentState_.mouseDelta.x) + ", " +
                  std::to_string(currentState_.mouseDelta.y) + ")");

        framesSinceLastInput_ = 0;

        // Re-center mouse for FPS controls using our Input module
        Input::ResetMousePosition();
    } else {
        framesSinceLastInput_++;
        LOG_DEBUG("No mouse movement detected (delta below threshold), frames since input: " +
                  std::to_string(framesSinceLastInput_));
    }

    // Update look directions based on processed mouse delta
    // Use smaller thresholds since we're now properly smoothed
    const float lookThreshold = 0.01f;

    if (currentState_.mouseDelta.x > lookThreshold) {
        currentState_.lookRight = true;
        currentState_.lookLeft = false;
    } else if (currentState_.mouseDelta.x < -lookThreshold) {
        currentState_.lookLeft = true;
        currentState_.lookRight = false;
    } else {
        currentState_.lookLeft = false;
        currentState_.lookRight = false;
    }

    if (currentState_.mouseDelta.y > lookThreshold) {
        currentState_.lookDown = true;
        currentState_.lookUp = false;
    } else if (currentState_.mouseDelta.y < -lookThreshold) {
        currentState_.lookUp = true;
        currentState_.lookDown = false;
    } else {
        currentState_.lookUp = false;
        currentState_.lookDown = false;
    }
}

void InputSystem::SetupDefaultMappings()
{
    // 3D WASD movement (FPS style)
    MapKey(KEY_W, InputAction::MOVE_FORWARD);
    MapKey(KEY_S, InputAction::MOVE_BACKWARD);
    MapKey(KEY_A, InputAction::STRAFE_LEFT);
    MapKey(KEY_D, InputAction::STRAFE_RIGHT);
    
    // Vertical movement
    MapKey(KEY_SPACE, InputAction::MOVE_UP);        // Fly up
    MapKey(KEY_LEFT_SUPER, InputAction::MOVE_DOWN); // Fly down (Mac Cmd key)
    MapKey(KEY_LEFT_ALT, InputAction::MOVE_DOWN);   // Fly down (Alt key alternative)
    MapKey(KEY_C, InputAction::MOVE_DOWN);          // C key as additional down option
    
    // Movement modifiers
    MapKey(KEY_LEFT_SHIFT, InputAction::RUN);       // Sprint/run
    MapKey(KEY_LEFT_CONTROL, InputAction::CROUCH);  // Crouch
    
    // Actions
    MapKey(KEY_E, InputAction::SHOOT);              // Shoot/interact
    MapKey(KEY_ESCAPE, InputAction::PAUSE);         // Pause
    MapKey(KEY_ENTER, InputAction::CONFIRM);        // Confirm
    MapKey(KEY_BACKSPACE, InputAction::CANCEL);     // Cancel
    
    // Legacy 2D mappings (for compatibility)
    MapKey(KEY_UP, InputAction::MOVE_2D_UP);
    MapKey(KEY_DOWN, InputAction::MOVE_2D_DOWN);
    MapKey(KEY_LEFT, InputAction::MOVE_2D_LEFT);
    MapKey(KEY_RIGHT, InputAction::MOVE_2D_RIGHT);

    LOG_DEBUG("3D FPS input mappings set up - WASD for movement, Space/Cmd for up/down");
}

void InputSystem::MapKey(int key, InputAction action)
{
    keyMappings_[key] = action;
    LOG_DEBUG("Mapped key " + std::to_string(key) + " to action " + std::to_string(static_cast<int>(action)));
}

void InputSystem::UnmapKey(int key)
{
    auto it = keyMappings_.find(key);
    if (it != keyMappings_.end()) {
        keyMappings_.erase(it);
        LOG_DEBUG("Unmapped key " + std::to_string(key));
    }
}

bool InputSystem::IsActionPressed(InputAction action) const
{
    switch (action) {
        // 3D Movement (pressed once)
        case InputAction::MOVE_FORWARD: return currentState_.moveForward && !previousState_.moveForward;
        case InputAction::MOVE_BACKWARD: return currentState_.moveBackward && !previousState_.moveBackward;
        case InputAction::STRAFE_LEFT: return currentState_.strafeLeft && !previousState_.strafeLeft;
        case InputAction::STRAFE_RIGHT: return currentState_.strafeRight && !previousState_.strafeRight;
        case InputAction::MOVE_UP: return currentState_.moveUp && !previousState_.moveUp;
        case InputAction::MOVE_DOWN: return currentState_.moveDown && !previousState_.moveDown;
        
        // Legacy 2D movement
        case InputAction::MOVE_2D_UP: return currentState_.move2DUp && !previousState_.move2DUp;
        case InputAction::MOVE_2D_DOWN: return currentState_.move2DDown && !previousState_.move2DDown;
        case InputAction::MOVE_2D_LEFT: return currentState_.move2DLeft && !previousState_.move2DLeft;
        case InputAction::MOVE_2D_RIGHT: return currentState_.move2DRight && !previousState_.move2DRight;
        
        // Actions
        case InputAction::JUMP: return currentState_.jump;
        case InputAction::SHOOT: return currentState_.shoot;
        case InputAction::PAUSE: return currentState_.pause;
        case InputAction::CONFIRM: return currentState_.confirm;
        case InputAction::CANCEL: return currentState_.cancel;
        case InputAction::RUN: return currentState_.run && !previousState_.run;
        case InputAction::CROUCH: return currentState_.crouch && !previousState_.crouch;
        default: return false;
    }
}

bool InputSystem::IsActionDown(InputAction action) const
{
    switch (action) {
        // 3D Movement (held down)
        case InputAction::MOVE_FORWARD: return currentState_.moveForward;
        case InputAction::MOVE_BACKWARD: return currentState_.moveBackward;
        case InputAction::STRAFE_LEFT: return currentState_.strafeLeft;
        case InputAction::STRAFE_RIGHT: return currentState_.strafeRight;
        case InputAction::MOVE_UP: return currentState_.moveUp;
        case InputAction::MOVE_DOWN: return currentState_.moveDown;
        
        // Legacy 2D movement
        case InputAction::MOVE_2D_UP: return currentState_.move2DUp;
        case InputAction::MOVE_2D_DOWN: return currentState_.move2DDown;
        case InputAction::MOVE_2D_LEFT: return currentState_.move2DLeft;
        case InputAction::MOVE_2D_RIGHT: return currentState_.move2DRight;
        
        // Look
        case InputAction::LOOK_UP: return currentState_.lookUp;
        case InputAction::LOOK_DOWN: return currentState_.lookDown;
        case InputAction::LOOK_LEFT: return currentState_.lookLeft;
        case InputAction::LOOK_RIGHT: return currentState_.lookRight;
        
        // Actions
        case InputAction::RUN: return currentState_.run;
        case InputAction::CROUCH: return currentState_.crouch;
        default: return false;
    }
}

bool InputSystem::IsActionReleased(InputAction action) const
{
    switch (action) {
        // 3D Movement
        case InputAction::MOVE_FORWARD: return !currentState_.moveForward && previousState_.moveForward;
        case InputAction::MOVE_BACKWARD: return !currentState_.moveBackward && previousState_.moveBackward;
        case InputAction::STRAFE_LEFT: return !currentState_.strafeLeft && previousState_.strafeLeft;
        case InputAction::STRAFE_RIGHT: return !currentState_.strafeRight && previousState_.strafeRight;
        case InputAction::MOVE_UP: return !currentState_.moveUp && previousState_.moveUp;
        case InputAction::MOVE_DOWN: return !currentState_.moveDown && previousState_.moveDown;
        
        // Legacy 2D movement
        case InputAction::MOVE_2D_UP: return !currentState_.move2DUp && previousState_.move2DUp;
        case InputAction::MOVE_2D_DOWN: return !currentState_.move2DDown && previousState_.move2DDown;
        case InputAction::MOVE_2D_LEFT: return !currentState_.move2DLeft && previousState_.move2DLeft;
        case InputAction::MOVE_2D_RIGHT: return !currentState_.move2DRight && previousState_.move2DRight;
        
        // Actions
        case InputAction::RUN: return !currentState_.run && previousState_.run;
        case InputAction::CROUCH: return !currentState_.crouch && previousState_.crouch;
        default: return false;
    }
}

Vector2 InputSystem::GetMovementVector() const
{
    // Legacy 2D movement vector for backward compatibility
    Vector2 movement = {0, 0};

    if (currentState_.move2DUp) movement.y -= 1.0f;
    if (currentState_.move2DDown) movement.y += 1.0f;
    if (currentState_.move2DLeft) movement.x -= 1.0f;
    if (currentState_.move2DRight) movement.x += 1.0f;

    // Normalize if moving diagonally
    if (movement.x != 0 && movement.y != 0) {
        float length = sqrt(movement.x * movement.x + movement.y * movement.y);
        if (length > 0) {
            movement.x /= length;
            movement.y /= length;
        }
    }

    return movement;
}

Vector3 InputSystem::Get3DMovementVector() const
{
    Vector3 movement = {0, 0, 0};

    // Forward/Backward movement (Z axis) - FIXED: swap forward/backward
    if (currentState_.moveForward) movement.z += 1.0f;   // FIXED: Positive Z is forward 
    if (currentState_.moveBackward) movement.z -= 1.0f;  // FIXED: Negative Z is backward
    
    // Strafe movement (X axis) - FIXED: swap left/right
    if (currentState_.strafeLeft) movement.x += 1.0f;    // FIXED: Positive X is left
    if (currentState_.strafeRight) movement.x -= 1.0f;   // FIXED: Negative X is right
    
    // Vertical movement (Y axis)
    if (currentState_.moveUp) movement.y += 1.0f;        // Positive Y is up
    if (currentState_.moveDown) movement.y -= 1.0f;      // Negative Y is down

    // Normalize horizontal movement if moving diagonally (don't normalize Y)
    float horizontalLength = sqrt(movement.x * movement.x + movement.z * movement.z);
    if (horizontalLength > 0) {
        movement.x /= horizontalLength;
        movement.z /= horizontalLength;
    }

    return movement;
}

Vector2 InputSystem::GetLookVector() const
{
    Vector2 look = {0, 0};

    if (currentState_.lookUp) look.y -= 1.0f;
    if (currentState_.lookDown) look.y += 1.0f;
    if (currentState_.lookLeft) look.x -= 1.0f;
    if (currentState_.lookRight) look.x += 1.0f;

    // Normalize if looking diagonally
    if (look.x != 0 && look.y != 0) {
        float length = sqrt(look.x * look.x + look.y * look.y);
        if (length > 0) {
            look.x /= length;
            look.y /= length;
        }
    }

    return look;
}

// Helper functions for mouse processing
Vector2 InputSystem::ApplyMouseSmoothing(Vector2 rawDelta, float deltaTime)
{
    // Exponential smoothing: new_value = old_value * factor + new_input * (1 - factor)
    // Higher smoothingFactor_ = smoother movement, lower = more responsive

    // Scale smoothing factor by deltaTime for frame-rate independence
    float adjustedSmoothing = powf(mouseSmoothingFactor_, 60.0f * deltaTime); // Assume 60 FPS baseline

    Vector2 smoothed;
    smoothed.x = previousSmoothedDelta_.x * adjustedSmoothing + rawDelta.x * (1.0f - adjustedSmoothing);
    smoothed.y = previousSmoothedDelta_.y * adjustedSmoothing + rawDelta.y * (1.0f - adjustedSmoothing);

    // Update previous smoothed value for next frame
    previousSmoothedDelta_ = smoothed;

    return smoothed;
}

Vector2 InputSystem::ApplyMouseAcceleration(Vector2 delta)
{
    // Apply acceleration curve: small movements stay precise, large movements get more responsive
    // This creates a natural feel similar to professional FPS games

    Vector2 accelerated = delta;

    // Calculate magnitude
    float magnitude = sqrtf(delta.x * delta.x + delta.y * delta.y);

    if (magnitude > 0.001f) { // Avoid division by zero
        // Apply power curve: values < 1 become smaller, values > 1 become larger
        float acceleratedMagnitude = powf(magnitude, mouseAcceleration_);

        // Normalize and scale
        accelerated.x = (delta.x / magnitude) * acceleratedMagnitude;
        accelerated.y = (delta.y / magnitude) * acceleratedMagnitude;
    }

    return accelerated;
}

void InputSystem::InitializeMouseState()
{
    // Wait a few frames to ensure the window and cursor are properly initialized
    static int initializationFrames = 0;
    initializationFrames++;

    if (initializationFrames < 3) {
        LOG_DEBUG("Waiting for mouse initialization, frame " + std::to_string(initializationFrames));
        return;
    }

    // Reset mouse position and clear any accumulated deltas
    Input::ResetMousePosition();

    // Wait another frame to let the reset take effect
    if (initializationFrames < 5) {
        LOG_DEBUG("Resetting mouse position, frame " + std::to_string(initializationFrames));
        return;
    }

    // Clear all smoothing state
    previousSmoothedDelta_ = {0.0f, 0.0f};
    accumulatedMouseDelta_ = {0.0f, 0.0f};
    framesSinceLastInput_ = 0;

    // Get another mouse delta to clear any reset artifacts
    Input::GetMouseDelta();
    Input::ResetMousePosition();

    mouseInitialized_ = true;
    LOG_INFO("Mouse input system fully initialized and stabilized");
}
