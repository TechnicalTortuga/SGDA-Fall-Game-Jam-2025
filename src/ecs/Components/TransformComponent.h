#pragma once

#include "../Component.h"
#include "raylib.h"
#include "raymath.h"
#include <memory>

/*
TransformComponent - Pure data transform component for ECS

This struct contains ONLY the essential transform data. All complex operations
(matrix calculations, interpolation, hierarchy management) are handled by
dedicated systems that reference this component by entity ID.

This is the purest form of data-oriented ECS design.
*/

// Forward declaration for utility function
struct TransformComponent;

struct TransformComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "TransformComponent"; }

    // Core transform data (pure data only - NO methods or complex state)
    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    Quaternion rotation = {0.0f, 0.0f, 0.0f, 1.0f};

    // Reference IDs for related systems (data-oriented decoupling)
    uint64_t interpolationSystemId = 0;  // For network interpolation
    uint64_t hierarchySystemId = 0;      // For parent-child relationships
    uint64_t physicsSystemId = 0;        // For physics integration

    // Simple state flags (pure data)
    bool isActive = true;
    bool needsMatrixUpdate = true;
};
