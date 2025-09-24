#pragma once

#include "../Component.h"

enum class SpawnPointType {
    PLAYER,
    AI,
    ANY  // Can be used for either
};

/*
SpawnPointComponent - Pure data component for spawn point locations

Contains spawn point configuration and state data. Spawn logic and
player/AI spawning is handled by dedicated spawn management systems
that reference this component by entity ID.
*/

struct SpawnPointComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "SpawnPointComponent"; }

    // Core spawn point data (pure data only - NO methods or complex state)
    SpawnPointType type = SpawnPointType::PLAYER;

    // Configuration
    int team = 0;           // Team assignment (0 = any/no team)
    int priority = 1;       // Priority (higher values are preferred)
    float cooldownTime = 5.0f; // Cooldown between spawns

    // State
    bool enabled = true;
    bool occupied = false;
    float lastSpawnTime = 0.0f;

    // Statistics
    int spawnCount = 0;

    // Relationships
    uint64_t associatedAreaId = 0; // Associated game area

    // System integration (reference IDs for related systems)
    uint64_t spawnSystemId = 0;    // For spawn management
    uint64_t teamSystemId = 0;     // For team management
};
