#include "CollisionSystem.h"
#include "../Entity.h"
#include "../Components/Player.h"
#include <algorithm>
#include "raylib.h"

CollisionSystem::CollisionSystem()
    : world_(nullptr)  // Will be set by WorldSystem
    , debugBoundsVisible_(false)
{
}

void CollisionSystem::Initialize() {
    // Register with archetype bitmask for entities with Collidable component
    // This would be set up in the ECS system
}

void CollisionSystem::Shutdown() {
    collidableEntities_.clear();
    collisionPairs_.clear();
}

void CollisionSystem::Update(float deltaTime) {
    if (!IsEnabled()) return;

    // Update list of collidable entities
    UpdateCollidableEntities();

    // Check for entity-to-entity collisions
    CheckEntityCollisions();

    // Render debug collision bounds if enabled
    if (debugBoundsVisible_) {
        RenderDebugBounds();
    }

    // Process collision events (this would integrate with the event system)
}

void CollisionSystem::UpdateCollidableEntities() {
    collidableEntities_.clear();

    // Get all entities with Collidable components
    // This would be done through the ECS query system
    for (auto* entity : GetEntities()) {
        if (entity->HasComponent<Collidable>()) {
            collidableEntities_.push_back(entity);
        }
    }
}

void CollisionSystem::CheckEntityCollisions() {
    // Clear previous collision pairs
    collisionPairs_.clear();

    // Broad phase: check all pairs for potential collisions
    for (size_t i = 0; i < collidableEntities_.size(); ++i) {
        Entity* entityA = collidableEntities_[i];
        auto* collidableA = entityA->GetComponent<Collidable>();

        for (size_t j = i + 1; j < collidableEntities_.size(); ++j) {
            Entity* entityB = collidableEntities_[j];
            auto* collidableB = entityB->GetComponent<Collidable>();

            // Check layer/mask filtering
            if (!collidableA->ShouldCollideWith(*collidableB)) {
                continue;
            }

            // Narrow phase: check actual collision
            if (CheckCollision(*collidableA, *collidableB)) {
                // Record collision pair
                collisionPairs_[entityA].push_back(entityB);
                collisionPairs_[entityB].push_back(entityA);

                // Resolve collision
                ResolveEntityCollision(entityA, entityB);

                // Fire collision event
                CollisionEvent event(entityA, entityB);
                OnCollisionEnter(event);
            }
        }
    }
}

bool CollisionSystem::CheckCollision(const Collidable& a, const Collidable& b) const {
    // Skip collision if either is a trigger
    if (a.IsTrigger() || b.IsTrigger()) {
        return false;
    }

    // AABB intersection test
    return AABBIntersect(a.GetBounds(), b.GetBounds());
}

bool CollisionSystem::CheckCollisionWithWorld(const Collidable& entity, const Vector3& position) {
    if (!world_) return false;

    // Update entity's bounds to the test position
    AABB testBounds = entity.GetBounds();
    Vector3 size = testBounds.GetSize();
    testBounds.min.x = position.x - size.x / 2.0f;
    testBounds.min.y = position.y - size.y / 2.0f;
    testBounds.min.z = position.z - size.z / 2.0f;
    testBounds.max.x = position.x + size.x / 2.0f;
    testBounds.max.y = position.y + size.y / 2.0f;
    testBounds.max.z = position.z + size.z / 2.0f;

    // Check against BSP geometry
    return CheckBSPCollision(position, size);
}

CollisionEvent CollisionSystem::GetDetailedCollisionWithWorld(const Collidable& entity, const Vector3& position) const {
    if (!world_) return CollisionEvent(nullptr, nullptr, position, {0,0,0}, 0.0f);

    Vector3 size = entity.GetBounds().GetSize();

    // Create AABB from position and size
    AABB playerBounds;
    playerBounds.min.x = position.x - size.x / 2.0f;
    playerBounds.min.y = position.y - size.y / 2.0f;
    playerBounds.min.z = position.z - size.z / 2.0f;
    playerBounds.max.x = position.x + size.x / 2.0f;
    playerBounds.max.y = position.y + size.y / 2.0f;
    playerBounds.max.z = position.z + size.z / 2.0f;

    // Check collision against all faces in the BSP tree
    const auto& faces = world_->surfaces;
    LOG_INFO("COLLISION CHECK: Checking " + std::to_string(faces.size()) + " faces at position (" + 
             std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) + ")");
    
    for (const auto& face : faces) {
        // Skip non-collidable faces
        if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;

        // Simple AABB vs triangle intersection check
        if (AABBIntersectsTriangle(playerBounds, face.vertices)) {
            // Calculate penetration depth
            float penetrationDepth = CalculatePenetrationDepth(playerBounds, face.vertices, face.normal);
            LOG_INFO("COLLISION HIT: Found collision with face normal (" + std::to_string(face.normal.x) + "," + 
                     std::to_string(face.normal.y) + "," + std::to_string(face.normal.z) + ") penetration: " + 
                     std::to_string(penetrationDepth));
            // Return detailed collision info
            return CollisionEvent(nullptr, nullptr, position, face.normal, penetrationDepth);
        }
    }

    return CollisionEvent(nullptr, nullptr, position, {0,0,0}, 0.0f);
}

CollisionResponse CollisionSystem::ResolveCollision(const Collidable& entity, const Vector3& position) const {
    CollisionResponse response;

    // First check BSP collision
    if (world_) {
        response = ResolveBSPCollision(position, entity.GetBounds().GetSize());
    }

    return response;
}

bool CollisionSystem::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                             Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const {
    hitEntity = nullptr;
    float closestDistance = maxDistance;

    // Cast ray against BSP world first
    Vector3 worldHitPoint, worldHitNormal;
    if (CastRayWorldOnly(origin, direction, maxDistance, worldHitPoint, worldHitNormal)) {
        Vector3 toHit = Vector3Subtract(worldHitPoint, origin);
        float distance = Vector3Length(toHit);
        if (distance < closestDistance) {
            closestDistance = distance;
            hitPoint = worldHitPoint;
            hitNormal = worldHitNormal;
        }
    }

    // Cast ray against entities
    Vector3 normalizedDir = Vector3Normalize(direction);
    for (Entity* entity : collidableEntities_) {
        auto* collidable = entity->GetComponent<Collidable>();
        if (!collidable) continue;

        // Ray-AABB intersection test
        AABB bounds = collidable->GetBounds();
        Vector3 invDir = {1.0f/normalizedDir.x, 1.0f/normalizedDir.y, 1.0f/normalizedDir.z};

        float tmin = 0.0f;
        float tmax = maxDistance;

        for (int i = 0; i < 3; ++i) {
            float originVal = (i == 0) ? origin.x : (i == 1) ? origin.y : origin.z;
            float dirVal = (i == 0) ? invDir.x : (i == 1) ? invDir.y : invDir.z;
            float minVal = (i == 0) ? bounds.min.x : (i == 1) ? bounds.min.y : bounds.min.z;
            float maxVal = (i == 0) ? bounds.max.x : (i == 1) ? bounds.max.y : bounds.max.z;

            float t1 = (minVal - originVal) * dirVal;
            float t2 = (maxVal - originVal) * dirVal;

            tmin = std::max(tmin, std::min(t1, t2));
            tmax = std::min(tmax, std::max(t1, t2));
        }

        if (tmax >= tmin && tmin < closestDistance) {
            Vector3 entityHitPoint = Vector3Add(origin, Vector3Scale(normalizedDir, tmin));
            if (tmin < closestDistance) {
                closestDistance = tmin;
                hitPoint = entityHitPoint;
                hitNormal = GetAABBNormal(bounds, AABB({origin.x, origin.y, origin.z}, {origin.x, origin.y, origin.z}));
                hitEntity = entity;
            }
        }
    }

    return closestDistance < maxDistance;
}

bool CollisionSystem::CastRayWorldOnly(const Vector3& origin, const Vector3& direction, float maxDistance,
                                      Vector3& hitPoint, Vector3& hitNormal) const {
    // TODO: Implement ray casting for new World system
    // For now, return false (no hit)
    return false;
}

bool CollisionSystem::AABBIntersect(const AABB& a, const AABB& b) const {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

bool CollisionSystem::PointInAABB(const Vector3& point, const AABB& aabb) const {
    return (point.x >= aabb.min.x && point.x <= aabb.max.x) &&
           (point.y >= aabb.min.y && point.y <= aabb.max.y) &&
           (point.z >= aabb.min.z && point.z <= aabb.max.z);
}

Vector3 CollisionSystem::GetAABBNormal(const AABB& a, const AABB& b) const {
    // Find the axis of least penetration
    float dx1 = fabsf(a.max.x - b.min.x);
    float dx2 = fabsf(b.max.x - a.min.x);
    float dy1 = fabsf(a.max.y - b.min.y);
    float dy2 = fabsf(b.max.y - a.min.y);
    float dz1 = fabsf(a.max.z - b.min.z);
    float dz2 = fabsf(b.max.z - a.min.z);

    float minPenetration = std::min({dx1, dx2, dy1, dy2, dz1, dz2});

    if (minPenetration == dx1) return {-1, 0, 0};
    if (minPenetration == dx2) return {1, 0, 0};
    if (minPenetration == dy1) return {0, -1, 0};
    if (minPenetration == dy2) return {0, 1, 0};
    if (minPenetration == dz1) return {0, 0, -1};
    if (minPenetration == dz2) return {0, 0, 1};

    return {0, 0, 1}; // Default
}

bool CollisionSystem::CheckBSPCollision(const Vector3& position, const Vector3& size) const {
    if (!world_) {
        LOG_INFO("CheckBSPCollision: No BSP tree available");
        return false;
    }

    // Create AABB from position and size
    AABB playerBounds;
    playerBounds.min.x = position.x - size.x / 2.0f;
    playerBounds.min.y = position.y - size.y / 2.0f;
    playerBounds.min.z = position.z - size.z / 2.0f;
    playerBounds.max.x = position.x + size.x / 2.0f;
    playerBounds.max.y = position.y + size.y / 2.0f;
    playerBounds.max.z = position.z + size.z / 2.0f;

    // Check collision against all faces in the BSP tree
    const auto& faces = world_->surfaces;
    int collidableFaces = 0;
    int totalFaces = faces.size();

    static int callCount = 0;
    callCount++;
    if (callCount % 10 == 0) { // Log every 10th call to avoid spam
        LOG_INFO("CheckBSPCollision: Total faces: " + std::to_string(totalFaces) +
                 ", player bounds: (" + std::to_string(playerBounds.min.x) + "," + std::to_string(playerBounds.min.y) + "," + std::to_string(playerBounds.min.z) +
                 ") to (" + std::to_string(playerBounds.max.x) + "," + std::to_string(playerBounds.max.y) + "," + std::to_string(playerBounds.max.z) + ")");
    }

    for (const auto& face : faces) {
        // Skip non-collidable faces
        if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;
        collidableFaces++;

        // Simple AABB vs triangle intersection check
        if (AABBIntersectsTriangle(playerBounds, face.vertices)) {
            LOG_INFO("COLLISION FOUND with face normal (" +
                     std::to_string(face.normal.x) + ", " + std::to_string(face.normal.y) + ", " + std::to_string(face.normal.z) + ")");
            return true;
        }
    }

    if (callCount % 10 == 0) { // Log every 10th call to avoid spam
        LOG_INFO("CheckBSPCollision: Checked " + std::to_string(collidableFaces) + "/" + std::to_string(totalFaces) +
                 " collidable faces, no collision at (" +
                 std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
    }
    return false;
}

CollisionResponse CollisionSystem::ResolveBSPCollision(const Vector3& position, const Vector3& size) const {
    CollisionResponse response;

    // This is a simplified response - in practice, you'd do proper collision resolution
    // against the BSP surfaces

    return response;
}

void CollisionSystem::ResolveEntityCollision(Entity* entityA, Entity* entityB) {
    auto* collidableA = entityA->GetComponent<Collidable>();
    auto* collidableB = entityB->GetComponent<Collidable>();

    if (!collidableA || !collidableB) return;

    // Skip if either is static
    if (collidableA->IsStatic() || collidableB->IsStatic()) return;

    // Get positions
    auto* posA = entityA->GetComponent<Position>();
    auto* posB = entityB->GetComponent<Position>();

    if (!posA || !posB) return;

    // Simple separation (push entities apart)
    Vector3 centerA = collidableA->GetBounds().GetCenter();
    Vector3 centerB = collidableB->GetBounds().GetCenter();

    Vector3 separation = Vector3Subtract(centerB, centerA);
    float distance = Vector3Length(separation);

    if (distance > 0) {
        Vector3 normal = Vector3Scale(separation, 1.0f / distance);
        float penetration = GetAABBPenetration(collidableA->GetBounds(), collidableB->GetBounds(), normal);

        if (penetration > 0) {
            // Move entities apart
            Vector3 correction = Vector3Scale(normal, penetration * 0.5f);
            posA->Move(Vector3Scale(correction, -1.0f));
            posB->Move(correction);

            // Update collidable bounds
            collidableA->UpdateBoundsFromPosition(posA->GetPosition());
            collidableB->UpdateBoundsFromPosition(posB->GetPosition());
        }
    }
}

float CollisionSystem::GetAABBPenetration(const AABB& a, const AABB& b, Vector3& normal) const {
    // Find overlap on each axis
    float overlapX = std::min(a.max.x - b.min.x, b.max.x - a.min.x);
    float overlapY = std::min(a.max.y - b.min.y, b.max.y - a.min.y);
    float overlapZ = std::min(a.max.z - b.min.z, b.max.z - a.min.z);

    // Find axis with minimum overlap
    if (overlapX < overlapY && overlapX < overlapZ) {
        normal = (a.GetCenter().x < b.GetCenter().x) ? Vector3{-1, 0, 0} : Vector3{1, 0, 0};
        return overlapX;
    } else if (overlapY < overlapZ) {
        normal = (a.GetCenter().y < b.GetCenter().y) ? Vector3{0, -1, 0} : Vector3{0, 1, 0};
        return overlapY;
    } else {
        normal = (a.GetCenter().z < b.GetCenter().z) ? Vector3{0, 0, -1} : Vector3{0, 0, 1};
        return overlapZ;
    }
}

void CollisionSystem::BuildSpatialGrid() {
    // Spatial partitioning for broad phase optimization
    // This would be implemented for performance with many entities
}

std::vector<Entity*> CollisionSystem::QuerySpatialGrid(const AABB& bounds) const {
    // Query entities in spatial grid cells
    return collidableEntities_; // Simplified implementation
}

// Helper function to check if AABB intersects with a triangle
bool CollisionSystem::AABBIntersectsTriangle(const AABB& aabb, const std::vector<Vector3>& triangle) const {
    if (triangle.size() < 3) return false;

    Vector3 v0 = triangle[0];
    Vector3 v1 = triangle[1];
    Vector3 v2 = triangle[2];

    // Calculate triangle normal
    Vector3 edge1 = Vector3Subtract(v1, v0);
    Vector3 edge2 = Vector3Subtract(v2, v0);
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

    // For floor/ceiling collision, check if AABB intersects the infinite plane
    // Calculate distance from AABB center to plane
    Vector3 aabbCenter = {
        (aabb.min.x + aabb.max.x) * 0.5f,
        (aabb.min.y + aabb.max.y) * 0.5f,
        (aabb.min.z + aabb.max.z) * 0.5f
    };

    float distToPlane = Vector3DotProduct(normal, Vector3Subtract(aabbCenter, v0));
    float aabbHalfExtent = fabsf(normal.x) * (aabb.max.x - aabb.min.x) * 0.5f +
                          fabsf(normal.y) * (aabb.max.y - aabb.min.y) * 0.5f +
                          fabsf(normal.z) * (aabb.max.z - aabb.min.z) * 0.5f;

    // If AABB doesn't intersect the plane, no collision
    if (fabsf(distToPlane) > aabbHalfExtent) {
        return false;
    }

    // AABB intersects plane, now check if the intersection overlaps with the triangle
    // Project onto the dominant plane (usually XZ for floors)
    if (fabsf(normal.y) > 0.9f) { // Near-vertical normal (floor/ceiling)
        // Project onto XZ plane and check 2D triangle-AABB intersection
        float triMinX = fminf(fminf(v0.x, v1.x), v2.x);
        float triMaxX = fmaxf(fmaxf(v0.x, v1.x), v2.x);
        float triMinZ = fminf(fminf(v0.z, v1.z), v2.z);
        float triMaxZ = fmaxf(fmaxf(v0.z, v1.z), v2.z);

        // If we have a quad, include the 4th vertex
        if (triangle.size() >= 4) {
            const Vector3& v3 = triangle[3];
            triMinX = fminf(triMinX, v3.x);
            triMaxX = fmaxf(triMaxX, v3.x);
            triMinZ = fminf(triMinZ, v3.z);
            triMaxZ = fmaxf(triMaxZ, v3.z);
        }

        // Check if AABB projection overlaps triangle projection
        bool xOverlap = (aabb.min.x <= triMaxX) && (aabb.max.x >= triMinX);
        bool zOverlap = (aabb.min.z <= triMaxZ) && (aabb.max.z >= triMinZ);

        return xOverlap && zOverlap;
    } else {
        // For walls, use simpler AABB overlap check with triangle bounds
        float triMinX = fminf(fminf(v0.x, v1.x), v2.x);
        float triMaxX = fmaxf(fmaxf(v0.x, v1.x), v2.x);
        float triMinY = fminf(fminf(v0.y, v1.y), v2.y);
        float triMaxY = fmaxf(fmaxf(v0.y, v1.y), v2.y);
        float triMinZ = fminf(fminf(v0.z, v1.z), v2.z);
        float triMaxZ = fmaxf(fmaxf(v0.z, v1.z), v2.z);

        // If we have a quad, include the 4th vertex
        if (triangle.size() >= 4) {
            const Vector3& v3 = triangle[3];
            triMinX = fminf(triMinX, v3.x);
            triMaxX = fmaxf(triMaxX, v3.x);
            triMinY = fminf(triMinY, v3.y);
            triMaxY = fmaxf(triMaxY, v3.y);
            triMinZ = fminf(triMinZ, v3.z);
            triMaxZ = fmaxf(triMaxZ, v3.z);
        }

        // Check AABB overlap
        bool xOverlap = (aabb.min.x <= triMaxX) && (aabb.max.x >= triMinX);
        bool yOverlap = (aabb.min.y <= triMaxY) && (aabb.max.y >= triMinY);
        bool zOverlap = (aabb.min.z <= triMaxZ) && (aabb.max.z >= triMinZ);

        return xOverlap && yOverlap && zOverlap;
    }
}

// Helper function to check if an edge intersects an AABB
bool CollisionSystem::EdgeIntersectsAABB(const Vector3& edgeStart, const Vector3& edgeEnd, const AABB& aabb) const {
    Vector3 dir = Vector3Subtract(edgeEnd, edgeStart);

    // Check intersection with AABB slabs
    float tmin = 0.0f;
    float tmax = 1.0f;

    // X slab
    if (fabsf(dir.x) > 1e-6f) {
        float invDir = 1.0f / dir.x;
        if (invDir >= 0) {
            tmin = fmaxf(tmin, (aabb.min.x - edgeStart.x) * invDir);
            tmax = fminf(tmax, (aabb.max.x - edgeStart.x) * invDir);
        } else {
            tmin = fmaxf(tmin, (aabb.max.x - edgeStart.x) * invDir);
            tmax = fminf(tmax, (aabb.min.x - edgeStart.x) * invDir);
        }
    } else {
        // Edge is parallel to YZ plane
        if (edgeStart.x < aabb.min.x || edgeStart.x > aabb.max.x) {
            return false;
        }
    }

    // Y slab
    if (fabsf(dir.y) > 1e-6f) {
        float invDir = 1.0f / dir.y;
        if (invDir >= 0) {
            tmin = fmaxf(tmin, (aabb.min.y - edgeStart.y) * invDir);
            tmax = fminf(tmax, (aabb.max.y - edgeStart.y) * invDir);
        } else {
            tmin = fmaxf(tmin, (aabb.max.y - edgeStart.y) * invDir);
            tmax = fminf(tmax, (aabb.min.y - edgeStart.y) * invDir);
        }
    } else {
        // Edge is parallel to XZ plane
        if (edgeStart.y < aabb.min.y || edgeStart.y > aabb.max.y) {
            return false;
        }
    }

    // Z slab
    if (fabsf(dir.z) > 1e-6f) {
        float invDir = 1.0f / dir.z;
        if (invDir >= 0) {
            tmin = fmaxf(tmin, (aabb.min.z - edgeStart.z) * invDir);
            tmax = fminf(tmax, (aabb.max.z - edgeStart.z) * invDir);
        } else {
            tmin = fmaxf(tmin, (aabb.max.z - edgeStart.z) * invDir);
            tmax = fminf(tmax, (aabb.min.z - edgeStart.z) * invDir);
        }
    } else {
        // Edge is parallel to XY plane
        if (edgeStart.z < aabb.min.z || edgeStart.z > aabb.max.z) {
            return false;
        }
    }

    return tmin <= tmax && tmax >= 0.0f && tmin <= 1.0f;
}

// Calculate penetration depth between AABB and triangle
float CollisionSystem::CalculatePenetrationDepth(const AABB& aabb, const std::vector<Vector3>& triangle, const Vector3& normal) const {
    if (triangle.size() < 3) return 0.0f;

    // Calculate AABB center
    Vector3 center = {
        (aabb.min.x + aabb.max.x) / 2.0f,
        (aabb.min.y + aabb.max.y) / 2.0f,
        (aabb.min.z + aabb.max.z) / 2.0f
    };

    // Calculate AABB extents
    Vector3 extents = {
        (aabb.max.x - aabb.min.x) / 2.0f,
        (aabb.max.y - aabb.min.y) / 2.0f,
        (aabb.max.z - aabb.min.z) / 2.0f
    };

    // Project AABB onto triangle normal
    float centerDist = Vector3DotProduct(normal, center);
    float aabbRadius = extents.x * fabsf(normal.x) + extents.y * fabsf(normal.y) + extents.z * fabsf(normal.z);

    // Calculate distance from triangle plane to AABB center
    Vector3 v0 = triangle[0];
    float planeDist = Vector3DotProduct(normal, Vector3Subtract(center, v0));

    // Penetration depth is how far the AABB extends beyond the triangle plane
    // Positive means penetrating, negative means not touching
    float penetration = aabbRadius - fabsf(planeDist);

    return penetration > 0.0f ? penetration : 0.0f;
}

void CollisionSystem::OnCollisionEnter(const CollisionEvent& event) {
    // Handle collision enter events
    // This would integrate with the game's event system
}

void CollisionSystem::RenderDebugBounds() {
    // Draw debug collision bounds for all collidable entities
    for (auto* entity : collidableEntities_) {
        if (!entity) continue;

        auto* collidable = entity->GetComponent<Collidable>();
        auto* position = entity->GetComponent<Position>();

        if (collidable && position) {
            AABB bounds = collidable->GetBounds();

            // Set color to green for collision bounds
            Color green = {0, 255, 0, 255};

            // Draw wireframe box using DrawLine3D
            // Bottom face
            DrawLine3D({bounds.min.x, bounds.min.y, bounds.min.z},
                     {bounds.max.x, bounds.min.y, bounds.min.z}, green);
            DrawLine3D({bounds.max.x, bounds.min.y, bounds.min.z},
                     {bounds.max.x, bounds.min.y, bounds.max.z}, green);
            DrawLine3D({bounds.max.x, bounds.min.y, bounds.max.z},
                     {bounds.min.x, bounds.min.y, bounds.max.z}, green);
            DrawLine3D({bounds.min.x, bounds.min.y, bounds.max.z},
                     {bounds.min.x, bounds.min.y, bounds.min.z}, green);

            // Top face
            DrawLine3D({bounds.min.x, bounds.max.y, bounds.min.z},
                     {bounds.max.x, bounds.max.y, bounds.min.z}, green);
            DrawLine3D({bounds.max.x, bounds.max.y, bounds.min.z},
                     {bounds.max.x, bounds.max.y, bounds.max.z}, green);
            DrawLine3D({bounds.max.x, bounds.max.y, bounds.max.z},
                     {bounds.min.x, bounds.max.y, bounds.max.z}, green);
            DrawLine3D({bounds.min.x, bounds.max.y, bounds.max.z},
                     {bounds.min.x, bounds.max.y, bounds.min.z}, green);

            // Vertical edges
            DrawLine3D({bounds.min.x, bounds.min.y, bounds.min.z},
                     {bounds.min.x, bounds.max.y, bounds.min.z}, green);
            DrawLine3D({bounds.max.x, bounds.min.y, bounds.min.z},
                     {bounds.max.x, bounds.max.y, bounds.min.z}, green);
            DrawLine3D({bounds.max.x, bounds.min.y, bounds.max.z},
                     {bounds.max.x, bounds.max.y, bounds.max.z}, green);
            DrawLine3D({bounds.min.x, bounds.min.y, bounds.max.z},
                     {bounds.min.x, bounds.max.y, bounds.max.z}, green);
        }
    }
}

void CollisionSystem::OnCollisionStay(const CollisionEvent& event) {
    // Handle collision stay events
}

void CollisionSystem::OnCollisionExit(const CollisionEvent& event) {
    // Handle collision exit events
}
