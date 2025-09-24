# PaintSplash Map Format Extensions (v2.1)

## Overview

The PaintSplash map format is a binary format designed for fast loading and efficient storage, built around our entity-component system. It's optimized for paint-based gameplay with support for dynamic surface painting and team-based mechanics.

### Key Features

- **Entity-Component Based**: Direct mapping to our ECS architecture
- **Efficient Binary Format**: Optimized for fast loading and minimal memory footprint
- **Paint System**: Support for dynamic paint application and blending
- **Multiplayer Ready**: Built with networked gameplay in mind
- **Level Streaming**: Support for large, streamable levels

## Design Philosophy

1. **Entities First**: Everything in the level is an entity with properties
2. **Performance-Oriented**: Structure optimized for fast loading and rendering
3. **Extensible**: Easy to add new entity types and properties
4. **Human-Readable**: YAML-based for easier debugging and hand-editing
5. **Binary-Compatible**: Designed to be easily convertible to a binary format

## File Format Specification (v2.1)

### 1. File Structure

```yaml
# PaintSplash Map Format v2.1
version: 2.1
name: "My Level"

# Global settings
settings:
    # Environment
    gravity: 0.0, -980.0, 0.0  # Default gravity (cm/s²)
    sky: "skybox/cloudy_blue"   # Skybox texture
    fog:
        color: 100, 100, 100   # RGB (0-255)
        start: 1000.0          # Distance fog start
        end: 5000.0            # Distance fog end

# Materials library
materials:
    - id: 0
      name: "brick_wall"
      type: "pbr"
      albedo: "textures/walls/brick_albedo.png"
      normal: "textures/walls/brick_normal.png"
      roughness: 0.8
      metalness: 0.1
      tiling: [1.0, 1.0]  # U, V tiling

# World geometry
world:
    # Brushes (BSP-style geometry)
    brushes:
        - id: 100
          faces:
              - vertices: [x,y,z, nx,ny,nz, u,v, r,g,b,a]  # Per-vertex data
                material: 0      # Material ID
                smoothing_group: 1
          physics: true         # Whether to generate collision

    # Static meshes (imported models)
    static_meshes:
        - id: 200
          file: "models/props/chair.obj"
          materials: [0, 0, 0]  # Material indices per submesh

# Entity definitions
entities:
    # Example: Point Light
    - id: 1000
      class: "light_point"
      name: "main_light"
      transform:
          position: [0.0, 500.0, 0.0]
          rotation: [0.0, 0.0, 0.0, 1.0]  # Quaternion (x,y,z,w)
          scale: [1.0, 1.0, 1.0]
      properties:
          color: [255, 240, 200]  # RGB (0-255)
          intensity: 5000.0       # Lumen equivalent
          radius: 1000.0          # Light influence radius
          cast_shadows: true
          shadow_bias: 0.005

    # Example: Spot Light
    - id: 1001
      class: "light_spot"
      name: "spotlight_01"
      transform: ...
      properties:
          color: [255, 255, 255]
          intensity: 10000.0
          range: 2000.0
          inner_angle: 30.0  # Degrees
          outer_angle: 45.0  # Degrees
          cast_shadows: true

    # Example: Directional Light (Sun/Moon)
    - id: 1002
      class: "light_directional"
      name: "sun"
      transform:
          rotation: [0.2, 0.8, 0.0, 0.6]  # Orients the light direction
      properties:
          color: [255, 245, 235]  # Slightly warm white
          intensity: 1.0          # 0.0-1.0 scale
          cast_shadows: true
          shadow_map_size: 2048    # Shadow map resolution

    # Paintable Surface Component
    - id: 2000
      components:
        - type: "PaintableSurface"
          texture_size: [2048, 2048]  # Resolution of paint texture
          max_layers: 4               # Maximum paint layers before blending
          paint_absorption: 1.0       # 0.0 to 1.0 (how much paint sticks)
          paint_spread: 0.1           # How much paint spreads (0.0 to 1.0)
          cleanable: true             # Can be cleaned by water or repainted
          
          # Visual properties
          base_material: "materials/wall.mat"  # Base material before painting
          paint_emissive: 0.2         # How much paint glows (0.0 to 1.0)
          paint_shininess: 0.5        # How shiny the paint is (0.0 to 1.0)
          
          # Runtime state (not saved in map)
          current_paint: []           # Array of paint layers (team_id, coverage, position, etc.)

    # Game Area Component
    - id: 2100
      components:
        - type: "GameArea"
          name: "main_gallery"
          bounds: [min_x, min_y, min_z, max_x, max_y, max_z]
          area_type: "PAINT_ZONE"     # SPAWN, PAINT_ZONE, CHOKEPOINT, HIGH_VALUE
          team_control: 0             # 0 = neutral, 1 = team1, 2 = team2
          
          # Scoring
          point_value: 100            # Points per second when controlled
          control_threshold: 0.7      # Paint coverage needed to control
          
          # Connections to other areas (for AI pathfinding)
          connected_areas: [2101, 2102]  # IDs of connected areas

    # Player and AI Spawns
    - id: 3000
      class: "player_start"
      name: "team1_spawn_01"
      transform:
          position: [0.0, 100.0, 0.0]
          rotation: [0.0, 0.0, 0.0, 1.0]
      properties:
          team: 1                    # Team number (0 = any/FFA)
          priority: 1                # Higher priority spawns are used first
          enabled: true              # Can be toggled by game logic
          cooldown: 5.0              # Seconds before reuse (for spawn protection)
          for_ai: false              # Whether this is an AI spawn point

    # AI Navigation and Behavior
    - id: 3100
      class: "ai_waypoint"
      name: "wp_paint_high_value_01"
      transform:
          position: [10.0, 0.0, 10.0]
      properties:
          type: "painting"           # painting, combat, patrol, cover, ambush, flank
          radius: 100.0              # Area of influence
          team: 0                    # 0 = all teams
          next_waypoints: [3101, 3102] # Connected waypoints
          tags: ["high_value", "exposed"]
          
          # Painting behavior
          target_area: "main_gallery" # Associated strategic area
          paint_priority: 0.8        # 0.0 to 1.0 importance for painting
          defend_priority: 0.5       # 0.0 to 1.0 importance for defending
          
          # Combat behavior
          preferred_range: [5.0, 15.0] # Min/max preferred combat range
          use_cover: true            # Will seek cover when possible
          can_flank: true            # Will attempt to flank enemies

    # Game Mode Component
    - id: 3200
      components:
        - type: "GameMode"
          mode: "TERRITORY_CONTROL"   # Only mode for now
          time_limit: 300             # 5 minutes per round
          score_limit: 1000           # Points needed to win
          
          # Team Configuration
          teams:
            - id: 1
              name: "Red Team"
              color: [255, 50, 50, 255]  # RGBA
              spawn_area: 2101          # ID of spawn area
              paint_color: [255, 0, 0, 200]  # RGBA
              
            - id: 2
              name: "Blue Team"
              color: [50, 50, 255, 255]  # RGBA
              spawn_area: 2102          # ID of spawn area
              paint_color: [0, 100, 255, 200]  # RGBA
          
          # Paint Settings
          max_paint_layers: 4         # Layers before blending
          paint_spread_amount: 0.1     # How much paint spreads (0.0 to 1.0)
          
          # Respawn Settings
          respawn_delay: 3.0          # Base respawn time
          spawn_protection: 2.0       # Seconds of invulnerability

    # Example: Static Prop
    - id: 4000
      class: "static_prop"
      name: "chair_01"
      transform: ...
      properties:
          model: "models/furniture/chair.obj"
          material_overrides: [1, 1, 1]  # Optional material overrides
          cast_shadow: true
          receive_shadow: true

# Navigation Data
navigation:
    navmesh:
        cell_size: 50.0          # Size of navmesh cells
        cell_height: 20.0        # Height of each cell
        walkable_slope: 45.0     # Maximum walkable slope (degrees)
        walkable_height: 200.0   # Minimum height for walkable areas
        walkable_radius: 25.0    # Agent radius for pathfinding

    # Navigation and Movement
    nav_volumes:
        - id: 1
          bounds: [min_x, min_y, min_z, max_x, max_y, max_z]
          type: "walkable"  # walkable, obstacle, paint_only, no_paint, ladder, jump_pad
          properties:
              team: 0                # 0 = all teams, or specific team ID
              paintable: true        # Can this area be painted?
              paint_effect: "normal" # normal, slow, speed_boost, bouncy, slippery
              movement_modifier: 1.0  # 1.0 = normal speed
              
    # Paint Paths (for AI pathfinding through paint)
    paint_paths:
        - id: 1
          type: "team1"  # team1, team2, or neutral
          waypoints: [[x1,y1,z1], [x2,y2,z2], ...]
          width: 1.0     # Path width
          decay_time: 30.0 # Time before path fades (0 = permanent)
          properties:
              speed_boost: 1.2       # Movement speed multiplier
              health_regen: 0.0       # Health regen per second
              ammo_regen: 0.0         # Ammo regen per second

# Editor Metadata
editor:
    # Camera bookmarks
    bookmarks:
        - name: "Main Area"
          position: [0.0, 200.0, 500.0]
          target: [0.0, 0.0, 0.0]
    
    # Layer system for organization
    layers:
        - name: "Geometry"
          visible: true
          objects: [100, 200, 300]  # Entity/brush IDs
        - name: "Lights"
          visible: true
          objects: [1000, 1001, 1002]

# Level metadata
metadata:
    author: "Level Designer"
    description: "Main game level with two rooms and a corridor"
    created: "2024-01-15T10:00:00Z"
    modified: "2024-01-20T15:30:00Z"
    default_lighting: true  # Whether to use default lighting if no lights are present
```

## Entity Reference

### Light Types

#### 1. Point Light
Omnidirectional light source that emits light in all directions from a single point.

```yaml
- id: 1000
  class: "light_point"
  name: "point_light_01"
  transform:
      position: [x, y, z]
  properties:
      color: [255, 255, 255]  # RGB (0-255)
      intensity: 1000.0       # Light intensity
      radius: 1000.0          # Maximum distance light affects
      cast_shadows: true      # Whether to cast shadows
      shadow_bias: 0.005      # Shadow mapping bias
      shadow_resolution: 1024 # Shadow map resolution
```

#### 2. Spot Light
Directional light that emits light in a cone shape.

```yaml
- id: 1001
  class: "light_spot"
  name: "spot_light_01"
  transform:
      position: [x, y, z]
      rotation: [x, y, z, w]  # Direction is forward vector of rotation
  properties:
      color: [255, 255, 255]
      intensity: 2000.0
      range: 2000.0
      inner_angle: 30.0  # Inner cone angle (degrees)
      outer_angle: 45.0  # Outer cone angle (degrees)
      cast_shadows: true
      shadow_bias: 0.005
```

#### 3. Directional Light
Infinite directional light source (like the sun).

```yaml
- id: 1002
  class: "light_directional"
  name: "sun"
  transform:
      rotation: [x, y, z, w]  # Direction is forward vector
  properties:
      color: [255, 245, 235]  # Slightly warm white
      intensity: 1.0          # 0.0-1.0 scale
      cast_shadows: true
      shadow_map_size: 2048    # Shadow map resolution
      shadow_cascade_count: 3  # Number of cascades for CSM
      shadow_distance: 10000.0 # Maximum shadow distance
```

### Common Properties

All entities share these common properties:

- **transform**: Position, rotation, and scale
- **name**: Unique identifier for the entity
- **tags**: Array of string tags for grouping/querying
- **visible**: Whether the entity is visible
- **locked**: Whether the entity can be modified in the editor

## Binary Format Specification

### File Structure
```
[Header]
- Magic: 'PSMP' (4 bytes) - PaintSplash Map Format
- Version: (uint16) - Format version
- Entity Count: (uint32) - Number of entities
- String Table Offset: (uint32) - Offset to string table
- Component Type Count: (uint16) - Number of component types

[Entity Section]
- For each entity:
  - ID: (uint32) - Entity ID
  - Component Count: (uint16) - Number of components
  - Components: (variable) - Component data

[Component Data]
- Component Type: (uint16) - Index into component type table
- Data Size: (uint32) - Size of component data
- Data: (bytes) - Serialized component data

[String Table]
- Count: (uint32) - Number of strings
- For each string:
  - Length: (uint16)
  - String: (bytes) - UTF-8 encoded string

[Component Type Table]
- Count: (uint16) - Number of component types
- For each type:
  - Name: (string) - Hashed component name
  - Version: (uint16) - Component data version
  - Size: (uint32) - Size of component data
```

### Component Serialization
Each component must implement:
- `size_t GetSerializedSize()` - Returns size needed for serialization
- `void Serialize(uint8_t* buffer)` - Writes component to buffer
- `void Deserialize(const uint8_t* buffer)` - Reads component from buffer

### Example Component (PaintableSurface):
```cpp
struct PaintableSurface {
    uint32_t texture_size[2];
    uint8_t max_layers;
    float paint_absorption;
    float paint_spread;
    bool cleanable;
    // ... other fields ...

    size_t GetSerializedSize() const {
        return sizeof(texture_size) + sizeof(max_layers) + 
               sizeof(paint_absorption) + sizeof(paint_spread) + 
               sizeof(cleanable);
    }

    void Serialize(uint8_t* buffer) const {
        memcpy(buffer, texture_size, sizeof(texture_size));
        buffer += sizeof(texture_size);
        *buffer++ = max_layers;
        memcpy(buffer, &paint_absorption, sizeof(paint_absorption));
        buffer += sizeof(paint_absorption);
        // ... serialize remaining fields ...
    }
};
```

## Multiplayer and AI Considerations

### Networked Entities
- All entities that can affect gameplay must be network-replicated
- Client-side prediction for player movement and actions
- Server authoritative for game state
- Entity interpolation for smooth movement

### AI System
- Navigation meshes for pathfinding
- Behavior trees for AI decision making
- Sensory system (sight, sound, etc.)
- Team awareness and coordination
- Dynamic difficulty adjustment

### Game Mode: Territory Control

**Objective**:
- Teams compete to cover the map in their team's color
- Score points based on the percentage of map controlled
- Control key areas to earn bonus points

**Core Mechanics**:
- Paint surfaces by shooting them with your team's color
- Paint over enemy paint to claim territory
- Water and certain surfaces can clean paint
- Collect paint ammo pickups to refill

**Scoring**:
- 1 point per second per 1% of map controlled
- 2x points for controlling key areas
- Match ends when time runs out or score limit reached

### Implementation Notes

#### Loading Process
1. Parse YAML file
2. Load materials and textures
3. Create geometry (brushes and static meshes)
4. Instantiate entities
5. Set up lighting
6. Generate navigation data (or load precomputed)
7. Initialize game mode and team spawns
8. Set up network replication

#### Network Optimization
- Area of Interest management
- Entity priority system
- State compression
- Delta compression for updates
- Client-side prediction and reconciliation

#### AI Navigation
1. Generate navigation mesh from walkable surfaces
2. Create navigation graph with waypoints
3. Implement pathfinding (A* or similar)
4. Add local avoidance
5. Implement behavior trees for decision making

### Editor Integration

For the level editor, we'll need to implement:

1. **Entity Creation/Editing**
   - Place, rotate, and scale entities
   - Edit properties in an inspector
   - Multi-select and group operations
   - Team assignment and visualization
   - AI waypoint linking and visualization
   - Spawn point management
   - Game mode configuration
   - Network relevance visualization

2. **Lighting Tools**
   - Light placement and adjustment
   - Light baking preview
   - Light probe placement

3. **Navigation**
   - Navmesh generation
   - Navigation volume editing
   - AI waypoint placement

4. **Optimization**
   - Level of Detail (LOD) configuration
   - Occlusion culling setup
   - Distance culling configuration

## Version History

### v1.0 (Current)
- Initial binary format implementation
- ECS-based map structure
- Paint system with layer support
- Team-based gameplay
- Basic AI navigation

### Pre-1.0
- Prototype formats and design iterations

### v2.1
- Complete redesign to entity-component system
- Added comprehensive lighting system
- Improved material system with PBR support
- Added navigation data support
- Editor metadata and organization features

### v2.0
- Initial extended format with entities and materials
- Basic geometry and texture support
- Simple event system

### v1.0
- Original format with basic surfaces and colors

### 5. Material System Extensions

#### Material References
```
materials:
    material:
        id: 0
        name: "textures/walls/brick"
        properties:
            diffuse: "brick_diffuse.png"
            normal: "brick_normal.png"
            specular: "brick_specular.png"
            shininess: 0.5
            double_sided: false
```

### 6. Implementation Plan

#### Phase 1: UV Coordinates
1. Extend face format to include UVs
2. Update MapLoader to parse UV data
3. Modify Face struct to store parsed UVs
4. Remove automatic UV calculation

#### Phase 2: Entity System
1. Add entity parsing to MapLoader
2. Create EntityFactory for spawning map entities
3. Implement basic entity types (lights, props)

#### Phase 3: Event System
1. Add event parsing and storage
2. Create EventManager system
3. Implement trigger volumes and event dispatching

#### Phase 4: Mesh Instances
1. Add mesh instance parsing
2. Integrate with existing MeshComponent system
3. Support material overrides

#### Phase 5: Advanced Features
1. Brush-based entities
2. Visibility groups
3. Lighting baked data
4. Metadata and versioning

## Source Engine Comparison

Source Engine's VMF format includes:

- **Geometry**: Brushes with faces (vertices + UVs + materials)
- **Entities**: Point entities + brush entities
- **Materials**: VMT files with shader parameters
- **Lighting**: Lightmap UVs + baked lighting data
- **Visgroups**: Grouping for organization
- **Cameras**: Editor camera positions
- **Cordon bounds**: Compile boundaries

Our extended format should cover similar functionality while being optimized for our ECS architecture.

## Migration Strategy

1. **Keep backward compatibility** with existing maps
2. **Version field** to distinguish format versions
3. **Gradual rollout** - implement features incrementally
4. **Conversion tools** for upgrading old maps

## Parser Architecture Changes

### New Parser Structure
```
MapLoader::ParseExtendedMap()
├── ParseHeader()           // version, metadata
├── ParseGeometry()         // brushes, meshes
├── ParseEntities()         // entity instances
├── ParseEvents()           // event definitions
├── ParseGroups()           // visibility groups
└── ValidateMap()           // integrity checks
```

### Face Parsing Extension
```
ParseFace(const std::string& line) -> Face
├── ParseVerticesWithUVs()  // x,y,z,u,v for each vertex
├── ValidateWinding()       // ensure correct normal direction
├── CalculateTangents()     // for normal mapping
└── AssignMaterial()        // material ID lookup
```

This extended format will provide the foundation for complex, production-quality level design in PaintSplash.
