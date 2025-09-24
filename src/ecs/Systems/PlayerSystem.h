#pragma once

#include "../System.h"
#include "../../core/Engine.h"
#include "../Components/Position.h"
#include "../Components/Player.h"
#include "../Components/Collidable.h"
#include "../../rendering/Renderer.h"
#include "../../ui/ConsoleSystem.h"
#include "InputSystem.h"
#include "LODSystem.h"
#include "CollisionSystem.h"

class PlayerSystem : public System {
public:
    PlayerSystem();
    ~PlayerSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Initialize() override;
    void Shutdown() override;

    // Player management
    Entity* CreatePlayer();
    Entity* GetPlayer() const { return playerEntity_; }
    void DestroyPlayer();

    // Camera management
    void UpdateCamera(float deltaTime);
    Vector3 GetCameraPosition() const;
    Vector3 GetCameraForward() const;
    Vector3 GetCameraRight() const;

    // Player state management
    void SetPlayerState(PlayerState state);
    PlayerState GetPlayerState() const;
    bool IsPlayerOnGround() const;
    bool IsPlayerCrouching() const;

    // Movement controls
    void HandleMovement(float deltaTime);
    void HandleCrouching();
    void HandleJumping();

    // Collision and physics
    void UpdatePlayerBounds();
    bool CheckGroundCollision() const;

    // External system integration
    void SetRenderer(Renderer* renderer) { renderer_ = renderer; }
    void SetConsoleSystem(ConsoleSystem* console) { consoleSystem_ = console; }
    void SetInputSystem(InputSystem* inputSystem) { inputSystem_ = inputSystem; }
    void SetCollisionSystem(CollisionSystem* collisionSystem) { collisionSystem_ = collisionSystem; }

private:
    Entity* playerEntity_;
    Renderer* renderer_;
    ConsoleSystem* consoleSystem_;
    InputSystem* inputSystem_;
    CollisionSystem* collisionSystem_;

    // Camera control variables
    float cameraSensitivity_;

    // Player movement variables
    float moveSpeed_;
    float runMultiplier_;
    float crouchMultiplier_;
    float jumpForce_;

    // Player state tracking
    bool isRunning_;
    bool isCrouching_;
    bool wantsToJump_;

    // Internal helper methods
    void InitializePlayerComponents();
    void UpdatePlayerInput();
    void ApplyMovement(float deltaTime);
    void ApplyCameraRotation(Vector2 mouseDelta, float deltaTime);
    Vector3 CalculateMovementVector() const;
    void ClampCameraAngles();

    // Collision detection
    bool RaycastGround(float& groundHeight) const;
    bool IsPositionValid(const Vector3& position) const;

    // Slope handling
    bool IsOnSlope(Vector3& outNormal) const;
    Vector2 AdjustMovementForSlope(Vector2 inputMovement, const Vector3& slopeNormal) const;
};
