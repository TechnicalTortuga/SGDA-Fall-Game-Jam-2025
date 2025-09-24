#pragma once

#include "../Component.h"
#include <string>
#include <unordered_map>
#include <any>
#include <vector>

/*
GameObject - Pure data component for dynamic entities with behaviors

GameObjects represent dynamic entities in the world that have behaviors,
interactions, and can affect gameplay. This distinguishes them from static
world geometry (brushes, static meshes) which are handled by WorldGeometry.

GameObjects include:
- Lights (point, spot, directional)
- Enemies/AI entities
- Triggers and interactive elements
- Spawn points and waypoints
- Game mode objects (capture points, etc.)

This is pure data - all logic belongs in systems that operate on GameObject components.
*/

enum class GameObjectType {
    LIGHT_POINT,
    LIGHT_SPOT,
    LIGHT_DIRECTIONAL,
    AUDIO_SOURCE,
    ENEMY,
    TRIGGER,
    SPAWN_POINT,
    WAYPOINT,
    PAINTABLE_SURFACE,
    GAME_AREA,
    GAME_MODE,
    STATIC_PROP,
    UNKNOWN
};

struct GameObject : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "GameObject"; }

    // Core game object data (pure data only - NO methods or complex state)
    GameObjectType type = GameObjectType::UNKNOWN;
    std::string name = "";
    std::string className = "";

    // Properties (key-value pairs for map-defined properties)
    std::unordered_map<std::string, std::any> properties;

    // Tags for grouping and querying
    std::vector<std::string> tags;

    // Entity relationship management
    uint64_t parentEntityId = 0; // 0 = no parent

    // State management
    bool enabled = true;

    // System integration (reference IDs for related systems)
    uint64_t behaviorSystemId = 0;  // For AI/behavior systems
    uint64_t interactionSystemId = 0; // For trigger/interaction systems
};
