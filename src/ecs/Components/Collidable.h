#pragma once

#include "../Component.h"
#include "raylib.h"
#include <cstdint>
#include <algorithm>
#include "../../math/AABB.h"

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

// Using shared AABB from math/AABB.h

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
