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

// Enhanced collision constants
const float CONTACT_TOLERANCE = 0.001f;  // Minimum distance for contact
const float VELOCITY_EPSILON = 0.01f;    // Threshold for stopping micro-movements
const float PENETRATION_SLOP = 0.005f;   // Allowed penetration before hard correction
const float STEP_HEIGHT = 0.6f;          // Maximum step height for stair climbing (increased for larger stairs)
const float SLOPE_THRESHOLD = 0.7f;      // Surface normal Y component threshold for slopes (cos(45°) ≈ 0.707)
const float MAX_SLOPE_ANGLE = 45.0f;     // Maximum walkable slope angle in degrees

/**
 * Physics system that handles movement, gravity, collision response, and player state management
 */
// Enhanced collision handling structures
struct CollisionPlane {
    Vector3 normal;
    float distance;
    Vector3 contactPoint;
    bool isContact;  // True for resting contact, false for collision
};

struct StabilizedMovement {
    Vector3 position;
    Vector3 velocity;
    bool onGround;
};

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
    void HandleCollision(Entity* entity, const Vector3& movement, const Vector3& surfaceNormal);
    void HandleCollisionResponse(Entity* entity, const Vector3& movement, const Vector3& surfaceNormal);
    Vector3 ResolveCollisionsSequentially(Entity* entity, const Vector3& startPos, const Vector3& intendedMovement);
    std::vector<CollisionEvent> GetAllCollisions(Entity* entity, const Vector3& position) const;
    void HandleMultipleCollisions(Entity* entity, const Vector3& intendedMovement, const std::vector<CollisionEvent>& collisions);
    bool WouldCollideWithAny(Entity* entity, const Vector3& position, const std::vector<CollisionEvent>& collisions, size_t excludeIndex) const;
    CollisionEvent GetDetailedCollision(Entity* entity, const Vector3& position, const Vector3& movement) const;
    void TryHorizontalMovement(Entity* entity, const Vector3& intendedMovement);

    // Player-specific physics
    void UpdatePlayerState(Entity* playerEntity);
    void HandlePlayerCrouching(Entity* playerEntity);
    void HandlePlayerJumping(Entity* playerEntity);
    void HandlePlayerLanding(Entity* playerEntity);

    // Enhanced collision handling methods
    Vector3 ResolveCornerCollision(const Vector3& velocity, const std::vector<CollisionPlane>& planes);
    Vector3 SlideVelocity(const Vector3& velocity, const Vector3& normal);
    Vector3 ResolveConstrainedMovement(Entity* entity, const Vector3& currentPos, const Vector3& intendedMovement);
    bool TryStepUp(Entity* entity, const Vector3& intendedMovement);
    std::vector<CollisionPlane> GatherCollisionPlanes(Entity* entity, const Vector3& position);
    
    // Slope detection and handling
    bool IsWalkableSlope(const Vector3& normal) const;
    bool IsSlopeAtPosition(const Vector3& position, Vector3& outNormal) const;
    Vector3 ProjectMovementOntoSlope(const Vector3& movement, const Vector3& slopeNormal) const;
    Vector3 ApplySlopePhysics(Entity* entity, const Vector3& intendedMovement, const Vector3& slopeNormal, float deltaTime);
    void ApplyUnstuckCorrection(Entity* entity, float deltaTime);
    
    // Utility methods
    Vector3 ClampVelocity(const Vector3& velocity, float maxSpeed) const;
    bool IsOnGround(const Vector3& position, const Vector3& size) const;
    Vector3 GetGroundNormal(const Vector3& position) const;
    float GetSurfaceHeightAtPosition(const Vector3& position) const;
    bool CheckCollisionAtPosition(Entity* entity, const Vector3& position) const;
};
