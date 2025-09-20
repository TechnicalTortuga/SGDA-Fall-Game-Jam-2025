#include "PlayerSystem.h"
#include "../Entity.h"
#include "../Components/Velocity.h"
#include "../../core/Engine.h"
#include "../../utils/Logger.h"
#include <cmath>

PlayerSystem::PlayerSystem()
    : playerEntity_(nullptr)
    , renderer_(nullptr)
    , consoleSystem_(nullptr)
    , inputSystem_(nullptr)
    , cameraYaw_(0.0f)
    , cameraPitch_(0.0f)
    , cameraSensitivity_(0.15f)
    , moveSpeed_(50.0f)
    , runMultiplier_(2.0f)
    , crouchMultiplier_(0.5f)
    , jumpForce_(15.0f)
    , isRunning_(false)
    , isCrouching_(false)
    , wantsToJump_(false)
{
}

void PlayerSystem::Initialize() {
    LOG_INFO("PlayerSystem initialized");
    CreatePlayer();
}

void PlayerSystem::Shutdown() {
    DestroyPlayer();
    LOG_INFO("PlayerSystem shutdown");
}

void PlayerSystem::Update(float deltaTime) {
    if (!playerEntity_ || !IsEnabled()) return;

    // Update player input
    UpdatePlayerInput();

    // Update camera
    UpdateCamera(deltaTime);

    // Handle player movement
    HandleMovement(deltaTime);

    // Handle crouching
    HandleCrouching();

    // Handle jumping
    HandleJumping();

    // Update player bounds based on state
    UpdatePlayerBounds();
}

Entity* PlayerSystem::CreatePlayer() {
    if (playerEntity_) {
        LOG_WARNING("Player already exists, destroying old one");
        DestroyPlayer();
    }

    // Create player entity
    playerEntity_ = GetEngine()->CreateEntity();

    // Initialize player components
    InitializePlayerComponents();

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering player entity with systems after components added");
    GetEngine()->UpdateEntityRegistration(playerEntity_);

    LOG_INFO("Player entity created with ID: " + std::to_string(playerEntity_->GetId()));
    return playerEntity_;
}

void PlayerSystem::DestroyPlayer() {
    if (playerEntity_) {
        GetEngine()->DestroyEntity(playerEntity_);
        playerEntity_ = nullptr;
        LOG_INFO("Player entity destroyed");
    }
}

void PlayerSystem::InitializePlayerComponents() {
    if (!playerEntity_) return;

    // Add Position component (spawn high enough to avoid initial ground contact)
    auto position = playerEntity_->AddComponent<Position>(0.0f, 5.0f, 0.0f);

    // Add Velocity component
    auto velocity = playerEntity_->AddComponent<Velocity>(0.0f, 0.0f, 0.0f);

    // Add Player component
    auto player = playerEntity_->AddComponent<Player>();
    player->SetWalkSpeed(moveSpeed_);
    player->SetRunSpeed(moveSpeed_ * runMultiplier_);
    player->SetCrouchSpeed(moveSpeed_ * crouchMultiplier_);
    player->SetJumpForce(jumpForce_);

    // Add Collidable component
    auto collidable = playerEntity_->AddComponent<Collidable>(Vector3{0.8f, 1.8f, 0.8f}); // Player collision box
    collidable->SetCollisionLayer(LAYER_PLAYER);
    collidable->SetCollisionMask(LAYER_WORLD | LAYER_ENEMY | LAYER_PROJECTILE);

    LOG_INFO("Player components initialized");
}

void PlayerSystem::UpdateCamera(float deltaTime) {
    if (!renderer_) return;

    // Update camera rotation based on mouse input from InputSystem
    Vector2 mouseDelta = {0.0f, 0.0f};
    if (inputSystem_) {
        mouseDelta = inputSystem_->GetMouseDelta();
    }

    if (fabsf(mouseDelta.x) > 0.001f || fabsf(mouseDelta.y) > 0.001f) {
        ApplyCameraRotation(mouseDelta, deltaTime);
    }

    // Update renderer camera to follow player
    Vector3 camPos = GetCameraPosition();
    LOG_DEBUG("PlayerSystem::UpdateCamera - player pos: (" + std::to_string(camPos.x) + "," + std::to_string(camPos.y) + "," + std::to_string(camPos.z) + ")");
    renderer_->UpdateCameraToFollowPlayer(camPos.x, camPos.y, camPos.z);
}

void PlayerSystem::UpdatePlayerInput() {
    if (!playerEntity_) return;

    auto player = playerEntity_->GetComponent<Player>();
    if (!player) return;

    // Update movement state
    isRunning_ = IsKeyDown(KEY_LEFT_SHIFT);
    wantsToJump_ = IsKeyPressed(KEY_SPACE);

    // Update player component state
    player->SetRunning(isRunning_);
    player->SetJumping(wantsToJump_);
}

void PlayerSystem::HandleMovement(float deltaTime) {
    if (!playerEntity_) return;

    auto position = playerEntity_->GetComponent<Position>();
    auto velocity = playerEntity_->GetComponent<Velocity>();
    auto player = playerEntity_->GetComponent<Player>();

    if (!position || !velocity || !player) return;

    // Calculate movement vector based on input and camera orientation
    Vector3 movement = CalculateMovementVector();

    // Apply movement speed modifiers
    float currentSpeed = player->GetWalkSpeed();
    if (isRunning_) {
        currentSpeed = player->GetRunSpeed();
    }
    if (isCrouching_) {
        currentSpeed = player->GetCrouchSpeed();
    }

    // Acceleration-based horizontal control for smoother feel
    Vector3 v = velocity->GetVelocity();
    Vector2 current = { v.x, v.z };
    Vector2 inputDir = { movement.x, movement.z };
    float len = sqrtf(inputDir.x*inputDir.x + inputDir.y*inputDir.y);
    if (len > 0.0f) { inputDir.x /= len; inputDir.y /= len; }

    Vector2 target = { inputDir.x * currentSpeed, inputDir.y * currentSpeed };

    bool onGround = (player->GetState() == PlayerState::ON_GROUND || player->GetState() == PlayerState::CROUCHING);
    float accel = onGround ? 60.0f : 20.0f;    // units/s^2
    float decel = onGround ? 80.0f : 10.0f;    // units/s^2

    if (len > 0.0f) {
        // Accelerate toward target
        Vector2 delta = { target.x - current.x, target.y - current.y };
        float dlen = sqrtf(delta.x*delta.x + delta.y*delta.y);
        if (dlen > 0.0f) {
            float step = accel * deltaTime;
            if (step > dlen) step = dlen;
            current.x += (delta.x / dlen) * step;
            current.y += (delta.y / dlen) * step;
        }
    } else if (onGround) {
        // Decelerate toward zero when no input and grounded
        float speed = sqrtf(current.x*current.x + current.y*current.y);
        if (speed > 0.0f) {
            float step = decel * deltaTime;
            if (step > speed) step = speed;
            current.x -= (current.x / speed) * step;
            current.y -= (current.y / speed) * step;
        }
    }

    // === SLOPE DETECTION AND ADJUSTMENT ===
    // Check if player is on a slope and adjust movement accordingly
    if (onGround && (fabsf(current.x) > 0.01f || fabsf(current.y) > 0.01f)) {
        Vector3 slopeNormal;
        if (IsOnSlope(slopeNormal)) {
            // Adjust movement for slope
            Vector2 adjustedMovement = AdjustMovementForSlope({current.x, current.y}, slopeNormal);
            current.x = adjustedMovement.x;
            current.y = adjustedMovement.y;

            LOG_INFO("PLAYER SLOPE: Applied slope adjustment - input: (" +
                     std::to_string(velocity->GetX()) + "," + std::to_string(velocity->GetZ()) +
                     ") -> output: (" + std::to_string(current.x) + "," + std::to_string(current.y) + ")");
        }
    }

    // Apply back to velocity (preserve Y; PhysicsSystem handles vertical)
    velocity->SetX(current.x);
    velocity->SetZ(current.y);

    // NOTE: PhysicsSystem will apply velocity to position - don't do it here to avoid conflicts

    // Update collidable bounds
    auto collidable = playerEntity_->GetComponent<Collidable>();
    if (collidable) {
        collidable->UpdateBoundsFromPosition(position->GetPosition());
    }
}

void PlayerSystem::HandleCrouching() {
    if (!playerEntity_) return;

    auto player = playerEntity_->GetComponent<Player>();
    if (!player) return;

    bool crouchPressed = IsKeyDown(KEY_LEFT_CONTROL);
    PlayerState currentState = player->GetState();

    if (crouchPressed && currentState == PlayerState::ON_GROUND) {
        player->SetState(PlayerState::CROUCHING);
        isCrouching_ = true;
    } else if (!crouchPressed && currentState == PlayerState::CROUCHING) {
        player->SetState(PlayerState::ON_GROUND);
        isCrouching_ = false;
    }
}

void PlayerSystem::HandleJumping() {
    if (!playerEntity_ || !wantsToJump_) return;

    auto player = playerEntity_->GetComponent<Player>();
    auto velocity = playerEntity_->GetComponent<Velocity>();

    if (!player || !velocity) return;

    if (player->IsOnGround()) {
        velocity->SetY(player->GetJumpForce());
        player->SetState(PlayerState::IN_AIR);
        wantsToJump_ = false;
    }
}

void PlayerSystem::UpdatePlayerBounds() {
    if (!playerEntity_) return;

    auto player = playerEntity_->GetComponent<Player>();
    auto collidable = playerEntity_->GetComponent<Collidable>();

    if (!player || !collidable) return;

    // Update collision box size based on crouching state
    Vector3 newSize = {0.8f, player->GetStandingHeight(), 0.8f};
    if (isCrouching_) {
        newSize.y = player->GetCrouchingHeight();
    }

    collidable->SetSize(newSize);
}

Vector3 PlayerSystem::CalculateMovementVector() const {
    Vector3 movement = {0.0f, 0.0f, 0.0f};

    // Get camera forward and right vectors
    Vector3 forward = GetCameraForward();
    Vector3 right = GetCameraRight();

    // WASD movement relative to camera
    if (IsKeyDown(KEY_W)) {
        movement.x += forward.x;
        movement.z += forward.z;
    }
    if (IsKeyDown(KEY_S)) {
        movement.x -= forward.x;
        movement.z -= forward.z;
    }
    if (IsKeyDown(KEY_A)) {
        movement.x -= right.x;
        movement.z -= right.z;
    }
    if (IsKeyDown(KEY_D)) {
        movement.x += right.x;
        movement.z += right.z;
    }

    // Normalize horizontal movement to prevent faster diagonal movement
    float horizontalLength = sqrtf(movement.x * movement.x + movement.z * movement.z);
    if (horizontalLength > 0.0f) {
        movement.x /= horizontalLength;
        movement.z /= horizontalLength;
    }

    return movement;
}

void PlayerSystem::ApplyCameraRotation(Vector2 mouseDelta, float deltaTime) {
    // Apply mouse sensitivity and delta time scaling
    float scaledDeltaX = mouseDelta.x * cameraSensitivity_ * deltaTime * 60.0f;
    float scaledDeltaY = mouseDelta.y * cameraSensitivity_ * deltaTime * 60.0f;

    // Update camera angles
    cameraYaw_ += scaledDeltaX;
    cameraPitch_ -= scaledDeltaY; // Inverted for natural up/down

    // Normalize yaw to [0, 2π]
    while (cameraYaw_ > 2.0f * PI) cameraYaw_ -= 2.0f * PI;
    while (cameraYaw_ < 0.0f) cameraYaw_ += 2.0f * PI;

    // Clamp pitch to prevent camera flipping
    const float maxPitch = PI * 0.45f;
    if (cameraPitch_ > maxPitch) cameraPitch_ = maxPitch;
    if (cameraPitch_ < -maxPitch) cameraPitch_ = -maxPitch;

    // Update renderer camera rotation
    if (renderer_) {
        renderer_->UpdateCameraRotation(scaledDeltaX, scaledDeltaY, deltaTime);
    }
}

Vector3 PlayerSystem::GetCameraPosition() const {
    if (!playerEntity_) return {0.0f, 0.0f, 0.0f};

    auto position = playerEntity_->GetComponent<Position>();
    if (!position) return {0.0f, 0.0f, 0.0f};

    // Camera is at player position with eye height
    return {
        position->GetX(),
        position->GetY() + 1.5f, // Eye height
        position->GetZ()
    };
}

Vector3 PlayerSystem::GetCameraForward() const {
    // Calculate forward vector from camera angles
    return {
        sinf(cameraYaw_) * cosf(cameraPitch_),
        sinf(cameraPitch_),
        -cosf(cameraYaw_) * cosf(cameraPitch_)
    };
}

Vector3 PlayerSystem::GetCameraRight() const {
    // Calculate right vector (perpendicular to forward)
    return {
        cosf(cameraYaw_),
        0.0f,
        sinf(cameraYaw_)
    };
}

void PlayerSystem::SetPlayerState(PlayerState state) {
    if (!playerEntity_) return;

    auto player = playerEntity_->GetComponent<Player>();
    if (player) {
        player->SetState(state);
    }
}

PlayerState PlayerSystem::GetPlayerState() const {
    if (!playerEntity_) return PlayerState::ON_GROUND;

    auto player = playerEntity_->GetComponent<Player>();
    return player ? player->GetState() : PlayerState::ON_GROUND;
}

bool PlayerSystem::IsPlayerOnGround() const {
    return GetPlayerState() == PlayerState::ON_GROUND;
}

bool PlayerSystem::IsPlayerCrouching() const {
    return GetPlayerState() == PlayerState::CROUCHING;
}

bool PlayerSystem::CheckGroundCollision() const {
    float groundHeight;
    return RaycastGround(groundHeight);
}

bool PlayerSystem::RaycastGround(float& groundHeight) const {
    // Simple ground check - in a full implementation, this would raycast downward
    groundHeight = 0.0f;
    return true;
}

bool PlayerSystem::IsPositionValid(const Vector3& position) const {
    // Check if the position is valid (not inside walls, etc.)
    // This would use the collision system in a full implementation
    return true;
}

// === SLOPE DETECTION AND MOVEMENT ADJUSTMENT ===

bool PlayerSystem::IsOnSlope(Vector3& outNormal) const {
    if (!playerEntity_) return false;

    auto position = playerEntity_->GetComponent<Position>();
    if (!position) return false;

    Vector3 playerPos = position->GetPosition();

    // Cast a ray downward to detect the surface normal
    Vector3 rayStart = playerPos;
    rayStart.y += 0.1f; // Start slightly above player position

    Vector3 rayDirection = {0.0f, -1.0f, 0.0f}; // Straight down
    float rayLength = 1.0f;

    // Use the collision system to cast the ray
    auto collisionSystem = GetEngine()->GetSystem<CollisionSystem>();
    if (!collisionSystem) {
        return false;
    }

    const BSPTree* bspTree = collisionSystem->GetBSPTree();
    if (!bspTree) {
        return false;
    }

    float hitDistance = bspTree->CastRayWithNormal(rayStart, rayDirection, rayLength, outNormal);

    if (hitDistance < rayLength) {
        // Check if this is a slope surface (not vertical, not flat)
        float normalY = fabsf(outNormal.y);
        if (normalY > 0.1f && normalY < 0.95f) { // Between ~6° and ~71° from horizontal
            LOG_INFO("PLAYER SLOPE: *** CONFIRMED SLOPE SURFACE *** normal=(" +
                     std::to_string(outNormal.x) + "," + std::to_string(outNormal.y) + "," +
                     std::to_string(outNormal.z) + ")");
            return true;
        }
    }

    return false;
}

Vector2 PlayerSystem::AdjustMovementForSlope(Vector2 inputMovement, const Vector3& slopeNormal) const {
    // Project the input movement onto the slope surface
    Vector3 movement3D = {inputMovement.x, 0.0f, inputMovement.y};

    // Project movement onto plane defined by slope normal
    float dot = Vector3DotProduct(movement3D, slopeNormal);
    Vector3 projected = Vector3Subtract(movement3D, Vector3Scale(slopeNormal, dot));

    // Preserve the original movement magnitude
    float originalLength = Vector2Length(inputMovement);
    if (Vector3Length(projected) > 0.001f) {
        Vector3 normalized = Vector3Normalize(projected);
        projected = Vector3Scale(normalized, originalLength);
    }

    return {projected.x, projected.z};
}
