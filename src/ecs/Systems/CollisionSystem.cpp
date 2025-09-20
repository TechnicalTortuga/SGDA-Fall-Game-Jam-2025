#include "CollisionSystem.h"
#include "../Entity.h"
#include "../Components/Player.h"
#include <algorithm>
#include "raylib.h"

CollisionSystem::CollisionSystem()
    : bspTree_(nullptr)
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
    if (!bspTree_) return false;

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
    if (!bspTree_) return CollisionEvent(nullptr, nullptr, position, {0,0,0}, 0.0f);

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
    const auto& faces = bspTree_->GetAllFaces();
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
    if (bspTree_) {
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
    if (!bspTree_) return false;

    Vector3 normalizedDir = Vector3Normalize(direction);
    float distance = bspTree_->CastRay(origin, normalizedDir, maxDistance);

    if (distance < maxDistance) {
        hitPoint = Vector3Add(origin, Vector3Scale(normalizedDir, distance));
        // For now, return a default normal (this would be improved with proper BSP ray casting)
        hitNormal = {0, 0, -1};
        return true;
    }

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
    if (!bspTree_) {
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
    const auto& faces = bspTree_->GetAllFaces();
    int collidableFaces = 0;
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

    LOG_INFO("CheckBSPCollision: Checked " + std::to_string(collidableFaces) + " collidable faces, no collision at (" +
              std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
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

    // Calculate triangle AABB for quick rejection
    AABB triAABB;
    triAABB.min = triAABB.max = v0;
    for (const auto& vertex : triangle) {
        triAABB.min.x = fminf(triAABB.min.x, vertex.x);
        triAABB.min.y = fminf(triAABB.min.y, vertex.y);
        triAABB.min.z = fminf(triAABB.min.z, vertex.z);
        triAABB.max.x = fmaxf(triAABB.max.x, vertex.x);
        triAABB.max.y = fmaxf(triAABB.max.y, vertex.y);
        triAABB.max.z = fmaxf(triAABB.max.z, vertex.z);
    }

    // Quick rejection: if triangle AABB doesn't intersect player AABB, no collision
    if (triAABB.max.x < aabb.min.x || triAABB.min.x > aabb.max.x ||
        triAABB.max.y < aabb.min.y || triAABB.min.y > aabb.max.y ||
        triAABB.max.z < aabb.min.z || triAABB.min.z > aabb.max.z) {
        return false;
    }

    // Check if any triangle vertex is inside AABB
    for (const auto& vertex : triangle) {
        if (PointInAABB(vertex, aabb)) {
            return true;
        }
    }

    // Check if any AABB vertex is on the positive side of triangle plane
    // and if the triangle intersects the AABB
    Vector3 aabbVertices[8] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
        {aabb.min.x, aabb.max.y, aabb.max.z}
    };

    // Check triangle edges vs AABB faces
    Vector3 triEdges[3] = {edge1, edge2, Vector3Subtract(v0, v2)};

    for (int i = 0; i < 3; i++) {
        Vector3 edgeStart = (i == 2) ? v2 : v0;
        Vector3 edgeEnd = (i == 0) ? v1 : (i == 1) ? v2 : v0;

        // Check if edge intersects AABB
        if (EdgeIntersectsAABB(edgeStart, edgeEnd, aabb)) {
            return true;
        }
    }

    // If triangle AABB intersects and we're close to the plane, consider it a collision
    // Distance from AABB center to triangle plane
    Vector3 center = {
        (aabb.min.x + aabb.max.x) / 2.0f,
        (aabb.min.y + aabb.max.y) / 2.0f,
        (aabb.min.z + aabb.max.z) / 2.0f
    };

    float distance = fabsf(Vector3DotProduct(normal, Vector3Subtract(center, v0)));
    float aabbExtent = Vector3Length({aabb.max.x - aabb.min.x, aabb.max.y - aabb.min.y, aabb.max.z - aabb.min.z}) / 2.0f;

    // If AABB is close to plane and triangle AABB overlaps, likely intersection
    if (distance < aabbExtent + 0.1f) { // Small epsilon for floating point
        return true;
    }

    return false;
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
