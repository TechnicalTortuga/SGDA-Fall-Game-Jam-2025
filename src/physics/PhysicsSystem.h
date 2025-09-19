#pragma once

#include "../ecs/System.h"
#include "../ecs/Components/Position.h"
#include "../ecs/Components/Velocity.h"
#include "../ecs/Components/Collidable.h"
#include "../ecs/Components/Player.h"
#include "../world/BSPTree.h"
#include "../ecs/Systems/CollisionSystem.h"

// Physics constants
const float GRAVITY = -30.0f;          // Gravity acceleration (units/s²)
const float TERMINAL_VELOCITY = -50.0f; // Maximum falling speed
const float GROUND_FRICTION = 0.8f;     // Ground friction coefficient
const float AIR_RESISTANCE = 0.98f;     // Air resistance coefficient
const float BOUNCE_DAMPING = 0.3f;      // Bounce energy loss

/**
 * Physics system that handles movement, gravity, collision response, and player state management
 */
class PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Initialize() override;
    void Shutdown() override;

    // Physics configuration
    void SetGravity(float gravity) { gravity_ = gravity; }
    void SetTerminalVelocity(float velocity) { terminalVelocity_ = velocity; }
    void SetGroundFriction(float friction) { groundFriction_ = friction; }
    void SetAirResistance(float resistance) { airResistance_ = resistance; }

    // BSP integration
    void SetBSPTree(BSPTree* bspTree) { bspTree_ = bspTree; }
    void SetCollisionSystem(System* collisionSystem) { collisionSystem_ = collisionSystem; }

private:
    float gravity_;
    float terminalVelocity_;
    float groundFriction_;
    float airResistance_;
    BSPTree* bspTree_;
    System* collisionSystem_;

    // Physics update methods
    void UpdateEntityPhysics(Entity* entity, float deltaTime);
    void UpdatePlayerPhysics(Entity* playerEntity, float deltaTime);

    // Force and movement calculations
    void ApplyGravity(Velocity& velocity, float deltaTime);
    void ApplyFriction(Velocity& velocity, float deltaTime, bool onGround);
    void ApplyAirResistance(Velocity& velocity, float deltaTime);

    // Collision and movement resolution
    void ResolveMovement(Entity* entity, const Vector3& intendedMovement, float deltaTime);
    bool CheckGroundCollision(const Vector3& position, const Vector3& size) const;
    void HandleWallCollision(Entity* entity, const Vector3& movement);

    // Player-specific physics
    void UpdatePlayerState(Entity* playerEntity);
    void HandlePlayerCrouching(Entity* playerEntity);
    void HandlePlayerJumping(Entity* playerEntity);
    void HandlePlayerLanding(Entity* playerEntity);

    // Utility methods
    Vector3 ClampVelocity(const Vector3& velocity, float maxSpeed) const;
    bool IsOnGround(const Vector3& position, const Vector3& size) const;
    Vector3 GetGroundNormal(const Vector3& position) const;
    bool CheckCollisionAtPosition(Entity* entity, const Vector3& position) const;
};
