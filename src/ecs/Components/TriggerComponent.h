#pragma once

#include "../Component.h"
#include "raylib.h"
#include <vector>

enum class TriggerType {
    BOX,        // Axis-aligned bounding box
    SPHERE,     // Spherical trigger volume
    CYLINDER,   // Cylindrical trigger volume
    ONCE,       // Triggers only once
    MULTIPLE    // Can trigger multiple times
};

/*
TriggerComponent - Pure data component for interactive trigger volumes

Contains trigger volume geometry and behavior data. Trigger logic and
entity interactions are handled by dedicated trigger systems that reference
this component by entity ID.
*/

struct TriggerComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "TriggerComponent"; }

    // Core trigger data (pure data only - NO methods or complex state)
    TriggerType type = TriggerType::BOX;

    // Shape properties
    Vector3 size = {1.0f, 1.0f, 1.0f};  // For box triggers
    float radius = 1.0f;                 // For sphere/cylinder triggers
    float height = 1.0f;                 // For cylinder triggers

    // Trigger behavior
    bool enabled = true;
    bool triggered = false;
    int maxActivations = -1;  // -1 = unlimited
    int activationCount = 0;

    // Target entities (entities this trigger affects)
    std::vector<uint64_t> targetEntities;

    // System integration (reference IDs for related systems)
    uint64_t triggerSystemId = 0;   // For trigger logic and collision detection
    uint64_t eventSystemId = 0;     // For event dispatching
};
