#pragma once

#include "../Component.h"
#include "raylib.h"

enum class LightType {
    POINT,
    SPOT,
    DIRECTIONAL
};

/*
LightComponent - Pure data component for lighting information

Contains all lighting data needed for rendering. Light calculations and
shadow mapping are handled by dedicated lighting systems that reference
this component by entity ID.
*/

struct LightComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "LightComponent"; }

    // Core light data (pure data only - NO methods or complex state)
    LightType type = LightType::POINT;

    // Common properties
    Color color = {255, 255, 255, 255};
    float intensity = 1000.0f;
    bool castShadows = true;
    bool enabled = true;

    // Point light specific
    float radius = 1000.0f;
    float shadowBias = 0.005f;
    int shadowResolution = 1024;

    // Spot light specific
    float range = 2000.0f;
    float innerAngle = 30.0f;  // Degrees
    float outerAngle = 45.0f;  // Degrees

    // Directional light specific
    int shadowMapSize = 2048;
    int shadowCascadeCount = 3;
    float shadowDistance = 10000.0f;

    // System integration (reference IDs for related systems)
    uint64_t lightingSystemId = 0;  // For lighting calculations
    uint64_t shadowSystemId = 0;    // For shadow mapping
};
