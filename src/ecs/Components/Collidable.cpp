#include "Collidable.h"

Collidable::Collidable()
    : bounds_(Vector3{-0.5f, -0.5f, -0.5f}, Vector3{0.5f, 0.5f, 0.5f})
    , collisionLayer_(LAYER_NONE)
    , collisionMask_(LAYER_ALL)
    , isStatic_(false)
    , isTrigger_(false)
{
}

Collidable::Collidable(const Vector3& size)
    : bounds_(Vector3{-size.x/2, -size.y/2, -size.z/2},
              Vector3{size.x/2, size.y/2, size.z/2})
    , collisionLayer_(LAYER_NONE)
    , collisionMask_(LAYER_ALL)
    , isStatic_(false)
    , isTrigger_(false)
{
}

Collidable::Collidable(const AABB& bounds)
    : bounds_(bounds)
    , collisionLayer_(LAYER_NONE)
    , collisionMask_(LAYER_ALL)
    , isStatic_(false)
    , isTrigger_(false)
{
}

void Collidable::SetSize(const Vector3& size) {
    Vector3 center = bounds_.GetCenter();
    bounds_.min.x = center.x - size.x / 2.0f;
    bounds_.min.y = center.y - size.y / 2.0f;
    bounds_.min.z = center.z - size.z / 2.0f;
    bounds_.max.x = center.x + size.x / 2.0f;
    bounds_.max.y = center.y + size.y / 2.0f;
    bounds_.max.z = center.z + size.z / 2.0f;
}

void Collidable::SetPosition(const Vector3& position) {
    Vector3 size = bounds_.GetSize();
    bounds_.min.x = position.x - size.x / 2.0f;
    bounds_.min.y = position.y - size.y / 2.0f;
    bounds_.min.z = position.z - size.z / 2.0f;
    bounds_.max.x = position.x + size.x / 2.0f;
    bounds_.max.y = position.y + size.y / 2.0f;
    bounds_.max.z = position.z + size.z / 2.0f;
}

void Collidable::UpdateBoundsFromPosition(const Vector3& position) {
    SetPosition(position);
}
