#pragma once

#include "../System.h"
#include "../Components/Collidable.h"
#include "../Components/Position.h"
#include "../Components/Velocity.h"
#include "../../world/WorldGeometry.h"
#include "../../utils/Logger.h"
#include <vector>
#include <unordered_map>
#include "raylib.h"

// Collision event data
struct CollisionEvent {
    Entity* entityA;
    Entity* entityB;
    Vector3 contactPoint;
    Vector3 normal;
    float penetrationDepth;

    CollisionEvent(Entity* a = nullptr, Entity* b = nullptr,
                  const Vector3& point = {0,0,0}, const Vector3& n = {0,0,0},
                  float depth = 0.0f)
        : entityA(a), entityB(b), contactPoint(point), normal(n), penetrationDepth(depth) {}
};

// Collision response data
struct CollisionResponse {
    Vector3 correction;     // Position correction to resolve collision
    Vector3 reflection;     // Velocity reflection for bouncing
    bool shouldSlide;       // Whether to slide along the surface

    CollisionResponse() : correction{0,0,0}, reflection{0,0,0}, shouldSlide(true) {}
};

/**
 * Hybrid collision system that handles both BSP geometry and entity-to-entity collisions
 */
class CollisionSystem : public System {
public:
    CollisionSystem();
    ~CollisionSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Initialize() override;
    void Shutdown() override;

    // World geometry integration (replaces old BSP integration)
    void SetWorld(const World* world) { world_ = world; }
    bool HasWorldGeometry() const { return world_ != nullptr; }
    const World* GetWorld() const { return world_; }

    // Collision queries
    bool CheckCollision(const Collidable& a, const Collidable& b) const;
    bool CheckCollisionWithWorld(const Collidable& entity, const Vector3& position) const;
    CollisionResponse ResolveCollision(const Collidable& entity, const Vector3& position) const;
    CollisionEvent GetDetailedCollisionWithWorld(const Collidable& entity, const Vector3& position) const;

    // World collision detection (for PhysicsSystem)
    bool CheckCollisionWithWorld(const Collidable& entity, const Vector3& position);

    // Ray casting
    bool CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const;
    bool CastRayWorldOnly(const Vector3& origin, const Vector3& direction, float maxDistance,
                         Vector3& hitPoint, Vector3& hitNormal) const;

    // Collision detection helpers (public access for physics system)
    bool CheckAABBIntersectsTriangle(const AABB& aabb, const std::vector<Vector3>& triangle) const {
        return AABBIntersectsTriangle(aabb, triangle);
    }
    float GetPenetrationDepth(const AABB& aabb, const std::vector<Vector3>& triangle, const Vector3& normal) const {
        return CalculatePenetrationDepth(aabb, triangle, normal);
    }

    // Debug visualization
    void SetDebugBoundsVisible(bool visible) { debugBoundsVisible_ = visible; }
    bool IsDebugBoundsVisible() const { return debugBoundsVisible_; }
    void ToggleDebugBounds() { debugBoundsVisible_ = !debugBoundsVisible_; }

    // Collision event callbacks
    void OnCollisionEnter(const CollisionEvent& event);
    void OnCollisionStay(const CollisionEvent& event);
    void RenderDebugBounds();
    void OnCollisionExit(const CollisionEvent& event);

private:
    const World* world_;  // New World structure instead of old BSPTree
    bool debugBoundsVisible_;

    // Cached collision data for optimization
    std::vector<Entity*> collidableEntities_;

    // Helper functions for collision detection
    bool PointInAABB(const Vector3& point, const AABB& aabb) const;
    bool AABBIntersectsTriangle(const AABB& aabb, const std::vector<Vector3>& triangle) const;
    bool EdgeIntersectsAABB(const Vector3& edgeStart, const Vector3& edgeEnd, const AABB& aabb) const;
    float CalculatePenetrationDepth(const AABB& aabb, const std::vector<Vector3>& triangle, const Vector3& normal) const;
    std::unordered_map<Entity*, std::vector<Entity*>> collisionPairs_;

    // Internal collision detection methods
    bool AABBIntersect(const AABB& a, const AABB& b) const;
    Vector3 GetAABBNormal(const AABB& a, const AABB& b) const;

    // BSP collision methods
    bool CheckBSPCollision(const Vector3& position, const Vector3& size) const;
    CollisionResponse ResolveBSPCollision(const Vector3& position, const Vector3& size) const;

    // Entity collision methods
    void UpdateCollidableEntities();
    void CheckEntityCollisions();
    void ResolveEntityCollision(Entity* entityA, Entity* entityB);

    // Utility methods
    Vector3 ClampToAABB(const Vector3& point, const AABB& aabb) const;
    float GetAABBPenetration(const AABB& a, const AABB& b, Vector3& normal) const;

    // Broad phase optimization
    void BuildSpatialGrid();
    std::vector<Entity*> QuerySpatialGrid(const AABB& bounds) const;
};
