#include "PhysicsSystem.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Player.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cfloat>

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

    // Get updated state after UpdatePlayerState
    currentState = player->GetState();

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

    // Apply friction based on ground contact (use actual ground check, not just state)
    bool onGround = IsOnGround(position->GetPosition(), collidable->GetBounds().GetSize());
    ApplyFriction(*velocity, deltaTime, onGround);
    
    // Check for geometry stuck and apply gentle unstuck correction
    ApplyUnstuckCorrection(playerEntity, deltaTime);

    // Ground snap: if very close to ground while moving downward, snap to surface
    const BSPTree* bspTree = nullptr;
    if (collisionSystem_) {
        bspTree = dynamic_cast<CollisionSystem*>(collisionSystem_)->GetBSPTree();
    }
    if (bspTree && position && collidable) {
        Vector3 size = collidable->GetBounds().GetSize();
        float halfHeight = size.y * 0.5f;
        Vector3 bottom = { position->GetX(), position->GetY() - halfHeight, position->GetZ() };
        Vector3 down = {0.0f, -1.0f, 0.0f};
        const float maxSnap = 0.1f;   // 10cm snap range
        float d = bspTree->CastRay(bottom, down, maxSnap);
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
    Vector3 targetPos = Vector3Add(currentPos, intendedMovement);

    // Only log when near stairs area (X=40-50, Z=-10 to 10) or when moving significantly
    bool nearStairs = (currentPos.x >= 40.0f && currentPos.x <= 50.0f && currentPos.z >= -10.0f && currentPos.z <= 10.0f);
    bool significantMovement = Vector3Length(intendedMovement) > 0.01f;

    if (nearStairs || significantMovement) {
        LOG_INFO("PHYSICS: Player at (" + std::to_string(currentPos.x) + "," + std::to_string(currentPos.y) + "," + std::to_string(currentPos.z) +
                 ") moving by (" + std::to_string(intendedMovement.x) + "," + std::to_string(intendedMovement.y) + "," + std::to_string(intendedMovement.z) + ")");
    }

    // Store horizontal movement for later use
    Vector3 horizontalMovement = {intendedMovement.x, 0, intendedMovement.z};
    float horizontalSpeed = Vector3Length(horizontalMovement);

    // For stair climbing, we need to detect when horizontal movement is blocked by a higher surface
    // Only trigger step-up when there's significant horizontal movement AND we're not falling fast

    // Check for collisions at target position
    if (!CheckCollisionAtPosition(entity, targetPos)) {
        // No collisions, apply full movement
        position->SetPosition(targetPos);
        collidable->UpdateBoundsFromPosition(targetPos);
        LOG_INFO("MOVEMENT: No collision, applied full movement to (" +
                 std::to_string(targetPos.x) + "," + std::to_string(targetPos.y) + "," + std::to_string(targetPos.z) + ")");
        return;
    }

    // Collision detected - but check if PlayerSystem already handled slope movement
    LOG_INFO("COLLISION: Collision detected at target pos (" + std::to_string(targetPos.x) + "," + std::to_string(targetPos.y) + "," + std::to_string(targetPos.z) +
             ") - horizontalSpeed: " + std::to_string(horizontalSpeed) + ", vertical: " + std::to_string(intendedMovement.y));

    // PRIORITY 1: Check if the collision surface is a slope - if so, project movement onto slope
    std::vector<CollisionEvent> collisionEvents = GetAllCollisions(entity, targetPos);
    
    if (!collisionEvents.empty()) {
        const CollisionEvent& firstCollision = collisionEvents[0];
        Vector3 surfaceNormal = firstCollision.normal;
        
        // Check if this is a slope surface (not vertical, not flat)
        float normalY = surfaceNormal.y;
        if (normalY > 0.1f && normalY < 0.99f && horizontalSpeed > 0.001f) {
            LOG_INFO("SLOPE COLLISION: Detected slope normal=(" +
                     std::to_string(surfaceNormal.x) + "," + std::to_string(surfaceNormal.y) + "," +
                     std::to_string(surfaceNormal.z) + ") - projecting movement");
            
            // Project the horizontal movement onto the slope plane
            Vector3 horizontalMovement = {intendedMovement.x, 0.0f, intendedMovement.z};
            
            // Project movement onto slope plane (remove component perpendicular to slope)
            float dot = Vector3DotProduct(horizontalMovement, surfaceNormal);
            Vector3 projectedMovement = Vector3Subtract(horizontalMovement, Vector3Scale(surfaceNormal, dot));
            
            // Calculate Y movement to follow the slope
            float slopeMovementY = 0.0f;
            if (Vector3Length(horizontalMovement) > 0.001f) {
                // Use slope angle to calculate Y component
                Vector3 slopeDirection = Vector3Normalize({surfaceNormal.x, 0.0f, surfaceNormal.z});
                Vector3 movementDirection = Vector3Normalize(horizontalMovement);
                float slopeAngle = acosf(surfaceNormal.y);
                float horizontalDistance = Vector3Length(projectedMovement);
                slopeMovementY = horizontalDistance * tanf(slopeAngle);
                
                // Determine if moving up or down slope
                float slopeDot = Vector3DotProduct(movementDirection, slopeDirection);
                if (slopeDot > 0) slopeMovementY = -slopeMovementY; // Moving up slope
            }
            
            // Combine projected horizontal movement with slope Y and preserve vertical movement
            Vector3 finalMovement = {projectedMovement.x, slopeMovementY + intendedMovement.y, projectedMovement.z};
            Vector3 finalPos = Vector3Add(currentPos, finalMovement);
            
            position->SetPosition(finalPos);
            collidable->UpdateBoundsFromPosition(finalPos);
            
            // Update velocity to reflect slope movement
            Vector3 slopeVelocity = Vector3Scale(finalMovement, 1.0f / deltaTime);
            velocity->SetVelocity(slopeVelocity);
            
            LOG_INFO("SLOPE MOVEMENT: Applied projected movement (" + std::to_string(finalMovement.x) + "," + 
                     std::to_string(finalMovement.y) + "," + std::to_string(finalMovement.z) + ")");
            return;
        }
    }

    // PRIORITY 2: Try step-up for stairs/obstacles (only if surface normal indicates upward-facing surface)
    if (horizontalSpeed > 0.001f && intendedMovement.y >= -0.1f) { // Only try step-up if not falling fast
        LOG_INFO("COLLISION: Attempting step-up at pos (" +
                 std::to_string(currentPos.x) + "," + std::to_string(currentPos.y) + "," + std::to_string(currentPos.z) +
                 ") with horizontal speed " + std::to_string(horizontalSpeed));

        // Try to step up over the obstacle
        if (TryStepUp(entity, horizontalMovement)) {
            LOG_INFO("STEP-UP: Successfully stepped up over obstacle!");
            return; // Successfully stepped up
        } else {
            LOG_INFO("STEP-UP: Failed to step up - obstacle too high or no valid step surface");
        }
    }

    // PRIORITY 3: Fallback to constraint-based collision resolution
    LOG_INFO("COLLISION: Using constraint-based collision resolution");
    Vector3 finalMovement = ResolveConstrainedMovement(entity, currentPos, intendedMovement);

    // Apply constraint-resolved movement
    Vector3 finalPos = Vector3Add(currentPos, finalMovement);
    position->SetPosition(finalPos);
    collidable->UpdateBoundsFromPosition(finalPos);

    // Update velocity based on movement constraints
    Vector3 actualMovement = Vector3Scale(finalMovement, 1.0f / deltaTime);
    velocity->SetVelocity(actualMovement);

    LOG_INFO("COLLISION: Applied constraint movement to (" +
             std::to_string(finalPos.x) + "," + std::to_string(finalPos.y) + "," + std::to_string(finalPos.z) + ")");
}

void PhysicsSystem::HandleCollision(Entity* entity, const Vector3& movement, const Vector3& surfaceNormal) {
    auto* position = entity->GetComponent<Position>();
    auto* velocity = entity->GetComponent<Velocity>();
    auto* collidable = entity->GetComponent<Collidable>();

    if (!position || !velocity || !collidable) return;

    // Simple collision response - just stop movement in the collision direction
    // Classify surface type based on normal
    float absX = fabsf(surfaceNormal.x);
    float absY = fabsf(surfaceNormal.y);
    float absZ = fabsf(surfaceNormal.z);

    if (absY > absX && absY > absZ) {
        // Floor/Ceiling collision (Y-dominant normal)
        if (surfaceNormal.y > 0.0f) {
            // Floor - stop downward movement
            if (movement.y < 0.0f) {
                velocity->SetY(0.0f);
            }
            // Allow horizontal movement with sliding
            Vector3 horizontalMovement = {movement.x, 0.0f, movement.z};
            if (Vector3Length(horizontalMovement) > 0.001f) {
                Vector3 testPos = Vector3Add(position->GetPosition(), horizontalMovement);
                if (!CheckCollisionAtPosition(entity, testPos)) {
                    position->Move(horizontalMovement);
                }
            }
        } else {
            // Ceiling - stop upward movement
            if (movement.y > 0.0f) {
                velocity->SetY(0.0f);
            }
        }
    } else {
        // Wall collision (X/Z-dominant normal) - stop movement perpendicular to wall
        if (absX > absZ) {
            // X-facing wall
            velocity->SetX(0.0f);
            // Allow Z movement
            Vector3 slideMovement = {0.0f, movement.y, movement.z};
            if (Vector3Length(slideMovement) > 0.001f) {
                Vector3 testPos = Vector3Add(position->GetPosition(), slideMovement);
                if (!CheckCollisionAtPosition(entity, testPos)) {
                    position->Move(slideMovement);
                }
            }
        } else {
            // Z-facing wall
            velocity->SetZ(0.0f);
            // Allow X movement
            Vector3 slideMovement = {movement.x, movement.y, 0.0f};
            if (Vector3Length(slideMovement) > 0.001f) {
                Vector3 testPos = Vector3Add(position->GetPosition(), slideMovement);
                if (!CheckCollisionAtPosition(entity, testPos)) {
                    position->Move(slideMovement);
                }
            }
        }
    }

    // Update collidable bounds
    collidable->UpdateBoundsFromPosition(position->GetPosition());
}

void PhysicsSystem::HandleCollisionResponse(Entity* entity, const Vector3& movement, const Vector3& surfaceNormal) {
    auto* velocity = entity->GetComponent<Velocity>();
    if (!velocity) return;

    Vector3 currentVel = velocity->GetVelocity();

    // Use the enhanced slide velocity function for smoother collision response
    Vector3 slidVel = SlideVelocity(currentVel, surfaceNormal);
    
    // Apply minimal friction based on surface type
    float absX = fabsf(surfaceNormal.x);
    float absY = fabsf(surfaceNormal.y);
    float absZ = fabsf(surfaceNormal.z);

    if (absY > absX && absY > absZ) {
        // Floor/Ceiling collision - stop vertical movement
        slidVel.y = 0.0f;
    } else {
        // Wall collision - apply very light friction to prevent sticking
        const float WALL_FRICTION = 0.995f; // Reduced friction for smoother sliding
        
        // Only apply friction to the perpendicular component
        if (absX > absZ) {
            // X-facing wall
            slidVel.z *= WALL_FRICTION;
        } else {
            // Z-facing wall  
            slidVel.x *= WALL_FRICTION;
        }
    }
    
    // Apply anti-jitter velocity clamping
    if (Vector3Length(slidVel) < VELOCITY_EPSILON) {
        slidVel = {0, 0, 0};
    }

    velocity->SetVelocity(slidVel);
}

std::vector<CollisionEvent> PhysicsSystem::GetAllCollisions(Entity* entity, const Vector3& position) const {
    std::vector<CollisionEvent> collisions;
    auto* collidable = entity->GetComponent<Collidable>();
    if (!collidable || !collisionSystem_) {
        return collisions;
    }

    // Cast to CollisionSystem to access BSP tree
    auto collisionSys = dynamic_cast<CollisionSystem*>(collisionSystem_);
    if (!collisionSys) {
        return collisions;
    }

    // Create AABB from position and size
    Vector3 size = collidable->GetBounds().GetSize();
    AABB playerBounds;
    playerBounds.min.x = position.x - size.x / 2.0f;
    playerBounds.min.y = position.y - size.y / 2.0f;
    playerBounds.min.z = position.z - size.z / 2.0f;
    playerBounds.max.x = position.x + size.x / 2.0f;
    playerBounds.max.y = position.y + size.y / 2.0f;
    playerBounds.max.z = position.z + size.z / 2.0f;

    // Check collision against all faces in the BSP tree
    const auto& faces = collisionSys->GetBSPTree()->GetAllFaces();
    for (const auto& face : faces) {
        // Skip non-collidable faces
        if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;

        // Simple AABB vs triangle intersection check
        if (collisionSys->CheckAABBIntersectsTriangle(playerBounds, face.vertices)) {
            // Calculate penetration depth
            float penetrationDepth = collisionSys->GetPenetrationDepth(playerBounds, face.vertices, face.normal);
            

            
            // Add to collision list
            collisions.emplace_back(nullptr, nullptr, position, face.normal, penetrationDepth);
        }
    }

    return collisions;
}

CollisionEvent PhysicsSystem::GetDetailedCollision(Entity* entity, const Vector3& position, const Vector3& movement) const {
    auto* collidable = entity->GetComponent<Collidable>();
    if (!collidable || !collisionSystem_) {
        return CollisionEvent(nullptr, nullptr, position, {0,0,0}, 0.0f);
    }

    // Cast to CollisionSystem to access the method
    auto collisionSys = dynamic_cast<CollisionSystem*>(collisionSystem_);
    if (collisionSys) {
        return collisionSys->GetDetailedCollisionWithWorld(*collidable, position);
    }

    return CollisionEvent(nullptr, nullptr, position, {0,0,0}, 0.0f);
}

Vector3 PhysicsSystem::ResolveCollisionsSequentially(Entity* entity, const Vector3& startPos, const Vector3& intendedMovement) {
    auto* collidable = entity->GetComponent<Collidable>();
    if (!collidable) return Vector3Add(startPos, intendedMovement);

    Vector3 currentPos = startPos;

    // Check for collisions at target position
    std::vector<CollisionEvent> collisions = GetAllCollisions(entity, Vector3Add(startPos, intendedMovement));

    if (collisions.empty()) {
        // No collisions, apply full movement
        return Vector3Add(startPos, intendedMovement);
    }

    // Sort collisions by penetration depth
    std::sort(collisions.begin(), collisions.end(),
        [](const CollisionEvent& a, const CollisionEvent& b) {
            return a.penetrationDepth > b.penetrationDepth;
        });

    // Handle the deepest collision first
    const CollisionEvent& collision = collisions[0];

    // Apply position correction
    Vector3 correction = Vector3Scale(collision.normal, collision.penetrationDepth + 0.001f);
    currentPos = Vector3Add(currentPos, correction);

    // Apply sliding movement for walls
    Vector3 slideMovement = intendedMovement;
    float absX = fabsf(collision.normal.x);
    float absY = fabsf(collision.normal.y);
    float absZ = fabsf(collision.normal.z);

    if (!(absY > absX && absY > absZ)) {
        // Wall collision - project movement onto surface plane
        float dotProduct = Vector3DotProduct(slideMovement, collision.normal);
        Vector3 normalComponent = Vector3Scale(collision.normal, dotProduct);
        slideMovement = Vector3Subtract(slideMovement, normalComponent);
    } else {
        // Floor/ceiling collision - stop vertical movement
        slideMovement.y = 0.0f;
    }

    // Apply collision response
    HandleCollisionResponse(entity, slideMovement, collision.normal);

    return Vector3Add(currentPos, slideMovement);
}


void PhysicsSystem::HandleMultipleCollisions(Entity* entity, const Vector3& intendedMovement, const std::vector<CollisionEvent>& collisions) {
    // This function is now deprecated - sequential resolution is handled in ResolveCollisionsSequentially
    // Keep for backward compatibility but delegate to the new system
    auto* position = entity->GetComponent<Position>();
    auto* collidable = entity->GetComponent<Collidable>();

    if (!position || !collidable || collisions.empty()) return;

    Vector3 resolvedPos = ResolveCollisionsSequentially(entity, position->GetPosition(), intendedMovement);
    position->SetPosition(resolvedPos);
    collidable->UpdateBoundsFromPosition(resolvedPos);
}

bool PhysicsSystem::WouldCollideWithAny(Entity* entity, const Vector3& position, const std::vector<CollisionEvent>& collisions, size_t excludeIndex) const {
    auto* collidable = entity->GetComponent<Collidable>();
    if (!collidable || !collisionSystem_) return false;

    auto collisionSys = dynamic_cast<CollisionSystem*>(collisionSystem_);
    if (!collisionSys) return false;

    // Check if this position would intersect any of the collision triangles
    Vector3 size = collidable->GetBounds().GetSize();
    AABB playerBounds;
    playerBounds.min.x = position.x - size.x / 2.0f;
    playerBounds.min.y = position.y - size.y / 2.0f;
    playerBounds.min.z = position.z - size.z / 2.0f;
    playerBounds.max.x = position.x + size.x / 2.0f;
    playerBounds.max.y = position.y + size.y / 2.0f;
    playerBounds.max.z = position.z + size.z / 2.0f;

    for (size_t i = 0; i < collisions.size(); ++i) {
        if (i == excludeIndex) continue;

        // We need to find the face that corresponds to this collision
        // For now, just check if any collision would occur at this position
        if (collisionSys->CheckCollisionWithWorld(*collidable, position)) {
            return true;
        }
    }

    return false;
}

void PhysicsSystem::TryHorizontalMovement(Entity* entity, const Vector3& intendedMovement) {
    auto* position = entity->GetComponent<Position>();

    if (!position) return;

    // Try to move horizontally, allowing sliding along surfaces
    Vector3 horizontalMovement = {intendedMovement.x, 0.0f, intendedMovement.z};

    if (Vector3Length(horizontalMovement) > 0.001f) {
        Vector3 testPos = Vector3Add(position->GetPosition(), horizontalMovement);
        if (!CheckCollisionAtPosition(entity, testPos)) {
            position->Move(horizontalMovement);
        } else {
            // Try sliding - move only in X direction
            Vector3 slideX = {intendedMovement.x, 0.0f, 0.0f};
            Vector3 testPosX = Vector3Add(position->GetPosition(), slideX);
            if (!CheckCollisionAtPosition(entity, testPosX)) {
                position->Move(slideX);
            }

            // Try sliding - move only in Z direction
            Vector3 slideZ = {0.0f, 0.0f, intendedMovement.z};
            Vector3 testPosZ = Vector3Add(position->GetPosition(), slideZ);
            if (!CheckCollisionAtPosition(entity, testPosZ)) {
                position->Move(slideZ);
            }
        }
    }
}

float PhysicsSystem::GetSurfaceHeightAtPosition(const Vector3& position) const {
    // Cast ray down from high above to find surface height
    const float RAY_START_HEIGHT = 10.0f; // Start ray 10 units above
    const float RAY_LENGTH = 20.0f;       // Cast down 20 units

    Vector3 rayStart = {position.x, position.y + RAY_START_HEIGHT, position.z};
    Vector3 rayDirection = {0, -1, 0}; // Downward ray

    // Get BSP tree for raycasting
    const BSPTree* bspTree = nullptr;
    if (collisionSystem_) {
        bspTree = dynamic_cast<CollisionSystem*>(collisionSystem_)->GetBSPTree();
    }

    if (bspTree) {
        float distance = bspTree->CastRay(rayStart, rayDirection, RAY_LENGTH);
        if (distance > 0 && distance < RAY_LENGTH) {
            return rayStart.y - distance; // Surface height
        }
    }

    // No surface found, return current position Y as fallback
    return position.y;
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
    auto* velocity = playerEntity->GetComponent<Velocity>();

    if (!player || !position || !collidable || !velocity) return;

    Vector3 playerPos = position->GetPosition();
    Vector3 playerSize = collidable->GetBounds().GetSize();
    Vector3 currentVel = velocity->GetVelocity();

    // Check if player is on ground (any surface)
    bool onGround = IsOnGround(playerPos, playerSize);

    PlayerState currentState = player->GetState();

    // Enhanced state transitions for platform support
    if (onGround) {
        // Player is on some surface
        if (currentState == PlayerState::IN_AIR) {
            // Just landed on a surface (ground, platform, etc.)
            player->SetState(PlayerState::ON_GROUND);
            HandlePlayerLanding(playerEntity);
            LOG_INFO("Player landed on surface at Y=" + std::to_string(playerPos.y));
        } else if (currentState == PlayerState::ON_GROUND) {
            // Already on ground, ensure we stay grounded on platforms
            // This handles the case where player is walking on platforms above ground level
            LOG_INFO("Player staying grounded on surface at Y=" + std::to_string(playerPos.y));
        }
        // If crouching and on ground, stay crouching
    } else {
        // Player is in air
        if (currentState != PlayerState::IN_AIR) {
            // Just left the surface (jumped or fell off)
            player->SetState(PlayerState::IN_AIR);
            LOG_INFO("Player left surface, now in air at Y=" + std::to_string(playerPos.y) +
                     " with velocity Y=" + std::to_string(currentVel.y));
        }
    }

    // Additional check: If player has very low vertical velocity and is close to a surface,
    // ensure they stay grounded (helps with platform edges)
    if (currentState == PlayerState::IN_AIR && fabsf(currentVel.y) < 1.0f) {
        // Re-check grounding with a slightly more lenient check
        Vector3 testPos = Vector3Add(playerPos, {0, -0.1f, 0}); // Test 10cm below
        if (IsOnGround(testPos, playerSize)) {
            player->SetState(PlayerState::ON_GROUND);
            HandlePlayerLanding(playerEntity);
            LOG_INFO("Player snapped to ground from near-surface position");
        }
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
    // Check if we have a collision system with BSP tree
    const BSPTree* bspTree = nullptr;
    if (collisionSystem_) {
        bspTree = dynamic_cast<CollisionSystem*>(collisionSystem_)->GetBSPTree();
    }

    if (!bspTree) {
        // Fallback: If no BSP tree, assume player is on ground at Y=0
        // This prevents infinite falling in test scenarios
        float groundThreshold = size.y/2.0f + 0.1f;
        bool onGround = position.y <= groundThreshold;
        LOG_INFO("Ground check: PosY=" + std::to_string(position.y) +
                 " SizeY=" + std::to_string(size.y) + " Threshold=" + std::to_string(groundThreshold) +
                 " onGround=" + (onGround ? "true" : "false"));
        return onGround;
    }

    // Enhanced ground check: Check for collision slightly below the player's feet
    Vector3 bottomCenter = {position.x, position.y - size.y/2.0f, position.z};
    Vector3 testPosition = {bottomCenter.x, bottomCenter.y - 0.05f, bottomCenter.z}; // 5cm below feet

    // Create a small AABB to test for collision below the player
    AABB testBounds;
    float halfWidth = size.x * 0.4f; // Slightly smaller than player for edge detection
    float halfDepth = size.z * 0.4f;
    testBounds.min = {testPosition.x - halfWidth, testPosition.y - 0.02f, testPosition.z - halfDepth};
    testBounds.max = {testPosition.x + halfWidth, testPosition.y + 0.02f, testPosition.z + halfDepth};

    // Check collision using existing BSP collision system
    bool hasGroundBelow = false;
    if (collisionSystem_) {
        Collidable testCollidable({halfWidth*2, 0.04f, halfDepth*2});
        hasGroundBelow = dynamic_cast<CollisionSystem*>(collisionSystem_)->CheckCollisionWithWorld(testCollidable, testPosition);
    }

    // Additional check: If no ground below but player has downward velocity very close to zero,
    // check if player is penetrating any surface from above (standing on top of something)
    if (!hasGroundBelow) {
        // Create player AABB at current position
        AABB playerBounds;
        playerBounds.min.x = position.x - size.x/2.0f;
        playerBounds.min.y = position.y - size.y/2.0f;
        playerBounds.min.z = position.z - size.z/2.0f;
        playerBounds.max.x = position.x + size.x/2.0f;
        playerBounds.max.y = position.y + size.y/2.0f;
        playerBounds.max.z = position.z + size.z/2.0f;

        // Check if player is currently colliding with any surface and find the highest surface
        float highestSurfaceY = -FLT_MAX;
        bool foundSurface = false;

        const auto& faces = collisionSystem_ ?
            dynamic_cast<CollisionSystem*>(collisionSystem_)->GetBSPTree()->GetAllFaces() :
            std::vector<Face>();

        for (const auto& face : faces) {
            if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;

            // Check if player AABB intersects this face
            if (dynamic_cast<CollisionSystem*>(collisionSystem_)->CheckAABBIntersectsTriangle(playerBounds, face.vertices)) {
                // Calculate triangle normal and check if it's a floor-like surface
                Vector3 edge1 = Vector3Subtract(face.vertices[1], face.vertices[0]);
                Vector3 edge2 = Vector3Subtract(face.vertices[2], face.vertices[0]);
                Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

                // If normal points mostly upward (Y-dominant), it's a potential ground surface
                if (fabsf(normal.y) > fabsf(normal.x) && fabsf(normal.y) > fabsf(normal.z) && normal.y > 0.0f) {
                    // Find the highest Y coordinate of the triangle
                    float maxY = std::max({face.vertices[0].y, face.vertices[1].y, face.vertices[2].y});
                    if (maxY > highestSurfaceY) {
                        highestSurfaceY = maxY;
                        foundSurface = true;
                    }
                }
            }
        }

        // If we found a surface and the player is very close to it (within epsilon), consider grounded
        if (foundSurface) {
            float surfaceEpsilon = 0.1f; // 10cm tolerance for standing on surfaces
            float playerBottomY = position.y - size.y/2.0f;
            if (playerBottomY >= highestSurfaceY - surfaceEpsilon && playerBottomY <= highestSurfaceY + surfaceEpsilon) {
                hasGroundBelow = true;
                LOG_INFO("Platform ground check: Player bottom Y=" + std::to_string(playerBottomY) +
                         " Surface Y=" + std::to_string(highestSurfaceY) + " - GROUNDED on platform!");
            }
        }
    }

    LOG_INFO("Enhanced ground check: PosY=" + std::to_string(position.y) +
             " TestY=" + std::to_string(testPosition.y) + " hasGroundBelow=" + (hasGroundBelow ? "true" : "false"));

    return hasGroundBelow;
}

Vector3 PhysicsSystem::GetGroundNormal(const Vector3& position) const {
    // Use proper raycasting for accurate ground normal detection
    if (!collisionSystem_) {
        LOG_INFO("GROUND NORMAL: No collision system available, using default up vector");
        return {0.0f, 1.0f, 0.0f}; // Default up vector if no collision system
    }

    auto collisionSys = dynamic_cast<CollisionSystem*>(collisionSystem_);
    if (!collisionSys) {
        LOG_INFO("GROUND NORMAL: Collision system cast failed, using default up vector");
        return {0.0f, 1.0f, 0.0f};
    }

    const BSPTree* bspTree = collisionSys->GetBSPTree();
    if (!bspTree) {
        LOG_INFO("GROUND NORMAL: No BSP tree available, using default up vector");
        return {0.0f, 1.0f, 0.0f};
    }

    // Cast ray downward from slightly above the position to find ground surface
    const float RAY_START_OFFSET = 0.1f;  // Start ray closer to position to avoid gaps
    const float RAY_LENGTH = 1.5f;        // Shorter ray to focus on immediate surface

    Vector3 rayStart = {position.x, position.y + RAY_START_OFFSET, position.z};
    Vector3 rayDirection = {0.0f, -1.0f, 0.0f}; // Straight down
    Vector3 hitNormal;

    LOG_INFO("GROUND NORMAL: Casting ray from (" + std::to_string(rayStart.x) + "," + std::to_string(rayStart.y) + "," + std::to_string(rayStart.z) +
             ") downward " + std::to_string(RAY_LENGTH) + " units");

    float hitDistance = bspTree->CastRayWithNormal(rayStart, rayDirection, RAY_LENGTH, hitNormal);

    LOG_INFO("GROUND NORMAL: Raycast result - distance: " + std::to_string(hitDistance) +
             ", max distance: " + std::to_string(RAY_LENGTH) +
             ", hit: " + (hitDistance < RAY_LENGTH ? "YES" : "NO"));

    if (hitDistance < RAY_LENGTH) {
        // We hit something - this is our ground normal
        LOG_INFO("GROUND NORMAL: Hit surface at distance " + std::to_string(hitDistance) +
                 ", normal: (" + std::to_string(hitNormal.x) + "," + std::to_string(hitNormal.y) + "," + std::to_string(hitNormal.z) +
                 ") at position (" + std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) + ")");

        // Check if this might be a slope surface
        if (hitNormal.y >= 0.7f && hitNormal.y < 0.99f) {
            LOG_INFO("GROUND NORMAL: *** POTENTIAL SLOPE DETECTED *** Normal Y=" + std::to_string(hitNormal.y) +
                     " (should trigger slope movement!)");
        } else if (hitNormal.y >= 0.99f) {
            LOG_INFO("GROUND NORMAL: Flat ground detected (Normal Y=" + std::to_string(hitNormal.y) + ")");
        }

        // Validate normal - should point somewhat upward for ground
        if (hitNormal.y > -0.1f) {  // Allow slightly downward normals for overhangs
            return hitNormal;
        } else {
            LOG_INFO("GROUND NORMAL: Invalid ground normal (points too downward), using default up vector");
            return {0.0f, 1.0f, 0.0f};
        }
    } else {
        LOG_INFO("GROUND NORMAL: No surface hit within " + std::to_string(RAY_LENGTH) + " units, using default up vector");
        return {0.0f, 1.0f, 0.0f};
    }
}

// Enhanced collision handling methods implementation

Vector3 PhysicsSystem::ResolveCornerCollision(const Vector3& velocity, const std::vector<CollisionPlane>& planes) {
    if (planes.empty()) return velocity;
    
    if (planes.size() == 1) {
        // Single surface - standard sliding
        return SlideVelocity(velocity, planes[0].normal);
    }
    
    if (planes.size() == 2) {
        // Two surfaces - slide along crease
        Vector3 crease = Vector3CrossProduct(planes[0].normal, planes[1].normal);
        float creaseLength = Vector3Length(crease);
        
        if (creaseLength > 0.001f) {
            crease = Vector3Normalize(crease);
            float projection = Vector3DotProduct(velocity, crease);
            return Vector3Scale(crease, projection);
        }
    }
    
    // Three or more surfaces - stop movement (corner trap)
    return {0, 0, 0};
}

Vector3 PhysicsSystem::SlideVelocity(const Vector3& velocity, const Vector3& normal) {
    float dotProduct = Vector3DotProduct(velocity, normal);
    if (dotProduct >= 0) return velocity; // Moving away from surface
    
    Vector3 normalComponent = Vector3Scale(normal, dotProduct);
    return Vector3Subtract(velocity, normalComponent);
}


Vector3 PhysicsSystem::ResolveConstrainedMovement(Entity* entity, const Vector3& currentPos, const Vector3& intendedMovement) {
    // Constraint-based collision: prevent movement into collision zones, allow sliding

    // Try movement components separately for smooth sliding
    Vector3 remainingMovement = intendedMovement;
    Vector3 finalMovement = {0, 0, 0};

    // Try X movement first
    if (fabsf(remainingMovement.x) > 0.001f) {
        Vector3 xMovement = {remainingMovement.x, 0, 0};
        Vector3 testPos = Vector3Add(Vector3Add(currentPos, finalMovement), xMovement);

        if (!CheckCollisionAtPosition(entity, testPos)) {
            finalMovement.x += remainingMovement.x;
        }
    }

    // Try Z movement
    if (fabsf(remainingMovement.z) > 0.001f) {
        Vector3 zMovement = {0, 0, remainingMovement.z};
        Vector3 testPos = Vector3Add(Vector3Add(currentPos, finalMovement), zMovement);

        if (!CheckCollisionAtPosition(entity, testPos)) {
            finalMovement.z += remainingMovement.z;
        }
    }

    // Try Y movement (gravity/jumping)
    if (fabsf(remainingMovement.y) > 0.001f) {
        Vector3 yMovement = {0, remainingMovement.y, 0};
        Vector3 testPos = Vector3Add(Vector3Add(currentPos, finalMovement), yMovement);

        if (!CheckCollisionAtPosition(entity, testPos)) {
            finalMovement.y += remainingMovement.y;
        }
    }

    // If we couldn't move in individual axes, try diagonal combinations
    if (Vector3Length(finalMovement) < 0.001f && Vector3Length(intendedMovement) > 0.001f) {
        // Try XZ diagonal movement (common for corner sliding)
        Vector3 xzMovement = {intendedMovement.x, 0, intendedMovement.z};
        if (Vector3Length(xzMovement) > 0.001f) {
            Vector3 testPos = Vector3Add(currentPos, xzMovement);
            if (!CheckCollisionAtPosition(entity, testPos)) {
                finalMovement.x = intendedMovement.x;
                finalMovement.z = intendedMovement.z;
            }
        }
    }

    return finalMovement;
}

void PhysicsSystem::ApplyUnstuckCorrection(Entity* entity, float deltaTime) {
    auto* position = entity->GetComponent<Position>();
    auto* collidable = entity->GetComponent<Collidable>();
    auto* velocity = entity->GetComponent<Velocity>();
    
    if (!position || !collidable || !velocity) return;
    
    Vector3 currentPos = position->GetPosition();
    
    // Check if player is currently inside geometry
    if (!CheckCollisionAtPosition(entity, currentPos)) {
        return; // Not stuck, nothing to do
    }
    
    // Player is stuck in geometry - find the best direction to unstuck
    Vector3 unstuckDirection = {0, 0, 0};
    float bestDistance = 0.0f;
    
    // Try different directions to find the shortest path out
    Vector3 testDirections[] = {
        {1, 0, 0}, {-1, 0, 0},    // X axis
        {0, 1, 0}, {0, -1, 0},    // Y axis  
        {0, 0, 1}, {0, 0, -1},    // Z axis
        {0.707f, 0, 0.707f}, {-0.707f, 0, 0.707f},   // Diagonal XZ
        {0.707f, 0, -0.707f}, {-0.707f, 0, -0.707f}  // Diagonal XZ
    };
    
    const float MAX_UNSTUCK_DISTANCE = 1.0f; // Maximum distance to check
    const float UNSTUCK_STEP = 0.05f;        // Step size for testing
    
    for (const Vector3& dir : testDirections) {
        for (float dist = UNSTUCK_STEP; dist <= MAX_UNSTUCK_DISTANCE; dist += UNSTUCK_STEP) {
            Vector3 testPos = Vector3Add(currentPos, Vector3Scale(dir, dist));
            
            if (!CheckCollisionAtPosition(entity, testPos)) {
                // Found a free position in this direction
                if (bestDistance == 0.0f || dist < bestDistance) {
                    bestDistance = dist;
                    unstuckDirection = dir;
                }
                break; // Found closest free position in this direction
            }
        }
    }
    
    // Apply unstuck correction if we found a way out
    if (bestDistance > 0.0f) {
        Vector3 correctionMovement = Vector3Scale(unstuckDirection, bestDistance);
        Vector3 newPos = Vector3Add(currentPos, correctionMovement);
        
        // Apply correction gradually to avoid jittery movement
        const float UNSTUCK_SPEED = 2.0f; // Units per second
        float maxMovement = UNSTUCK_SPEED * deltaTime;
        
        if (bestDistance > maxMovement) {
            // Move gradually towards the free position
            Vector3 gradualMovement = Vector3Scale(unstuckDirection, maxMovement);
            newPos = Vector3Add(currentPos, gradualMovement);
        }
        
        position->SetPosition(newPos);
        collidable->UpdateBoundsFromPosition(newPos);
        
        LOG_INFO("UNSTUCK: Applied correction (" + std::to_string(correctionMovement.x) + "," + 
                 std::to_string(correctionMovement.y) + "," + std::to_string(correctionMovement.z) + 
                 ") distance=" + std::to_string(bestDistance));
    }
}



bool PhysicsSystem::TryStepUp(Entity* entity, const Vector3& intendedMovement) {
    auto* position = entity->GetComponent<Position>();
    auto* collidable = entity->GetComponent<Collidable>();
    auto* velocity = entity->GetComponent<Velocity>();

    if (!position || !collidable || !velocity) return false;

    Vector3 currentPos = position->GetPosition();

    // Only attempt step-up for horizontal movement
    Vector3 horizontalMovement = {intendedMovement.x, 0, intendedMovement.z};
    if (Vector3Length(horizontalMovement) < 0.001f) {
        LOG_INFO("STEP-UP: Skipping - no horizontal movement");
        return false;
    }

    LOG_INFO("STEP-UP: Attempting step-up with horizontal movement: (" + 
             std::to_string(horizontalMovement.x) + "," + std::to_string(horizontalMovement.z) + ")");

    // Industry standard approach: Simple step offset
    const float STEP_OFFSET = STEP_HEIGHT; // Use the defined constant (0.6 units)
    const float STEP_EPSILON = 0.02f; // Small buffer for precision

    // Try to move up by step offset + intended horizontal movement
    Vector3 liftedPos = Vector3Add(currentPos, {horizontalMovement.x, STEP_OFFSET, horizontalMovement.z});

    LOG_INFO("STEP-UP: Testing lifted position: (" + 
             std::to_string(liftedPos.x) + "," + std::to_string(liftedPos.y) + "," + std::to_string(liftedPos.z) + ")");

    // Check if we can stand at the lifted position (no collision above)
    if (CheckCollisionAtPosition(entity, liftedPos)) {
        LOG_INFO("STEP-UP: Collision at lifted position - cannot step up");
        return false;
    }

    // Check if there's solid ground below the lifted position
    // This uses the existing ground detection logic
    bool hasGroundBelow = IsOnGround(liftedPos, collidable->GetBounds().GetSize());
    
    if (hasGroundBelow) {
        LOG_INFO("STEP-UP: Found ground below lifted position - successful step up");
        
        // Move to lifted position
        position->SetPosition(liftedPos);
        collidable->UpdateBoundsFromPosition(liftedPos);
        
        // Reset vertical velocity to prevent bouncing
        velocity->SetY(0.0f);
        
        return true;
    } else {
        // Alternative approach: Try to find the actual step surface height
        // Check for ground at various heights below the lifted position
        for (float dropHeight = STEP_EPSILON; dropHeight <= STEP_OFFSET + STEP_EPSILON; dropHeight += 0.1f) {
            Vector3 testPos = Vector3Add(liftedPos, {0, -dropHeight, 0});
            
            // Ensure we don't step down below original position
            if (testPos.y < currentPos.y - STEP_EPSILON) {
                continue;
            }
            
            if (IsOnGround(testPos, collidable->GetBounds().GetSize())) {
                LOG_INFO("STEP-UP: Found step surface at height " + std::to_string(testPos.y) + 
                         " (drop: " + std::to_string(dropHeight) + ")");
                
                position->SetPosition(testPos);
                collidable->UpdateBoundsFromPosition(testPos);
                velocity->SetY(0.0f);
                
                return true;
            }
        }
    }

    LOG_INFO("STEP-UP: No valid step surface found - step up failed");
    return false;
}

std::vector<CollisionPlane> PhysicsSystem::GatherCollisionPlanes(Entity* entity, const Vector3& position) {
    std::vector<CollisionPlane> planes;
    
    auto collisions = GetAllCollisions(entity, position);
    for (const auto& collision : collisions) {
        CollisionPlane plane;
        plane.normal = collision.normal;
        plane.distance = collision.penetrationDepth;
        plane.contactPoint = collision.contactPoint;
        plane.isContact = collision.penetrationDepth <= PENETRATION_SLOP;
        planes.push_back(plane);
    }
    
    return planes;
}

// Slope detection and handling methods
bool PhysicsSystem::IsWalkableSlope(const Vector3& normal) const {
    // Check if surface normal is within walkable slope threshold
    // normal.y represents how "vertical" the surface is (1.0 = flat ground, 0.0 = vertical wall)
    return normal.y >= SLOPE_THRESHOLD;
}

bool PhysicsSystem::IsSlopeAtPosition(const Vector3& position, Vector3& outNormal) const {
    // Get the ground normal at this position using raycasting
    Vector3 groundNormal = GetGroundNormal(position);

    LOG_INFO("SLOPE CHECK: Ground normal at (" + std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) +
             ") = (" + std::to_string(groundNormal.x) + "," + std::to_string(groundNormal.y) + "," + std::to_string(groundNormal.z) +
             ") threshold=" + std::to_string(SLOPE_THRESHOLD));

    // Normalize the normal to ensure consistent calculations
    groundNormal = Vector3Normalize(groundNormal);

    // Check if it's actually a slope (not flat ground, not a vertical wall)
    // A slope should have normal.y between SLOPE_THRESHOLD and ~0.95 (not completely flat)
    if (groundNormal.y < 0.95f && groundNormal.y >= SLOPE_THRESHOLD) {
        // Additional check: make sure it's not too vertical (wall-like)
        float horizontalComponent = sqrtf(groundNormal.x * groundNormal.x + groundNormal.z * groundNormal.z);
        if (horizontalComponent <= 0.8f) { // If horizontal component is too large, it's more wall than slope
            LOG_INFO("SLOPE CHECK: Found walkable slope! Normal Y=" + std::to_string(groundNormal.y) +
                     " horizontal=" + std::to_string(horizontalComponent) +
                     " (between " + std::to_string(SLOPE_THRESHOLD) + " and 0.95, horizontal <= 0.8)");
            outNormal = groundNormal;
            return true;
        } else {
            LOG_INFO("SLOPE CHECK: Surface too wall-like (horizontal=" + std::to_string(horizontalComponent) + " > 0.8)");
        }
    }

    LOG_INFO("SLOPE CHECK: Not a walkable slope - Normal Y=" + std::to_string(groundNormal.y) +
             " (need between " + std::to_string(SLOPE_THRESHOLD) + " and 0.95)");
    return false;
}

Vector3 PhysicsSystem::ProjectMovementOntoSlope(const Vector3& movement, const Vector3& slopeNormal) const {
    // Industry standard approach: Project movement onto the slope plane
    // This allows smooth traversal of slopes rather than treating them as stairs

    // Extract horizontal movement (X,Z plane)
    Vector3 horizontalMovement = {movement.x, 0.0f, movement.z};
    float horizontalLength = Vector3Length(horizontalMovement);

    if (horizontalLength < 0.001f) {
        // No horizontal movement to project, return original movement
        LOG_INFO("SLOPE: No horizontal movement to project, returning original movement");
        return movement;
    }

    // Normalize slope normal for consistent calculations
    Vector3 normalizedNormal = Vector3Normalize(slopeNormal);

    LOG_INFO("SLOPE: Projecting movement (" + std::to_string(movement.x) + "," + std::to_string(movement.y) + "," + std::to_string(movement.z) +
             ") onto slope normal (" + std::to_string(normalizedNormal.x) + "," + std::to_string(normalizedNormal.y) + "," + std::to_string(normalizedNormal.z) + ")");

    // Project horizontal movement onto the slope plane
    // Formula: projected = vector - (vector · normal) * normal
    // This gives us the component of movement that's parallel to the slope surface
    float dotProduct = Vector3DotProduct(horizontalMovement, normalizedNormal);
    Vector3 projectedHorizontal = Vector3Subtract(horizontalMovement, Vector3Scale(normalizedNormal, dotProduct));

    // Calculate the slope-adjusted movement
    Vector3 slopeAdjustedMovement;

    // If the projected movement is valid (not too small)
    float projectedLength = Vector3Length(projectedHorizontal);
    if (projectedLength > 0.001f) {
        // Normalize and scale to maintain original movement speed
        // This ensures we don't slow down on slopes
        Vector3 normalizedProjected = Vector3Normalize(projectedHorizontal);
        slopeAdjustedMovement.x = normalizedProjected.x * horizontalLength;
        slopeAdjustedMovement.z = normalizedProjected.z * horizontalLength;

        // Calculate vertical component needed to stay on slope
        // This creates the "slide up/down" effect on slopes
        float slopeAngle = asinf(fabsf(normalizedNormal.y)); // Get slope steepness
        float verticalAdjustment = sinf(slopeAngle) * horizontalLength;

        // Adjust Y based on slope direction
        if (normalizedNormal.y > 0) {
            // Upward slope - we need to move up
            slopeAdjustedMovement.y = verticalAdjustment;
        } else {
            // Downward slope - we need to move down
            slopeAdjustedMovement.y = -verticalAdjustment;
        }

        // Add original vertical movement (for jumping/falling)
        slopeAdjustedMovement.y += movement.y;

        LOG_INFO("SLOPE: Slope-adjusted movement = (" + std::to_string(slopeAdjustedMovement.x) + "," +
                 std::to_string(slopeAdjustedMovement.y) + "," + std::to_string(slopeAdjustedMovement.z) +
                 ") [preserved speed: " + std::to_string(horizontalLength) + ", slope angle: " +
                 std::to_string(slopeAngle * 180.0f / PI) + "°]");
    } else {
        // Projected movement too small, use original horizontal movement
        LOG_INFO("SLOPE: Projected movement too small, using original horizontal movement");
        slopeAdjustedMovement = movement;
    }

    return slopeAdjustedMovement;
}

Vector3 PhysicsSystem::ApplySlopePhysics(Entity* entity, const Vector3& intendedMovement, const Vector3& slopeNormal, float deltaTime) {
    auto* velocity = entity->GetComponent<Velocity>();
    if (!velocity) return intendedMovement;

    // Project the intended movement onto the slope surface
    Vector3 horizontalMovement = {intendedMovement.x, 0.0f, intendedMovement.z};
    float verticalMovement = intendedMovement.y;

    // Project horizontal movement onto slope plane
    Vector3 projectedMovement = ProjectMovementOntoSlope(horizontalMovement, slopeNormal);
    
    // For slopes, we want to follow the surface, not penetrate it
    // Calculate the Y-component needed to stay on the slope
    float slopeY = 0.0f;
    if (Vector3Length(horizontalMovement) > 0.001f) {
        // Calculate how much Y movement is needed to stay on slope
        float horizontalDistance = Vector3Length(horizontalMovement);
        float slopeAngle = acosf(slopeNormal.y);
        slopeY = horizontalDistance * tanf(slopeAngle);
        
        // If moving up slope, add the Y component; if down slope, subtract it
        Vector3 slopeDirection = Vector3Normalize({slopeNormal.x, 0.0f, slopeNormal.z});
        Vector3 movementDirection = Vector3Normalize(horizontalMovement);
        float dot = Vector3DotProduct(movementDirection, slopeDirection);
        
        if (dot > 0) {
            // Moving up slope
            slopeY = fabsf(slopeY);
        } else {
            // Moving down slope
            slopeY = -fabsf(slopeY);
        }
    }

    // Combine projected horizontal movement with calculated slope Y movement
    Vector3 finalMovement = {projectedMovement.x, slopeY + verticalMovement * 0.1f, projectedMovement.z};
    
    // Apply some damping to prevent jittering on slopes
    const float SLOPE_DAMPING = 0.95f;
    Vector3 currentVel = velocity->GetVelocity();
    
    // Preserve some of the original vertical movement for jumping/falling
    if (verticalMovement > 0.1f) {
        // Jumping - allow full upward movement
        finalMovement.y = verticalMovement;
    } else if (verticalMovement < -0.1f) {
        // Falling - blend with slope movement
        finalMovement.y = verticalMovement * 0.3f + slopeY * 0.7f;
    }

    LOG_INFO("SLOPE PHYSICS: Input (" + std::to_string(intendedMovement.x) + "," + 
             std::to_string(intendedMovement.y) + "," + std::to_string(intendedMovement.z) + 
             ") -> Output (" + std::to_string(finalMovement.x) + "," + 
             std::to_string(finalMovement.y) + "," + std::to_string(finalMovement.z) + 
             ") slopeY=" + std::to_string(slopeY));

    return finalMovement;
}
