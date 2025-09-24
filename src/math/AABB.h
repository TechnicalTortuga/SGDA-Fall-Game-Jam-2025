#pragma once

#include "raylib.h"
#include <algorithm>
#include <cfloat>

// Unified Axis-Aligned Bounding Box used across BSP, physics, and ECS
struct AABB {
    Vector3 min;
    Vector3 max;

    // Default constructs a zero-sized box at origin to be safe for entity components
    AABB() : min{0,0,0}, max{0,0,0} {}
    AABB(const Vector3& minPos, const Vector3& maxPos) : min(minPos), max(maxPos) {}

    // Factory for an empty box suitable for accumulating bounds
    static AABB Infinite() {
        AABB b;
        b.min = { FLT_MAX,  FLT_MAX,  FLT_MAX };
        b.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX };
        return b;
    }

    // Expand to include a point
    void Encapsulate(const Vector3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    // Expand to include another box
    void Encapsulate(const AABB& other) {
        Encapsulate(other.min);
        Encapsulate(other.max);
    }

    // Expand by vector delta (symmetric)
    void Expand(const Vector3& delta) {
        min.x -= fabsf(delta.x);
        min.y -= fabsf(delta.y);
        min.z -= fabsf(delta.z);
        max.x += fabsf(delta.x);
        max.y += fabsf(delta.y);
        max.z += fabsf(delta.z);
    }

    bool Intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    bool Contains(const Vector3& point) const {
        return (point.x >= min.x && point.x <= max.x) &&
               (point.y >= min.y && point.y <= max.y) &&
               (point.z >= min.z && point.z <= max.z);
    }

    Vector3 GetCenter() const {
        return Vector3{ (min.x + max.x) * 0.5f,
                        (min.y + max.y) * 0.5f,
                        (min.z + max.z) * 0.5f };
    }

    Vector3 GetSize() const {
        return Vector3{ max.x - min.x,
                        max.y - min.y,
                        max.z - min.z };
    }
};
