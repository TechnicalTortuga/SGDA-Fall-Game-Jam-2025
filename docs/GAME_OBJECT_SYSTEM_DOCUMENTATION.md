# PaintStrike Game Object System Documentation

*Comprehensive documentation for the Game Object infrastructure and map parsing system*

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Core Components](#core-components)
4. [Game Object Components](#game-object-components)
5. [Map Format Specification](#map-format-specification)
6. [EntityFactory System](#entityfactory-system)
7. [GameObjectSystem](#gameobjectsystem)
8. [Engine Integration](#engine-integration)
9. [Usage Examples](#usage-examples)
10. [Migration Notes](#migration-notes)

---

## 🎯 Overview

The PaintSplash Game Object System provides a complete infrastructure for creating, managing, and serializing dynamic entities in the game world. This system distinguishes between static world geometry (BSP brushes, static meshes) and dynamic Game Objects (lights, enemies, triggers, spawn points, etc.) that have behaviors and can affect gameplay.

### Key Features

- **Pure Data-Oriented Components**: All components follow ECS principles with public data only
- **YAML Map Format**: Human-readable format for development with binary format planned for production
- **EntityFactory**: Centralized entity creation from map definitions
- **GameObjectSystem**: Manages Game Object lifecycle and queries
- **Engine Integration**: Global access to factory and system instances
- **Type-Safe Component System**: Strongly typed Game Object types with associated behaviors

---

## 🏗️ Architecture

### System Layers

```
┌─────────────────┐
│   Engine        │  ← Global access to EntityFactory & GameObjectSystem
├─────────────────┤
│ WorldSystem     │  ← Loads maps, creates entities via EntityFactory
├─────────────────┤
│ EntityFactory   │  ← Creates entities from EntityDefinition structs
├─────────────────┤
│ GameObjectSystem│  ← Manages Game Object lifecycle & queries
├─────────────────┤
│ ECS Components  │  ← Pure data components (GameObject, Light, etc.)
├─────────────────┤
│ MapLoader       │  ← Parses YAML map files into EntityDefinition structs
└─────────────────┘
```

### Data Flow

1. **Map Loading**: YAML files → MapLoader → EntityDefinition structs
2. **Entity Creation**: EntityDefinition → EntityFactory → ECS Entities
3. **System Registration**: Entities → GameObjectSystem → Query system
4. **Runtime Management**: GameObjectSystem → Behavior updates → Entity queries

---

## 🔧 Core Components

### GameObject Component

The base component that identifies entities as Game Objects with behaviors.

```cpp
struct GameObject : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "GameObject"; }

    // Core game object data (pure data only)
    GameObjectType type = GameObjectType::UNKNOWN;
    std::string name = "";
    std::string className = "";

    // Properties (key-value pairs for map-defined properties)
    std::unordered_map<std::string, std::any> properties;
    std::vector<std::string> tags;

    // Entity relationship management
    uint64_t parentEntityId = 0; // 0 = no parent
    bool enabled = true;

    // System integration
    uint64_t behaviorSystemId = 0;
    uint64_t interactionSystemId = 0;
};
```

### EntityDefinition Struct

Used for parsing map files into a format suitable for entity creation.

```cpp
struct EntityDefinition {
    uint32_t id = 0;
    std::string className;
    std::string name;
    GameObjectType type = GameObjectType::UNKNOWN;

    // Transform data
    Vector3 position = {0, 0, 0};
    Vector3 scale = {1, 1, 1};
    Quaternion rotation = {0, 0, 0, 1};

    // Properties and component-specific data
    std::unordered_map<std::string, std::any> properties;
    struct { /* Light data */ } light;
    struct { /* Enemy data */ } enemy;
    struct { /* Trigger data */ } trigger;
    struct { /* Spawn point data */ } spawnPoint;
};
```

---

## 🎮 Game Object Components

All Game Object components follow the pure data struct pattern with public members only.

### LightComponent

Handles lighting information for point, spot, and directional lights.

```cpp
struct LightComponent : public Component {
    const char* GetTypeName() const override { return "LightComponent"; }

    // Core light data
    LightType type = LightType::POINT;
    Color color = {255, 255, 255, 255};
    float intensity = 1000.0f;
    bool castShadows = true;
    bool enabled = true;

    // Type-specific properties
    float radius = 1000.0f;        // Point lights
    float range = 2000.0f;         // Spot lights
    float innerAngle = 30.0f;      // Spot lights (degrees)
    float outerAngle = 45.0f;      // Spot lights (degrees)
    int shadowMapSize = 2048;      // Directional lights
    int shadowCascadeCount = 3;    // Directional lights
    float shadowDistance = 10000.0f; // Directional lights

    // System integration
    uint64_t lightingSystemId = 0;
    uint64_t shadowSystemId = 0;
};
```

### EnemyComponent

Contains all enemy AI and combat data.

```cpp
struct EnemyComponent : public Component {
    const char* GetTypeName() const override { return "EnemyComponent"; }

    // Core enemy data
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
    float turnSpeed = 180.0f;

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

    // System integration
    uint64_t aiSystemId = 0;
    uint64_t combatSystemId = 0;
    uint64_t navigationSystemId = 0;
};
```

### TriggerComponent

Manages interactive trigger volumes.

```cpp
struct TriggerComponent : public Component {
    const char* GetTypeName() const override { return "TriggerComponent"; }

    // Core trigger data
    TriggerType type = TriggerType::BOX;
    Vector3 size = {1.0f, 1.0f, 1.0f};
    float radius = 1.0f;
    float height = 1.0f;

    // Trigger behavior
    bool enabled = true;
    bool triggered = false;
    int maxActivations = -1;  // -1 = unlimited
    int activationCount = 0;

    // Target entities
    std::vector<uint64_t> targetEntities;

    // System integration
    uint64_t triggerSystemId = 0;
    uint64_t eventSystemId = 0;
};
```

### SpawnPointComponent

Handles player and AI spawn locations.

```cpp
struct SpawnPointComponent : public Component {
    const char* GetTypeName() const override { return "SpawnPointComponent"; }

    // Core spawn point data
    SpawnPointType type = SpawnPointType::PLAYER;
    int team = 0;
    int priority = 1;
    float cooldownTime = 5.0f;

    // State
    bool enabled = true;
    bool occupied = false;
    float lastSpawnTime = 0.0f;

    // Statistics
    int spawnCount = 0;
    uint64_t associatedAreaId = 0;

    // System integration
    uint64_t spawnSystemId = 0;
    uint64_t teamSystemId = 0;
};
```

---

## 🗺️ Map Format Specification

PaintSplash uses a YAML-based map format for development, with binary format planned for production.

### Basic Structure

```yaml
# PaintSplash Map Format
version: 2.1
name: "My Game Level"

# Global settings
settings:
    gravity: 0.0, -980.0, 0.0
    sky: "skybox/cloudy_blue"
    fog:
        color: 135, 206, 235
        start: 1000.0
        end: 5000.0

# Materials library (placeholder)
materials:
    - id: 0
      name: "brick_wall"
      type: "pbr"
      albedo: "textures/walls/brick_albedo.png"

# World geometry (static brushes/meshes)
world:
    brushes: []  # Static geometry - handled separately

# Game Objects - Dynamic entities
entities:
    # Example entities...
```

### Entity Definition Format

```yaml
entities:
    - id: 1000
      class: "light_point"
      name: "main_room_light"
      transform:
          position: [0.0, 7.0, 0.0]
          rotation: [0.0, 0.0, 0.0, 1.0]
          scale: [1.0, 1.0, 1.0]
      properties:
          color: [255, 240, 200]
          intensity: 5000.0
          radius: 1000.0
          cast_shadows: true
```

### Supported Entity Classes

| Class | Type | Description |
|-------|------|-------------|
| `light_point` | Light | Omnidirectional point light |
| `light_spot` | Light | Directional cone light |
| `light_directional` | Light | Infinite directional light (sun) |
| `player_start` | SpawnPoint | Player spawn location |
| `enemy` | Enemy | AI enemy entity |
| `trigger` | Trigger | Interactive trigger volume |
| `waypoint` | GameObject | AI navigation point |
| `static_prop` | GameObject | Static decorative object |

---

## 🏭 EntityFactory System

The EntityFactory is responsible for creating ECS entities from parsed EntityDefinition data.

### Key Methods

```cpp
class EntityFactory {
public:
    // Create single entity from definition
    Entity* CreateEntityFromDefinition(const EntityDefinition& definition);

    // Create multiple entities
    std::vector<Entity*> CreateEntitiesFromDefinitions(const std::vector<EntityDefinition>& definitions);

    // Register custom entity creators
    void RegisterEntityCreator(GameObjectType type, std::function<Entity*(const EntityDefinition&)> creator);
};
```

### Usage Example

```cpp
// Get factory from engine
auto factory = engine->GetEntityFactory();

// Create entities from map data
auto entities = factory->CreateEntitiesFromDefinitions(mapData.entities);

// Register with GameObjectSystem
auto goSystem = engine->GetGameObjectSystem();
for (auto entity : entities) {
    if (entity->GetComponent<GameObject>()) {
        goSystem->RegisterGameObject(entity);
    }
}
```

### Entity Creation Process

1. **Determine Type**: Parse `class` field to determine GameObjectType
2. **Create Base Entity**: Call `engine->CreateEntity()`
3. **Add Components**: Add TransformComponent, GameObject component, and type-specific components
4. **Set Properties**: Copy data from EntityDefinition to component fields
5. **Setup Relationships**: Link to parent entities and related systems

---

## 🎮 GameObjectSystem

The GameObjectSystem manages the lifecycle and queries for all Game Objects.

### Key Features

- **Registration**: Tracks all active Game Objects
- **Queries**: Find objects by type, tag, or state
- **Updates**: Coordinate updates for different Game Object types
- **Events**: Handle Game Object lifecycle events

### Key Methods

```cpp
class GameObjectSystem : public System {
public:
    // Registration
    void RegisterGameObject(Entity* entity);
    void UnregisterGameObject(Entity* entity);

    // Queries
    std::vector<Entity*> GetGameObjectsByType(GameObjectType type) const;
    std::vector<Entity*> GetGameObjectsByTag(const std::string& tag) const;
    std::vector<Entity*> GetActiveGameObjects() const;

    // Specialized queries
    std::vector<Entity*> GetActiveLights() const;
    std::vector<Entity*> GetActiveEnemies() const;
    std::vector<Entity*> GetSpawnPoints(int team = -1) const;
    Entity* FindBestSpawnPoint(int team = 0) const;

    // Events
    void OnGameObjectEvent(Entity* entity, const std::string& eventType);
};
```

### Usage Examples

```cpp
// Get all active enemies
auto enemies = goSystem->GetActiveEnemies();

// Find spawn point for team 1
auto spawnPoint = goSystem->FindBestSpawnPoint(1);

// Get all lights for rendering
auto lights = goSystem->GetActiveLights();
```

---

## 🔗 Engine Integration

EntityFactory and GameObjectSystem are now globally accessible through the Engine.

### Engine Access Methods

```cpp
class Engine {
public:
    // Access to global systems
    EntityFactory* GetEntityFactory() const { return entityFactory_; }
    GameObjectSystem* GetGameObjectSystem() const { return gameObjectSystem_; }
};
```

### Initialization Order

1. **Engine Constructor**: Initialize pointers to nullptr
2. **Initialize()**: Call `InitializeEntityFactory()` and `InitializeGameObjectSystem()`
3. **Add Systems**: GameObjectSystem added to systems vector like other systems
4. **System Updates**: GameObjectSystem participates in normal update/render cycle

### Memory Management

- **EntityFactory**: Dynamically allocated in Engine, cleaned up in Shutdown()
- **GameObjectSystem**: Managed as a regular System (unique_ptr in systems vector)
- **Game Objects**: Created by EntityFactory, destroyed by WorldSystem

---

## 💡 Usage Examples

### Creating a Custom Game Object

```cpp
// 1. Define your component
struct CustomComponent : public Component {
    const char* GetTypeName() const override { return "CustomComponent"; }

    std::string customData;
    float customValue = 0.0f;
};

// 2. Register with EntityFactory
entityFactory->RegisterEntityCreator(GameObjectType::UNKNOWN, [](const EntityDefinition& def) {
    auto engine = def.engine; // Assuming we pass engine reference
    auto entity = engine->CreateEntity();

    // Add standard components
    auto transform = entity->AddComponent<TransformComponent>();
    transform->position = def.position;

    auto gameObj = entity->AddComponent<GameObject>();
    gameObj->type = GameObjectType::UNKNOWN;
    gameObj->name = def.name;

    // Add custom component
    auto custom = entity->AddComponent<CustomComponent>();
    // Set custom properties...

    return entity;
});
```

### Map Loading Workflow

```cpp
// 1. Load map file
MapLoader loader;
MapData mapData = loader.LoadMap("level1.map");

// 2. Create entities
auto factory = engine->GetEntityFactory();
auto entities = factory->CreateEntitiesFromDefinitions(mapData.entities);

// 3. Register with systems
auto goSystem = engine->GetGameObjectSystem();
for (auto entity : entities) {
    goSystem->RegisterGameObject(entity);
}

// 4. Query for specific types
auto lights = goSystem->GetGameObjectsByType(GameObjectType::LIGHT_POINT);
auto enemies = goSystem->GetGameObjectsByType(GameObjectType::ENEMY);
```

---

## 🔄 Migration Notes

### From Legacy System

- **Removed**: Old surface-based map parsing (`ParseFace`, `ParseTexture`)
- **Removed**: Legacy fallback loading (no more old format support)
- **Changed**: WorldSystem no longer owns EntityFactory/GameObjectSystem
- **Changed**: All components now follow pure data struct pattern
- **Added**: Global access to factory and system through Engine

### Breaking Changes

1. **Component Structure**: All components converted to structs with public data
2. **Factory Access**: Now accessed via `engine->GetEntityFactory()`
3. **System Access**: Now accessed via `engine->GetGameObjectSystem()`
4. **Map Format**: Only YAML format supported (legacy format removed)

### Benefits

- **Consistency**: All components follow same data-oriented pattern
- **Global Access**: Factory and system available throughout codebase
- **Clean Architecture**: Clear separation between static geometry and dynamic objects
- **Extensibility**: Easy to add new Game Object types
- **Performance**: Data-oriented design for cache-friendly access

---

## 🚀 Future Enhancements

### Planned Features

1. **Binary Map Format**: For production builds with fast loading
2. **Entity Prefabs**: Reusable entity templates
3. **Component Serialization**: Network-ready component data
4. **Advanced AI**: Behavior trees and navigation meshes
5. **Visual Editor**: In-engine map and entity editing

### Performance Optimizations

- **Archetype-based Storage**: For cache-friendly component access
- **Spatial Partitioning**: For efficient Game Object queries
- **LOD System**: Distance-based Game Object management
- **Instancing**: For repeated Game Object types

---

*This documentation covers the complete Game Object infrastructure implemented for PaintSplash. The system provides a solid foundation for dynamic entities while maintaining ECS principles and data-oriented design.*
