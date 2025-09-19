#pragma once

#include "../Component.h"
#include "raylib.h"
#include <cstdint>
#include <algorithm>

// Collision layer bitmasks for filtering collision detection
enum CollisionLayer : uint32_t {
    LAYER_NONE      = 0,
    LAYER_PLAYER    = 1 << 0,   // Player entities
    LAYER_ENEMY     = 1 << 1,   // Enemy entities
    LAYER_PROJECTILE = 1 << 2,  // Projectiles
    LAYER_WORLD     = 1 << 3,   // World geometry (walls, floors)
    LAYER_PICKUP    = 1 << 4,   // Pickups and power-ups
    LAYER_DEBRIS    = 1 << 5,   // Debris and effects
    LAYER_ALL       = 0xFFFFFFFF
};

// Axis-Aligned Bounding Box for collision detection
struct AABB {
    Vector3 min;    // Minimum corner of the AABB
    Vector3 max;    // Maximum corner of the AABB

    AABB() : min{0, 0, 0}, max{0, 0, 0} {}
    AABB(const Vector3& minPos, const Vector3& maxPos) : min(minPos), max(maxPos) {}

    // Check if this AABB intersects with another
    bool Intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    // Get the center point of the AABB
    Vector3 GetCenter() const {
        return Vector3{
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f
        };
    }

    // Get the size/extents of the AABB
    Vector3 GetSize() const {
        return Vector3{
            max.x - min.x,
            max.y - min.y,
            max.z - min.z
        };
    }

    // Expand the AABB to include a point
    void Expand(const Vector3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }
};

// Component that defines collision properties for entities
class Collidable : public Component {
public:
    Collidable();
    Collidable(const Vector3& size);
    Collidable(const AABB& bounds);

    // Getters
    const AABB& GetBounds() const { return bounds_; }
    uint32_t GetCollisionLayer() const { return collisionLayer_; }
    uint32_t GetCollisionMask() const { return collisionMask_; }
    bool IsStatic() const { return isStatic_; }
    bool IsTrigger() const { return isTrigger_; }

    // Setters
    void SetBounds(const AABB& bounds) { bounds_ = bounds; }
    void SetSize(const Vector3& size);
    void SetPosition(const Vector3& position);
    void SetCollisionLayer(uint32_t layer) { collisionLayer_ = layer; }
    void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }
    void SetStatic(bool isStatic) { isStatic_ = isStatic; }
    void SetTrigger(bool isTrigger) { isTrigger_ = isTrigger; }

    // Collision layer convenience methods
    void AddToLayer(CollisionLayer layer) { collisionLayer_ |= layer; }
    void RemoveFromLayer(CollisionLayer layer) { collisionLayer_ &= ~layer; }
    bool IsInLayer(CollisionLayer layer) const { return (collisionLayer_ & layer) != 0; }

    // Collision mask convenience methods
    void AddToMask(CollisionLayer layer) { collisionMask_ |= layer; }
    void RemoveFromMask(CollisionLayer layer) { collisionMask_ &= ~layer; }
    bool CanCollideWith(CollisionLayer layer) const { return (collisionMask_ & layer) != 0; }

    // Utility methods
    bool ShouldCollideWith(const Collidable& other) const {
        return (collisionMask_ & other.collisionLayer_) != 0 &&
               (other.collisionMask_ & collisionLayer_) != 0;
    }

    void UpdateBoundsFromPosition(const Vector3& position);

private:
    AABB bounds_;              // Axis-aligned bounding box
    uint32_t collisionLayer_;  // Which layer this entity belongs to
    uint32_t collisionMask_;   // Which layers this entity can collide with
    bool isStatic_;            // Whether this collider is static (doesn't move)
    bool isTrigger_;           // Whether this is a trigger (no physical response)
};
