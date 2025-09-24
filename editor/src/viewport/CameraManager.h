#pragma once

#include <raylib.h>
#include <imgui.h>

class CameraManager {
public:
    enum class CameraMode {
        NAVIGATION,     // Standard navigation mode
        MOUSELOOK      // Z key FPS-style mouselook mode
    };

    enum class NavigationType {
        NONE,
        ORBIT,         // Spacebar + LMB
        PAN,           // Spacebar + RMB  
        STRAFE         // Spacebar + Both buttons
    };

    CameraManager();
    ~CameraManager() = default;

    // Camera control
    void Update(float deltaTime, bool isViewportHovered);
    void HandleInput(float deltaTime, bool isViewportHovered);
    
    // Mode management
    void ToggleMouseLookMode();
    void SetMouseLookMode(bool enabled);
    bool IsMouseLookMode() const { return currentMode_ == CameraMode::MOUSELOOK; }
    
    // Camera properties
    Vector3 GetPosition() const { return position_; }
    Vector3 GetTarget() const;
    Vector3 GetUp() const { return up_; }
    
    void SetPosition(Vector3 position) { position_ = position; }
    void SetYawPitch(float yaw, float pitch);
    void SetMoveSpeed(float speed) { baseMoveSpeed_ = speed; }
    
    // Focus and framing
    void FocusOnPoint(Vector3 point, float distance = 10.0f);
    void FrameSelection(Vector3 center, Vector3 size);
    void ResetToDefault();
    
    // Distance-based speed scaling
    void SetDistanceBasedSpeed(bool enabled) { useDistanceBasedSpeed_ = enabled; }
    float GetCurrentMoveSpeed() const;
    
    // Camera matrix
    Matrix GetViewMatrix() const;
    Camera3D GetRaylibCamera() const;

    // Camera controls
    void Zoom(float factor);
    void Orbit(float deltaYaw, float deltaPitch);

private:
    // Camera state
    Vector3 position_;
    Vector3 target_;      // Point to orbit around
    float yaw_;           // Horizontal rotation (radians)
    float pitch_;         // Vertical rotation (radians)
    Vector3 up_;
    
    // Mode and interaction
    CameraMode currentMode_;
    NavigationType currentNavigation_;
    
    // Movement
    float baseMoveSpeed_;
    bool useDistanceBasedSpeed_;
    float distance_;      // Distance from focus point for speed calculation
    
    // Mouse look state
    bool mouseLookActive_;
    ImVec2 lastMousePos_;
    
    // Navigation state (spacebar combos)
    bool spacebarHeld_;
    bool leftMouseDown_;
    bool rightMouseDown_;
    ImVec2 navigationStartPos_;
    Vector3 navigationStartPosition_;
    float navigationStartYaw_;
    float navigationStartPitch_;
    
    // Sensitivity settings
    float mouseSensitivity_;
    float orbitSensitivity_;
    float panSensitivity_;
    
    // Helper methods
    void HandleMouseLookInput(float deltaTime);
    void HandleNavigationInput(float deltaTime);
    void HandleKeyboardMovement(float deltaTime);
    void UpdateCameraVectors();
    Vector3 GetForwardVector() const;
    Vector3 GetRightVector() const;
    
    // Navigation helpers
    void StartOrbitNavigation(ImVec2 mousePos);
    void StartPanNavigation(ImVec2 mousePos);
    void StartStrafeNavigation(ImVec2 mousePos);
    void UpdateOrbitNavigation(ImVec2 mousePos);
    void UpdatePanNavigation(ImVec2 mousePos);
    void UpdateStrafeNavigation(ImVec2 mousePos);
    void EndNavigation();
    
    // Constraints
    void ApplyPitchConstraints();
    void ClampPosition();
    
    static constexpr float MIN_PITCH = -PI * 0.49f;  // Slightly less than 90 degrees
    static constexpr float MAX_PITCH = PI * 0.49f;
    static constexpr float DEFAULT_MOVE_SPEED = 300.0f; // Units per second
    static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.005f;
    static constexpr float DEFAULT_DISTANCE = 10.0f;
};