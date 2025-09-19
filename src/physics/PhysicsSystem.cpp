#include "PhysicsSystem.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Player.h"
#include "../utils/Logger.h"
#include <algorithm>

PhysicsSystem::PhysicsSystem()
    : gravity_(GRAVITY)
    , terminalVelocity_(TERMINAL_VELOCITY)
    , groundFriction_(GROUND_FRICTION)
    , airResistance_(AIR_RESISTANCE)
    , bspTree_(nullptr)
    , collisionSystem_(nullptr)
{
}

void PhysicsSystem::Initialize() {
    // Set up archetype for entities with Position and Velocity components
}

void PhysicsSystem::Shutdown() {
    // Clean up physics state
}

void PhysicsSystem::Update(float deltaTime) {
    if (!IsEnabled()) return;

    // Update physics for all entities
    for (Entity* entity : GetEntities()) {
        UpdateEntityPhysics(entity, deltaTime);
    }
}

void PhysicsSystem::UpdateEntityPhysics(Entity* entity, float deltaTime) {
    auto* position = entity->GetComponent<Position>();
    auto* velocity = entity->GetComponent<Velocity>();
    auto* player = entity->GetComponent<Player>();

    if (!position || !velocity) return;

    // Special handling for player entities
    if (player) {
        LOG_INFO("Updating player physics");
        UpdatePlayerPhysics(entity, deltaTime);
        return;
    }

    // Apply basic physics forces
    ApplyGravity(*velocity, deltaTime);
    ApplyAirResistance(*velocity, deltaTime);

    // Calculate intended movement
    Vector3 currentVel = velocity->GetVelocity();
    LOG_INFO("Physics input velocity=(" + std::to_string(currentVel.x) + ", " +
             std::to_string(currentVel.y) + ", " + std::to_string(currentVel.z) + ")");
    Vector3 movement = Vector3Scale(currentVel, deltaTime);
    LOG_INFO("Physics movement=(" + std::to_string(movement.x) + ", " +
             std::to_string(movement.y) + ", " + std::to_string(movement.z) + ")");

    // Resolve movement with collision detection
    ResolveMovement(entity, movement, deltaTime);

    // Apply friction based on ground contact
    bool onGround = IsOnGround(position->GetPosition(), {1.0f, 1.0f, 1.0f});
    ApplyFriction(*velocity, deltaTime, onGround);
}

void PhysicsSystem::UpdatePlayerPhysics(Entity* playerEntity, float deltaTime) {
    auto* position = playerEntity->GetComponent<Position>();
    auto* velocity = playerEntity->GetComponent<Velocity>();
    auto* player = playerEntity->GetComponent<Player>();
    auto* collidable = playerEntity->GetComponent<Collidable>();

    if (!position || !velocity || !player) return;

    // Debug: Player physics - Position and Velocity tracking

    PlayerState currentState = player->GetState();

    // Update player state based on current conditions
    UpdatePlayerState(playerEntity);

    // Apply gravity only when in air
    if (currentState == PlayerState::IN_AIR) {
        ApplyGravity(*velocity, deltaTime);
    } else {
        // Reset vertical velocity when on ground (Y-axis in 3D, not Z)
        // Note: In 3D coordinate system, Y is typically up/down, not Z
        velocity->SetY(0.0f);
    }

    // Handle crouching mechanics
    HandlePlayerCrouching(playerEntity);

    // Handle jumping
    HandlePlayerJumping(playerEntity);

    // Apply air resistance
    ApplyAirResistance(*velocity, deltaTime);

    // Calculate intended movement
    Vector3 movement = Vector3Scale(velocity->GetVelocity(), deltaTime);

    // Check for noclip
    if (!player->HasNoClip()) {
        // Resolve movement with collision detection
        ResolveMovement(playerEntity, movement, deltaTime);
    } else {
        // No-clip mode: just apply movement directly
        position->Move(movement);
        if (collidable) {
            collidable->UpdateBoundsFromPosition(position->GetPosition());
        }
    }

    // Apply friction based on ground contact
    bool onGround = (currentState == PlayerState::ON_GROUND || currentState == PlayerState::CROUCHING);
    ApplyFriction(*velocity, deltaTime, onGround);

    // Ground snap: if very close to ground while moving downward, snap to surface
    if (bspTree_ && position && collidable) {
        Vector3 size = collidable->GetBounds().GetSize();
        float halfHeight = size.y * 0.5f;
        Vector3 bottom = { position->GetX(), position->GetY() - halfHeight, position->GetZ() };
        Vector3 down = {0.0f, -1.0f, 0.0f};
        const float maxSnap = 0.1f;   // 10cm snap range
        float d = bspTree_->CastRay(bottom, down, maxSnap);
        if (d < maxSnap) {
            // Move player up by the penetration distance plus small epsilon
            float epsilon = 0.01f;
            Vector3 newPos = position->GetPosition();
            newPos.y = newPos.y - d + epsilon;
            position->SetPosition(newPos);
            collidable->UpdateBoundsFromPosition(newPos);
            if (velocity->GetY() < 0.0f) velocity->SetY(0.0f);
        }
    }
}

void PhysicsSystem::ApplyGravity(Velocity& velocity, float deltaTime) {
    // Apply gravity to Y-axis (up/down), not Z-axis
    float currentY = velocity.GetY();
    float newY = currentY + (gravity_ * deltaTime);

    // Clamp to terminal velocity
    velocity.SetY(std::max(newY, terminalVelocity_));
}

void PhysicsSystem::ApplyFriction(Velocity& velocity, float deltaTime, bool onGround) {
    Vector3 currentVel = velocity.GetVelocity();

    // Debug: Friction application
    LOG_INFO("Friction before - vel: (" + std::to_string(currentVel.x) + ", " +
             std::to_string(currentVel.y) + ", " + std::to_string(currentVel.z) +
             ") onGround: " + (onGround ? "true" : "false"));

    if (onGround) {
        // Ground friction (softer for smoother feel)
        const float frictionCoeffPerFrame = 0.98f; // ~2% speed loss per 60fps frame
        const float stopThreshold = 0.02f;

        // Apply exponential decay based on deltaTime (normalized to 60 FPS)
        float decayFactor = powf(frictionCoeffPerFrame, deltaTime * 60.0f);
        currentVel.x *= decayFactor;
        currentVel.z *= decayFactor; // Horizontal forward/back

        // Stop very small movements immediately
        if (fabsf(currentVel.x) < stopThreshold) currentVel.x = 0.0f;
        if (fabsf(currentVel.z) < stopThreshold) currentVel.z = 0.0f;

    } else {
        // Air resistance - very gentle
        const float airFrictionPerFrame = 0.995f; // 0.5% loss per frame
        float decayFactor = powf(airFrictionPerFrame, deltaTime * 60.0f);
        currentVel.x *= decayFactor;
        currentVel.z *= decayFactor;

        // Don't stop air movement as aggressively
        if (fabsf(currentVel.x) < 0.001f) currentVel.x = 0.0f;
        if (fabsf(currentVel.z) < 0.001f) currentVel.z = 0.0f;
    }

    // Debug: After friction application
    LOG_INFO("Friction after - vel: (" + std::to_string(currentVel.x) + ", " +
             std::to_string(currentVel.y) + ", " + std::to_string(currentVel.z) + ")");

    velocity.SetVelocity(currentVel);
}

void PhysicsSystem::ApplyAirResistance(Velocity& velocity, float deltaTime) {
    // Apply air resistance with time-based decay for smoother physics
    Vector3 currentVel = velocity.GetVelocity();

    // Use time-based exponential decay for air resistance
    float decayFactor = powf(airResistance_, deltaTime * 60.0f); // 60fps normalized
    currentVel.x *= decayFactor;
    currentVel.z *= decayFactor; // Apply to horizontal movement (X, Z)
    currentVel.y *= decayFactor; // Apply to vertical movement (Y)

    velocity.SetVelocity(currentVel);
}

void PhysicsSystem::ResolveMovement(Entity* entity, const Vector3& intendedMovement, float deltaTime) {
    auto* position = entity->GetComponent<Position>();
    auto* velocity = entity->GetComponent<Velocity>();
    auto* collidable = entity->GetComponent<Collidable>();

    if (!position || !velocity || !collidable) return;

    Vector3 currentPos = position->GetPosition();
    Vector3 newPos = Vector3Add(currentPos, intendedMovement);

    // Check collision at new position
    if (collisionSystem_ && collisionSystem_->IsEnabled()) {
        // Cast to CollisionSystem to access the method
        auto collisionSys = dynamic_cast<class CollisionSystem*>(collisionSystem_);
        if (collisionSys && collisionSys->CheckCollisionWithWorld(*collidable, newPos)) {
            // Collision detected, resolve it
            HandleWallCollision(entity, intendedMovement);
            return;
        }
    }

    // No collision, apply full movement
    position->SetPosition(newPos);
    collidable->UpdateBoundsFromPosition(newPos);
}

void PhysicsSystem::HandleWallCollision(Entity* entity, const Vector3& movement) {
    auto* position = entity->GetComponent<Position>();
    auto* velocity = entity->GetComponent<Velocity>();

    if (!position || !velocity) return;

    // Sliding collision response (horizontal first)
    Vector3 originalPos = position->GetPosition();

    // Try move only along X
    Vector3 slideX = {movement.x, 0.0f, 0.0f};
    Vector3 testPosX = Vector3Add(originalPos, slideX);
    bool movedX = false;
    if (!CheckCollisionAtPosition(entity, testPosX)) {
        position->Move(slideX);
        movedX = true;
    } else {
        // Stop X if blocked
        velocity->SetX(0.0f);
    }

    // Try move only along Z
    Vector3 currentPos = position->GetPosition();
    Vector3 slideZ = {0.0f, 0.0f, movement.z};
    Vector3 testPosZ = Vector3Add(currentPos, slideZ);
    if (!CheckCollisionAtPosition(entity, testPosZ)) {
        position->Move(slideZ);
    } else {
        velocity->SetZ(0.0f);
    }

    // Handle vertical move separately
    if (movement.y != 0.0f) {
        Vector3 slideY = {0.0f, movement.y, 0.0f};
        Vector3 testPosY = Vector3Add(position->GetPosition(), slideY);
        if (!CheckCollisionAtPosition(entity, testPosY)) {
            position->Move(slideY);
        } else {
            velocity->SetY(0.0f);
        }
    }

    // Update collidable bounds
    auto* collidable = entity->GetComponent<Collidable>();
    if (collidable) {
        collidable->UpdateBoundsFromPosition(position->GetPosition());
    }
}

bool PhysicsSystem::CheckCollisionAtPosition(Entity* entity, const Vector3& position) const {
    auto* collidable = entity->GetComponent<Collidable>();
    if (!collidable || !collisionSystem_) return false;

    // Cast to CollisionSystem to access the method
    auto collisionSys = dynamic_cast<class CollisionSystem*>(collisionSystem_);
    return collisionSys && collisionSys->CheckCollisionWithWorld(*collidable, position);
}

void PhysicsSystem::UpdatePlayerState(Entity* playerEntity) {
    auto* player = playerEntity->GetComponent<Player>();
    auto* position = playerEntity->GetComponent<Position>();
    auto* collidable = playerEntity->GetComponent<Collidable>();

    if (!player || !position || !collidable) return;

    Vector3 playerPos = position->GetPosition();
    Vector3 playerSize = collidable->GetBounds().GetSize();

    // Check if player is on ground
    bool onGround = IsOnGround(playerPos, playerSize);

    PlayerState currentState = player->GetState();

    if (onGround && currentState == PlayerState::IN_AIR) {
        // Just landed
        player->SetState(PlayerState::ON_GROUND);
        HandlePlayerLanding(playerEntity);
    } else if (!onGround && currentState != PlayerState::IN_AIR) {
        // Just started falling/jumping
        player->SetState(PlayerState::IN_AIR);
    }
}

void PhysicsSystem::HandlePlayerCrouching(Entity* playerEntity) {
    auto* player = playerEntity->GetComponent<Player>();
    auto* collidable = playerEntity->GetComponent<Collidable>();

    if (!player || !collidable) return;

    PlayerState state = player->GetState();

    if (state == PlayerState::CROUCHING) {
        // Set crouching height
        Vector3 currentSize = collidable->GetBounds().GetSize();
        currentSize.y = player->GetCrouchingHeight();
        collidable->SetSize(currentSize);
    } else if (state == PlayerState::ON_GROUND) {
        // Set standing height
        Vector3 currentSize = collidable->GetBounds().GetSize();
        currentSize.y = player->GetStandingHeight();
        collidable->SetSize(currentSize);
    }
}

void PhysicsSystem::HandlePlayerJumping(Entity* playerEntity) {
    auto* player = playerEntity->GetComponent<Player>();
    auto* velocity = playerEntity->GetComponent<Velocity>();

    if (!player || !velocity) return;

    if (player->IsJumping() && player->IsOnGround()) {
        // Apply jump force on Y axis (up)
        velocity->SetY(player->GetJumpForce());
        player->SetJumping(false);
        player->SetState(PlayerState::IN_AIR);
    }
}

void PhysicsSystem::HandlePlayerLanding(Entity* playerEntity) {
    auto* player = playerEntity->GetComponent<Player>();
    auto* velocity = playerEntity->GetComponent<Velocity>();

    if (!player || !velocity) return;

    // Dampen landing impact on Y axis
    Vector3 currentVel = velocity->GetVelocity();
    currentVel.y *= -BOUNCE_DAMPING; // Small bounce

    if (fabsf(currentVel.y) < 1.0f) {
        currentVel.y = 0.0f; // Stop bouncing
    }

    velocity->SetVelocity(currentVel);
}

Vector3 PhysicsSystem::ClampVelocity(const Vector3& velocity, float maxSpeed) const {
    float speed = Vector3Length(velocity);
    if (speed > maxSpeed) {
        return Vector3Scale(Vector3Normalize(velocity), maxSpeed);
    }
    return velocity;
}

bool PhysicsSystem::IsOnGround(const Vector3& position, const Vector3& size) const {
    if (!bspTree_) {
        // Fallback: If no BSP tree, assume player is on ground at Y=0
        // This prevents infinite falling in test scenarios
        float groundThreshold = size.y/2.0f + 0.1f;
        bool onGround = position.y <= groundThreshold;
        LOG_INFO("Ground check: PosY=" + std::to_string(position.y) +
                 " SizeY=" + std::to_string(size.y) + " Threshold=" + std::to_string(groundThreshold) +
                 " onGround=" + (onGround ? "true" : "false"));
        return onGround;
    }

    // Check a small distance below the player
    Vector3 groundCheck = {position.x, position.y - size.y/2.0f - 0.1f, position.z};

    // Cast a small ray downward to check for ground
    Vector3 down = {0.0f, -1.0f, 0.0f};
    float distance = bspTree_->CastRay(groundCheck, down, 0.2f);

    return distance < 0.2f;
}

Vector3 PhysicsSystem::GetGroundNormal(const Vector3& /*position*/) const {
    // This would calculate the ground normal for slope detection
    // For now, return up vector
    return {0.0f, 1.0f, 0.0f};
}
