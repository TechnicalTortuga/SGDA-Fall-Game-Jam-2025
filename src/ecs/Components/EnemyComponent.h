#pragma once

#include "../Component.h"
#include <vector>

enum class EnemyType {
    BASIC,      // Simple enemy
    FAST,       // Fast but weak
    TANK,       // Slow but strong
    SNIPER,     // Long range
    BOSS        // Special boss enemy
};

enum class EnemyState {
    IDLE,
    PATROLLING,
    CHASING,
    ATTACKING,
    RETREATING,
    DEAD
};

/*
EnemyComponent - Pure data component for enemy AI entities

Contains all enemy configuration, state, and behavior data. AI logic,
combat calculations, and movement are handled by dedicated AI systems
that reference this component by entity ID.
*/

struct EnemyComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "EnemyComponent"; }

    // Core enemy data (pure data only - NO methods or complex state)
    EnemyType type = EnemyType::BASIC;
    EnemyState state = EnemyState::IDLE;

    // Combat stats
    float health = 100.0f;
    float maxHealth = 100.0f;
    float damage = 10.0f;
    float attackRange = 5.0f;
    float detectionRange = 20.0f;

    // Movement
    float moveSpeed = 5.0f;
    float turnSpeed = 180.0f;  // Degrees per second

    // AI behavior
    int team = 0;
    uint64_t targetEntityId = 0;
    std::vector<uint64_t> waypointIds;

    // Behavior flags
    bool canChase = true;
    bool canAttack = true;
    bool canRetreat = true;

    // Respawn system
    bool respawnEnabled = true;
    float respawnTime = 10.0f;
    float deathTime = 0.0f;

    // System integration (reference IDs for related systems)
    uint64_t aiSystemId = 0;        // For AI decision making
    uint64_t combatSystemId = 0;    // For combat calculations
    uint64_t navigationSystemId = 0; // For pathfinding
};
