#include "CameraManager.h"
#include <algorithm>
#include <cmath>
#include <raymath.h>

CameraManager::CameraManager()
    : position_({0.0f, 0.0f, -10.0f}), target_({0.0f, 0.0f, 0.0f}), yaw_(0.0f), pitch_(0.0f), up_({0.0f, 1.0f, 0.0f}),
      currentMode_(CameraMode::NAVIGATION), currentNavigation_(NavigationType::NONE),
      baseMoveSpeed_(DEFAULT_MOVE_SPEED), useDistanceBasedSpeed_(true), distance_(DEFAULT_DISTANCE),
      mouseLookActive_(false), lastMousePos_({0, 0}),
      spacebarHeld_(false), leftMouseDown_(false), rightMouseDown_(false),
      navigationStartPos_({0, 0}), navigationStartPosition_({0.0f, 0.0f, 0.0f}),
      navigationStartYaw_(0.0f), navigationStartPitch_(0.0f),
      mouseSensitivity_(DEFAULT_MOUSE_SENSITIVITY), orbitSensitivity_(0.01f), panSensitivity_(1.0f)
{
    UpdateCameraVectors();
}

void CameraManager::Update(float deltaTime, bool isViewportHovered)
{
    if (isViewportHovered) {
        HandleInput(deltaTime, isViewportHovered);
    }
    
    UpdateCameraVectors();
    ApplyPitchConstraints();
    ClampPosition();
}

void CameraManager::Zoom(float factor)
{
    // For 3D camera zoom, change the distance from target
    distance_ *= factor;

    // Clamp distance to reasonable bounds
    distance_ = std::max(0.1f, std::min(1000.0f, distance_));

    // Update position based on new distance
    UpdateCameraVectors();
}

void CameraManager::Orbit(float deltaYaw, float deltaPitch)
{
    // Update yaw and pitch for orbiting
    yaw_ += deltaYaw;
    pitch_ += deltaPitch;

    // Apply pitch constraints
    ApplyPitchConstraints();

    // Update position based on spherical coordinates around target
    UpdateCameraVectors();
}

void CameraManager::HandleInput(float deltaTime, bool isViewportHovered)
{
    if (!isViewportHovered) return;

    // Handle Z key toggle for mouse look mode
    if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
        ToggleMouseLookMode();
    }

    // Handle spacebar for navigation modes
    spacebarHeld_ = ImGui::IsKeyDown(ImGuiKey_Space);
    leftMouseDown_ = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    rightMouseDown_ = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    if (currentMode_ == CameraMode::MOUSELOOK) {
        HandleMouseLookInput(deltaTime);
    } else {
        HandleNavigationInput(deltaTime);
    }

    // Always handle keyboard movement
    HandleKeyboardMovement(deltaTime);

    // Handle double-click to reset view
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle)) {
        ResetToDefault();
    }
}

void CameraManager::HandleMouseLookInput(float deltaTime)
{
    if (!mouseLookActive_) {
        mouseLookActive_ = true;
        lastMousePos_ = ImGui::GetMousePos();
        ImGui::SetMouseCursor(ImGuiMouseCursor_None); // Hide cursor in mouselook
    }

    // Get mouse delta
    ImVec2 currentMousePos = ImGui::GetMousePos();
    ImVec2 mouseDelta = ImVec2(
        currentMousePos.x - lastMousePos_.x,
        currentMousePos.y - lastMousePos_.y
    );

    // Apply mouse look rotation
    yaw_ += mouseDelta.x * mouseSensitivity_;
    pitch_ -= mouseDelta.y * mouseSensitivity_; // Inverted Y

    lastMousePos_ = currentMousePos;

    // Handle WASD movement in FPS mode
    float moveSpeed = baseMoveSpeed_ * (useDistanceBasedSpeed_ ? (distance_ / DEFAULT_DISTANCE) : 1.0f);
    Vector3 moveDir = {0.0f, 0.0f, 0.0f};
    Vector3 forward = GetForwardVector();
    Vector3 right = GetRightVector();

    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
        moveDir.x += forward.x;
        moveDir.y += forward.y;
        moveDir.z += forward.z;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
        moveDir.x -= forward.x;
        moveDir.y -= forward.y;
        moveDir.z -= forward.z;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
        moveDir.x -= right.x;
        moveDir.y -= right.y;
        moveDir.z -= right.z;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
        moveDir.x += right.x;
        moveDir.y += right.y;
        moveDir.z += right.z;
    }

    // Apply movement
    if (moveDir.x != 0.0f || moveDir.y != 0.0f || moveDir.z != 0.0f) {
        // Normalize movement direction
        float length = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
        if (length > 0.0f) {
            moveDir.x /= length;
            moveDir.y /= length;
            moveDir.z /= length;

            position_.x += moveDir.x * moveSpeed * deltaTime;
            position_.y += moveDir.y * moveSpeed * deltaTime;
            position_.z += moveDir.z * moveSpeed * deltaTime;
        }
    }

    // Wrap mouse cursor to keep it in viewport (optional)
    // This would require more complex viewport boundary detection
}

void CameraManager::HandleNavigationInput(float deltaTime)
{
    if (mouseLookActive_) {
        mouseLookActive_ = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); // Restore cursor
    }

    ImVec2 currentMousePos = ImGui::GetMousePos();

    // Determine navigation type based on spacebar + mouse combinations
    NavigationType newNavType = NavigationType::NONE;
    if (spacebarHeld_) {
        if (leftMouseDown_ && rightMouseDown_) {
            newNavType = NavigationType::STRAFE;
        } else if (leftMouseDown_) {
            newNavType = NavigationType::ORBIT;
        } else if (rightMouseDown_) {
            newNavType = NavigationType::PAN;
        }
    }

    // Start new navigation if changed
    if (newNavType != currentNavigation_) {
        if (currentNavigation_ != NavigationType::NONE) {
            EndNavigation();
        }
        
        currentNavigation_ = newNavType;
        
        if (currentNavigation_ != NavigationType::NONE) {
            switch (currentNavigation_) {
                case NavigationType::ORBIT:
                    StartOrbitNavigation(currentMousePos);
                    break;
                case NavigationType::PAN:
                    StartPanNavigation(currentMousePos);
                    break;
                case NavigationType::STRAFE:
                    StartStrafeNavigation(currentMousePos);
                    break;
                default:
                    break;
            }
        }
    }

    // Update ongoing navigation
    if (currentNavigation_ != NavigationType::NONE) {
        switch (currentNavigation_) {
            case NavigationType::ORBIT:
                UpdateOrbitNavigation(currentMousePos);
                break;
            case NavigationType::PAN:
                UpdatePanNavigation(currentMousePos);
                break;
            case NavigationType::STRAFE:
                UpdateStrafeNavigation(currentMousePos);
                break;
            default:
                break;
        }
    }
}

void CameraManager::HandleKeyboardMovement(float deltaTime)
{
    float moveSpeed = GetCurrentMoveSpeed() * deltaTime;
    Vector3 forward = GetForwardVector();
    Vector3 right = GetRightVector();

    if (ImGui::IsKeyDown(ImGuiKey_W)) {
        position_.x += forward.x * moveSpeed;
        position_.y += forward.y * moveSpeed;
        position_.z += forward.z * moveSpeed;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S)) {
        position_.x -= forward.x * moveSpeed;
        position_.y -= forward.y * moveSpeed;
        position_.z -= forward.z * moveSpeed;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
        position_.x -= right.x * moveSpeed;
        position_.y -= right.y * moveSpeed;
        position_.z -= right.z * moveSpeed;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
        position_.x += right.x * moveSpeed;
        position_.y += right.y * moveSpeed;
        position_.z += right.z * moveSpeed;
    }
    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        position_.y -= moveSpeed; // Move down
    }
    if (ImGui::IsKeyDown(ImGuiKey_E)) {
        position_.y += moveSpeed; // Move up
    }
}

void CameraManager::ToggleMouseLookMode()
{
    if (currentMode_ == CameraMode::MOUSELOOK) {
        currentMode_ = CameraMode::NAVIGATION;
        mouseLookActive_ = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    } else {
        currentMode_ = CameraMode::MOUSELOOK;
    }
}

void CameraManager::SetMouseLookMode(bool enabled)
{
    if (enabled) {
        currentMode_ = CameraMode::MOUSELOOK;
    } else {
        currentMode_ = CameraMode::NAVIGATION;
        mouseLookActive_ = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
}

void CameraManager::SetYawPitch(float yaw, float pitch)
{
    yaw_ = yaw;
    pitch_ = pitch;
    ApplyPitchConstraints();
    UpdateCameraVectors();
}

void CameraManager::FocusOnPoint(Vector3 point, float distance)
{
    // Move camera to look at the point from current angle
    Vector3 forward = GetForwardVector();
    position_.x = point.x - forward.x * distance;
    position_.y = point.y - forward.y * distance;
    position_.z = point.z - forward.z * distance;
    distance_ = distance;
}

void CameraManager::FrameSelection(Vector3 center, Vector3 size)
{
    // Calculate distance needed to frame the selection
    float maxSize = std::max({size.x, size.y, size.z});
    float framingDistance = maxSize * 2.0f; // Add some padding
    
    FocusOnPoint(center, framingDistance);
}

void CameraManager::ResetToDefault()
{
    position_ = {0.0f, 0.0f, -10.0f};
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    distance_ = DEFAULT_DISTANCE;
    currentMode_ = CameraMode::NAVIGATION;
    mouseLookActive_ = false;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    UpdateCameraVectors();
}

float CameraManager::GetCurrentMoveSpeed() const
{
    if (!useDistanceBasedSpeed_) {
        return baseMoveSpeed_;
    }
    
    // Scale movement speed based on distance from focus point
    // Closer = slower, farther = faster
    float speedMultiplier = std::max(0.1f, distance_ / DEFAULT_DISTANCE);
    return baseMoveSpeed_ * speedMultiplier;
}

Vector3 CameraManager::GetTarget() const
{
    Vector3 forward = GetForwardVector();
    return Vector3{
        position_.x + forward.x,
        position_.y + forward.y,
        position_.z + forward.z
    };
}

Matrix CameraManager::GetViewMatrix() const
{
    Vector3 target = GetTarget();
    return MatrixLookAt(position_, target, up_);
}

Camera3D CameraManager::GetRaylibCamera() const
{
    Camera3D camera = {};
    camera.position = position_;
    camera.target = GetTarget();
    camera.up = up_;
    camera.fovy = 60.0f; // Field of view
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

void CameraManager::StartOrbitNavigation(ImVec2 mousePos)
{
    navigationStartPos_ = mousePos;
    navigationStartYaw_ = yaw_;
    navigationStartPitch_ = pitch_;
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}

void CameraManager::StartPanNavigation(ImVec2 mousePos)
{
    navigationStartPos_ = mousePos;
    navigationStartPosition_ = position_;
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}

void CameraManager::StartStrafeNavigation(ImVec2 mousePos)
{
    navigationStartPos_ = mousePos;
    navigationStartPosition_ = position_;
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}

void CameraManager::UpdateOrbitNavigation(ImVec2 mousePos)
{
    ImVec2 delta = ImVec2(mousePos.x - navigationStartPos_.x, mousePos.y - navigationStartPos_.y);
    
    yaw_ = navigationStartYaw_ + delta.x * orbitSensitivity_;
    pitch_ = navigationStartPitch_ - delta.y * orbitSensitivity_; // Inverted Y
}

void CameraManager::UpdatePanNavigation(ImVec2 mousePos)
{
    ImVec2 delta = ImVec2(mousePos.x - navigationStartPos_.x, mousePos.y - navigationStartPos_.y);
    
    Vector3 right = GetRightVector();
    Vector3 up = Vector3{0.0f, 1.0f, 0.0f}; // World up for panning
    
    float panSpeed = panSensitivity_ * distance_ * 0.01f;
    
    position_.x = navigationStartPosition_.x - right.x * delta.x * panSpeed + up.x * delta.y * panSpeed;
    position_.y = navigationStartPosition_.y - right.y * delta.x * panSpeed + up.y * delta.y * panSpeed;
    position_.z = navigationStartPosition_.z - right.z * delta.x * panSpeed + up.z * delta.y * panSpeed;
}

void CameraManager::UpdateStrafeNavigation(ImVec2 mousePos)
{
    ImVec2 delta = ImVec2(mousePos.x - navigationStartPos_.x, mousePos.y - navigationStartPos_.y);
    
    Vector3 forward = GetForwardVector();
    Vector3 right = GetRightVector();
    
    float strafeSpeed = panSensitivity_ * distance_ * 0.01f;
    
    // Strafe left/right and forward/backward
    position_.x = navigationStartPosition_.x + right.x * delta.x * strafeSpeed + forward.x * delta.y * strafeSpeed;
    position_.y = navigationStartPosition_.y + right.y * delta.x * strafeSpeed + forward.y * delta.y * strafeSpeed;
    position_.z = navigationStartPosition_.z + right.z * delta.x * strafeSpeed + forward.z * delta.y * strafeSpeed;
}

void CameraManager::EndNavigation()
{
    currentNavigation_ = NavigationType::NONE;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
}

void CameraManager::UpdateCameraVectors()
{
    // Calculate forward vector from yaw and pitch
    Vector3 forward;
    forward.x = cosf(yaw_) * cosf(pitch_);
    forward.y = sinf(pitch_);
    forward.z = sinf(yaw_) * cosf(pitch_);

    // Normalize forward vector (though it should already be normalized)
    float length = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (length > 0.0f) {
        forward.x /= length;
        forward.y /= length;
        forward.z /= length;
    }

    // For orbiting camera, position camera at distance from target
    position_.x = target_.x - forward.x * distance_;
    position_.y = target_.y - forward.y * distance_;
    position_.z = target_.z - forward.z * distance_;
}

Vector3 CameraManager::GetForwardVector() const
{
    return Vector3{
        cosf(yaw_) * cosf(pitch_),
        sinf(pitch_),
        sinf(yaw_) * cosf(pitch_)
    };
}

Vector3 CameraManager::GetRightVector() const
{
    Vector3 forward = GetForwardVector();
    Vector3 worldUp = {0.0f, 1.0f, 0.0f};
    
    // Right = forward × worldUp
    Vector3 right = {
        forward.y * worldUp.z - forward.z * worldUp.y,
        forward.z * worldUp.x - forward.x * worldUp.z,
        forward.x * worldUp.y - forward.y * worldUp.x
    };
    
    // Normalize
    float length = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    if (length > 0.0f) {
        right.x /= length;
        right.y /= length;
        right.z /= length;
    }
    
    return right;
}

void CameraManager::ApplyPitchConstraints()
{
    pitch_ = std::max(MIN_PITCH, std::min(MAX_PITCH, pitch_));
}

void CameraManager::ClampPosition()
{
    // Optional: Add world bounds clamping here
    // For now, allow free movement
}