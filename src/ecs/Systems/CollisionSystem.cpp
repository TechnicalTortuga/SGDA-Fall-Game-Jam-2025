#include "CollisionSystem.h"
#include "../Entity.h"
#include "../Components/Player.h"
#include <algorithm>

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
    if (!bspTree_) return false;

    // Simple point-in-BSP check (would be more sophisticated in practice)
    return bspTree_->ContainsPoint(position);
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

void CollisionSystem::OnCollisionEnter(const CollisionEvent& event) {
    // Handle collision enter events
    // This would integrate with the game's event system
}

void CollisionSystem::OnCollisionStay(const CollisionEvent& event) {
    // Handle collision stay events
}

void CollisionSystem::OnCollisionExit(const CollisionEvent& event) {
    // Handle collision exit events
}
