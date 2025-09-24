#pragma once

#include "../Component.h"
#include <vector>
#include <string>

/*
LODComponent - Level of Detail component for ECS

Manages multiple mesh representations at different detail levels.
Automatically switches between LODs based on distance from camera
to optimize performance while maintaining visual quality.
*/

struct LODComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "LODComponent"; }

    // LOD level structure
    struct LODLevel {
        uint64_t meshEntityId = 0;     // Entity ID of the mesh for this LOD level
        float distanceThreshold = 0.0f; // Distance at which to switch to this LOD
        std::string levelName = "";    // For debugging ("HIGH", "MEDIUM", "LOW")
        bool isActive = true;          // Whether this LOD level is available
    };

    // LOD levels (ordered by distance, closest to farthest)
    std::vector<LODLevel> lodLevels;

    // Current LOD state
    int currentLODIndex = 0;           // Index of currently active LOD level
    float currentDistance = 0.0f;      // Current distance from camera (for debugging)
    bool lodEnabled = true;            // Whether LOD switching is enabled
    float hysteresis = 2.0f;           // Distance buffer to prevent rapid switching

    // LOD system integration
    uint64_t lodSystemId = 0;          // Reference to LODSystem

    // Performance tracking
    int switchCount = 0;               // Number of LOD switches (for debugging)

    // Simple state flags
    bool isActive = true;
    bool needsUpdate = true;           // Force LOD recalculation next frame
};
