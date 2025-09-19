Updated Gameplan.md

Note on Networking (TBD)
- The networking architecture is intentionally unspecified for now and will be defined by the networking lead.
- Phase 4 in this document should be treated as a placeholder; the actual API (transport, topology, replication model) will be integrated with the current engine once the design is chosen.
// The following is the structure of the project focused from the game perspective.
// The editor is in the editor directory.
src/
├── CMakeLists.txt                  # Build config: Links raylib, raygui, ENet
├── main.cpp                        # Entry: Creates Game instance, handles init/shutdown
├── Game.h                          # Game class: Runs game loop only
├── Game.cpp                        # Game implementation: Orchestrates game loop
├── core/
│   ├── Engine.h                    # Manages ECS registry, state manager, and systems
│   ├── Engine.cpp
│   ├── StateManager.h              # FSM: Switches between GameState classes
│   ├── StateManager.cpp
│   └── states/
│       ├── GameState.h             # Abstract base for state-specific logic
│       ├── MenuState.h             # Main menu logic
│       ├── MenuState.cpp
│       ├── LobbyState.h            # Lobby logic (team selection, readiness)
│       ├── LobbyState.cpp
│       ├── LoadingState.h          # Map loading and asset initialization
│       ├── LoadingState.cpp
│       ├── GamePlayState.h         # Core gameplay logic
│       └── GamePlayState.cpp
├── ecs/
│   ├── Entity.h                    # IDs, archetype management
│   ├── Component.h                 # Base for components (templates)
│   ├── System.h                    # Base for systems (update/query)
│   ├── Components/                 # Data structs
│   │   ├── Position.h              # Vector3 pos
│   │   ├── Velocity.h              # Vector3 vel
│   │   ├── Sprite.h                # Texture, scale, decalOverlay, lightInfluence
│   │   ├── Collidable.h            # AABB or mesh bounds, collision type, response flags
│   │   ├── Networkable.h           # PeerId, deltas
│   │   ├── Weapon.h                # Type, ammo, fireRate, paintColor
│   │   ├── Projectile.h            # Direction, speed, paintColor, lifetime
│   │   ├── Team.h                  # teamID, teamColor, score
│   │   ├── Light.h                 # Position, color, intensity, radius, type
│   │   ├── AudioEmitter.h          # soundPath, volume, is3D
│   │   ├── Surface.h               # Mesh data, material, surface type (wall, floor, etc.)
│   │   └── Player.h                # Camera position, rotation, input bindings
│   └── Systems/                    # Logic processors
│       ├── InputSystem.h           # Handles input polling and mapping
│       ├── PlayerSystem.h          # Sets up and manages player entity and camera
│       ├── WorldSystem.h           # Manages map entities (surfaces, lights)
│       ├── MovementSystem.h        # (Removed) PhysicsSystem now owns movement
│       ├── CollisionSystem.h       # AABB/mesh collision detection and response
│       ├── PhysicsSystem.h         # Movement constraints and collision physics
│       ├── RenderSystem.h          # Collects surfaces, sprites, lights
│       ├── SyncSystem.h            # Delta compression for P2P
│       ├── WeaponSystem.h          # Firing, event posting
│       ├── DecalSystem.h           # Apply splatters via events
│       ├── TeamSystem.h            # Assign/check teams, scoring
│       ├── LightingSystem.h        # Dynamic light culling and application
│       └── AudioSystem.h           # Play sounds on events
├── world/
│   ├── BSPTree.h                   # Nodes, build/traverse; surface and light lists
│   ├── BSPTree.cpp
├── rendering/
│   ├── Renderer.h / Renderer.cpp   # Camera and draw dispatch
│   ├── WorldRenderer.h / .cpp      # Static world (BSP surfaces) rendering
│   ├── TextureManager.h / .cpp     # Asset caching and path fallbacks
│   ├── Skybox.h / Skybox.cpp       # Background environment rendering
│   └── shaders/skybox/             # skybox.vs, skybox.fs
├── physics/
│   ├── Physics.h                   # Movement constraints and collision response
│   └── Physics.cpp
├── weapons/
│   ├── WeaponManager.h             # Coordinates gun logic
│   └── WeaponManager.cpp
├── decals/
│   ├── Decal.h                     # Pos, texture, size, UV
│   ├── Decal.cpp
│   └── DecalRenderer.h             # Blends paint on surfaces
├── networking/
│   ├── NetworkManager.h            # UDP P2P, peers, auth/sync
│   ├── NetworkManager.cpp
│   └── Packet.h                    # Deltas, readiness, team assigns
├── input/
│   ├── Input.h                     # Raylib key/mouse polling
│   └── Input.cpp
├── ui/
│   ├── UIManager.h                 # Raygui: Menus, HUD
│   ├── UIManager.cpp
│   ├── MenuState.h                 # Moved to core/states/
│   └── PauseSystem.h               # Overlay: Toggle, draw pause menu
├── loading/
│   ├── LoadingManager.h            # Progress tracking, asset loading
│   └── LoadingManager.cpp
├── audio/
│   ├── AudioManager.h              # Queues, spatialization
│   └── AudioManager.cpp
├── teams/
│   ├── TeamManager.h               # Balance assigns, host auth
│   └── TeamManager.cpp
├── lighting/
│   ├── LightManager.h              # Dynamic light culling and application
│   └── LightManager.cpp
├── events/
│   ├── Event.h                     # Struct: type, data
│   ├── EventManager.h              # Post/subscribe/dispatch
│   └── EventManager.cpp
└── utils/
    ├── Logger.h                    # Debug logging

// High-level overview of the project, to be updated as we go.

[Main.cpp] --> [Game] --> [Engine] --> [StateManager] --> [GameState (MENU/LOBBY/LOADING/GAME)]
                     |                    |
                     |                    +--> [MenuState] --> [UIManager] (Main Menu UI)
                     |                    |
                     |                    +--> [LobbyState] --> [TeamSystem] + [NetworkManager] (Team Selection)
                     |                    |
                     |                    +--> [LoadingState] --> [LoadingManager] (Map/Asset Loading)
                     |                    |
                     |                    +--> [GamePlayState] --> [Systems (Update)]
                     |                                          |
                     |                                          +--> [PlayerSystem] --> [InputSystem] (Player Setup/Camera)
                     |                                          |
                     |                                          +--> [WorldSystem] --> [BSPSurfaces] (Map Entity Management)
                     |                                          |
                     |                                          +--> [WeaponSystem] --> [Event: Fire/Hit] --> [DecalSystem] --> [Renderer]
                     |                                          |                                           |
                     |                                          |                                           +--> [AudioSystem] (Splat Sound)
                     |                                          |
                     |                                          +--> [CollisionSystem] <--> [BSPSurfaces] + [Entities] (Hybrid Collision)
                     |                                          |
                     |                                          +--> [PhysicsSystem] <--> [CollisionSystem] (Movement Constraints)
                     |                                          |
                     |                                          +--> [SyncSystem] <--> [NetworkManager] (P2P Deltas/Auth)
                     |                                          |
                     |                                          +--> [TeamSystem] --> [LoadingManager] (Readiness Sync)
                     |                                          |
                     |                                          +--> [LightingSystem] --> [Renderer] (Dynamic Light Culling)
                     |
                     +--> [EventManager (Pub-Sub Hub)]
                     |
                     +--> [ECS Registry] --> [Systems (Update)]
                     |
                     +--> [UIManager] --> [PauseSystem (Overlay in GAME)]
                     |
                     +--> [Renderer] <--> [BSPSurfaces] (Raycast + Shaders)
                     |                      |
                     |                      +--> [Skybox] (Background Environment)


// PHASE 1 STATUS: ✅ COMPLETED (December 2024)
// - Spherical coordinate camera system with full 360° rotation
// - Enhanced input system with action-based remappable controls
// - 3D FPS movement (WASD + mouse look + vertical flight)
// - ECS architecture with Position, Velocity, Sprite components
// - 3D rendering pipeline with cube primitives
// - Speed modifiers (run/crouch) and proper movement vectors

// PHASE 2 OBJECTIVES: 🎯 World Building & BSP (80 hours total)
// - Implement BSP tree structure for surfaces (walls, floors, ceilings, pillars, complex meshes)
// - Create map loader for surfaces, lights, and entity spawns
// - Add hybrid collision detection (BSP surfaces + entity-to-entity)
// - Implement physics constraints for navigation
// - Enhance raycasting for hit detection (surfaces and entities)
// - Render floors/ceilings with skybox environment
// - Support dynamic lighting with distance-based culling

// PHASE 3 OBJECTIVES: 🎯 Weapons & Projectiles
// - Weapon system with paintball gun
// - Projectile physics and movement
// - Hit detection and paint splatter effects
// - Decal system for persistent paint marks
// - Multiple paint colors and team assignment

// PHASE 4 OBJECTIVES: 🎯 Multiplayer & Networking (TBD)
// - Networking transport and topology to be defined by networking lead
// - Hook into Engine update loop and EventManager for replication/events
// - Team management and lobby system
// - Score tracking and win conditions

@startuml
!theme plain

class Game {
  +Initialize() : bool
  +Run() : void
  +Shutdown() : void
  -Update(float dt) : void
  -Render() : void
}

class Entity {
  +uint64_t id
  +addComponent(Component*)
  +getComponent<T>()
}

abstract class Component {
  +Entity* owner
}

class Position { +Vector3 pos }
class Velocity { +Vector3 vel }
class Sprite { +string texturePath +Texture2D decalOverlay +float lightInfluence }
class Collidable { +Bounds bounds +CollisionType type +CollisionFlags flags }
class Weapon { +string type +Color paintColor }
class Projectile { +Vector3 direction +Color paintColor }
class Team { +int teamID +Color teamColor +int score }
class Light { +Vector3 position +Color color +float intensity +float radius }
class AudioEmitter { +string soundPath +float volume +bool is3D }
class Surface { +MeshData mesh +Material material +SurfaceType type }

abstract class System {
  +update(float dt)
  +queryEntities(Archetype)
}

class CollisionSystem : System { +checkCollisions() +resolveCollisions() }
class PhysicsSystem : System { +applyConstraints() +updatePositions() }
class RenderSystem : System { +collectSurfacesAndSprites() }
class WeaponSystem { +fireWeapon() +postEvent(Event) }
class DecalSystem { +applyDecal() +onEvent(Event) }
class TeamSystem { +assignTeam() +updateScores() }
class LightingSystem { +computeLighting() +cullLights() }
class AudioSystem { +playSound() +onEvent(Event) }

class Physics { +checkCollision() }
class BSPSurface { +MeshData mesh +Material material +SurfaceType type +AABB bounds }
class Renderer { +render(BSPSurface*) +projectSprite() +applyLighting(Light) }
class NetworkManager { +broadcastDelta() +syncAuth() }
class StateManager { +enum State {MENU, LOBBY, LOADING, GAME} +switchState() }
class UIManager { +drawMenu() +drawHUD() }
class PauseSystem { +bool isPaused +toggle() +drawPauseMenu() }
class LoadingManager { +float progress +loadWorld() +checkReadiness() }
class EventManager { +post(Event) +subscribe() +dispatch() }
class Event { <<struct>> +string type +void* data }

' Relationships
Game --> StateManager : "manages"
Game --> EventManager : "manages"
Game --> Engine : "manages"

Entity ||--o{ Component : "has"
Position <|-- Component
Velocity <|-- Component
Sprite <|-- Component
Collidable <|-- Component
Weapon <|-- Component
Projectile <|-- Component
Team <|-- Component
Light <|-- Component
AudioEmitter <|-- Component
Surface <|-- Component

System <|-- MovementSystem
System <|-- CollisionSystem
System <|-- PhysicsSystem
System <|-- RenderSystem
System <|-- WeaponSystem
System <|-- DecalSystem
System <|-- TeamSystem
System <|-- LightingSystem
System <|-- AudioSystem
System <|-- PauseSystem

Renderer --> BSPSurface : "traverses"
Renderer --> Skybox : "renders background"
RenderSystem --> Renderer : "uses"
WeaponSystem --> Physics : "uses"
CollisionSystem --> BSPSurface : "queries"
CollisionSystem --> Entity : "queries entities"
PhysicsSystem --> CollisionSystem : "uses"
DecalSystem --> Renderer : "applies"
LightingSystem --> Renderer : "applies"
AudioSystem --> Renderer : "spatializes"
TeamSystem --> NetworkManager : "syncs"
WeaponSystem --> EventManager : "posts"
DecalSystem --> EventManager : "subscribes"
AudioSystem --> EventManager : "subscribes"
LightingSystem --> EventManager : "subscribes"
PauseSystem --> EventManager : "subscribes"
LoadingManager --> StateManager : "triggers"
LoadingManager --> NetworkManager : "syncs"
StateManager --> UIManager : "uses"
StateManager --> Engine : "delegates"

@enduml

// The following is the structure of the project focused from the editor perspective.
// The game is still in the src directory.
project_root/
├── CMakeLists.txt                  # Root build: add_subdirectory(src); add_subdirectory(editor)
├── assets/                         # Shared: maps, textures, sounds
│   ├── maps/
│   ├── textures/
│   └── sounds/
├── src/                            # Game executable
│   ├── ... (full tree as above)
│   └── utils/
│       └── Logger.h                # Shared with editor
├── editor/                         # Separate BSP blockout tool
│   ├── CMakeLists.txt              # Links raylib, raygui; includes src headers
│   ├── main_editor.cpp             # Entry: Inits raylib window, Editor loop
│   ├── Editor.h                    # Orchestrator: Modes (build/preview/export)
│   └── Editor.cpp
│   ├── Brush.h                     # Primitives: Cube, cylinder, custom meshes
│   └── Brush.cpp
│   ├── CSG.h                       # Union/subtract ops for brush carving
│   └── CSG.cpp
│   ├── BSPCompiler.h               # Brush-to-BSP tree (extends shared BSPTree)
│   └── BSPCompiler.cpp
│   ├── UI.h                        # Raygui panels: Toolbar, properties, object list
│   └── UI.cpp
│   ├── Viewport.h                  # 3D/2D cameras, grid snapping
│   └── Viewport.cpp
│   ├── MeshBuilder.h               # Simplify complex mesh creation
│   └── MeshBuilder.cpp
│   └── MapExporter.h               # Save .map files (text/binary for MapLoader)
│   └── MapExporter.cpp
└── README.md                       # Build/run instructions for game/editor

Updated Phase 2 Detailed Implementation Plan: World Building & BSP
PaintSplash - World Building & Collision System
Date: December 2024Phase Duration: 1-2 weeks (~80 hours)Dependencies: Phase 1 (ECS, Input, Rendering)Team: 1-2 developers (game jam setup)

🎯 Phase 2 Overview
Goal: Transform the "gray void" into a navigable BSP-partitioned world with generic surfaces (walls, floors, ceilings, pillars, complex meshes), scalable lighting, and a map system optimized for performance and editor usability.
Key Deliverables:

BSP tree for surfaces (walls, floors, ceilings, pillars, custom meshes)
Map loading system with .map format supporting surfaces, lights, and entities
Hybrid collision detection (BSP surfaces + entity-to-entity)
Physics constraints for smooth navigation
Raycasting for accurate hit detection
Floor/ceiling rendering with skybox
Scalable lighting with distance-based culling
Editor-friendly map format and mesh simplification tools

Success Criteria:

Navigate BSP levels without clipping through surfaces or entities
Smooth collision response for all surface types and entities
Efficient rendering with BSP traversal and frustum culling
.map files support diverse surfaces, lights, and entity spawns
Scalable lighting (>4 lights) with culling for performance
Editor-ready format inspired by Source/Unreal
60+ FPS with 100+ surfaces and 10+ entities


📋 Detailed Task Breakdown
1. BSP Tree Implementation (14 hours)
Objective: Create a BSP structure for generic surfaces, optimized for rendering and collision.
Requirements:

BSPSurface Structure: Support for diverse surface types (walls, floors, ceilings, pillars, meshes)
Tree Building: Balanced splits for performance
Traversal: Front-to-back rendering, collision queries, raycasting
Editor Support: Brush IDs and metadata for reconstruction
Optimization: Precomputed Potentially Visible Sets (PVS) and AABBs

Implementation Details:
enum class SurfaceType {
    WALL, FLOOR, CEILING, PILLAR, CUSTOM_MESH
};

struct BSPSurface {
    MeshData mesh;          // Vertices, indices, UVs
    Material material;      // Texture, color
    SurfaceType type;       // Wall, floor, etc.
    AABB bounds;            // Bounding box for culling/collision
    uint32_t brushId;       // Editor reference
};

struct BSPNode {
    Line splitter;              // 2D partitioning line (Doom-like)
    std::vector<BSPSurface> surfaces;  // Surfaces in leaf nodes
    BSPNode* front;             // Front subtree
    BSPNode* back;              // Back subtree
    AABB bounds;                // Node bounding box
    float lightMod;             // Ambient lighting multiplier
    bool isLeaf;                // Leaf node flag
    std::vector<EntityID> entities;  // Lights, spawns, etc.
};

Key Methods:

BuildTree(std::vector<BSPSurface>& surfaces, uint32_t maxDepth = 16): Recursive partitioning
TraverseFrontToBack(Camera camera, std::vector<BSPSurface>& result): Rendering optimization
FindCollisions(AABB bounds, std::vector<BSPSurface>& result): Broad-phase collision
Raycast(Ray ray, RaycastHit& hit): Surface intersection
Optimization: Precompute PVS for leaf nodes to reduce overdraw

Best Practices:

Balanced Splits: Use median splitting to minimize tree depth
PVS: Precompute visibility for rendering efficiency
Editor Metadata: Store brush IDs for editor workflows

2. Map Loading System (10 hours)
Objective: Load .map files with surfaces, lights, and entities.
Requirements:

File Format: Text-based, Source-inspired .map format
Content: Surfaces (walls, floors, ceilings, pillars, meshes), lights, entity spawns
Validation: Handle malformed files
Textures: Load materials for surfaces and skybox

Map Format:
MAP_VERSION 1.1
TEXTURE wall gray_wall.png
TEXTURE floor floor.png
TEXTURE ceiling ceiling.png
SKYBOX skybox/skybox_cube
LIGHT point 50.0 50.0 5.0 1.0 1.0 1.0 0.5 10.0 // x y z r g b intensity radius
ENTITY player_spawn 50.0 50.0 1.0 team1 // type x y z [params]
ENTITY light_point 75.0 75.0 5.0 1.0 1.0 1.0 0.7 15.0

BRUSH cube 0 0 100 100 0 10 type=wall texture=gray_wall.png // x1 z1 x2 z2 height_top height_bottom
BRUSH cylinder 50 50 5.0 10 type=pillar texture=gray_wall.png // x z radius height
BRUSH mesh custom1.obj 50 50 5 type=custom_mesh texture=gray_wall.png // file x z y

Key Components:

MapLoader::LoadMap(std::string path): Parse and build BSP
MapLoader::LoadTextures(): Load surface and skybox textures
MapLoader::PlaceEntities(): Spawn ECS entities (lights, spawns)
MapLoader::ValidateMap(): Check for overlaps and missing assets

Best Practices:

Source-Inspired: Clear, editable format with key-value pairs
Extensibility: Support custom mesh imports for complex geometry
Caching: Store parsed data for fast reloads

3. Mesh Simplification for Editor (8 hours, new task)
Objective: Simplify creation of complex geometry for editor workflows.
Requirements:

Brush Primitives: Cube, cylinder, wedge, plus custom mesh imports
CSG Operations: Union, subtract, intersect for brush-based design
Mesh Optimization: Simplify meshes for collision and rendering
Editor Integration: Generate surfaces from brushes and meshes

Implementation:
class MeshBuilder {
    MeshData CreateCube(Vector3 min, Vector3 max);
    MeshData CreateCylinder(Vector3 center, float radius, float height);
    MeshData ImportMesh(std::string objPath);
    MeshData SimplifyMesh(MeshData input, float reductionFactor); // Reduce vertex count
    MeshData ApplyCSG(MeshData a, MeshData b, CSGOperation op);
};

Best Practices:

CSG Simplicity: Use Source-style brush operations for level design
Mesh Simplification: Reduce polygons for collision meshes
Editor Tools: Preview simplified meshes in editor viewport

4. Collision Detection System (20 hours)
Objective: Handle collisions for BSP surfaces and entities.
Requirements:

BSP Collisions: AABB/mesh vs. surfaces (walls, floors, etc.)
Entity Collisions: AABB vs. AABB or mesh-based for complex entities
Hybrid System: Combine BSP and entity collisions
Response: Slide, stop, or knockback based on surface/entity type
Optimization: Broad-phase culling with spatial grid

Implementation:
enum class CollisionType {
    STATIC_SURFACE,  // BSP surfaces
    DYNAMIC_ENTITY,  // Players, projectiles
    TRIGGER          // Non-blocking
};

struct CollisionInfo {
    bool hit;
    Vector3 contactPoint;
    Vector3 normal;
    float penetration;
    Entity* hitEntity;      // nullptr for surfaces
    BSPSurface* hitSurface; // nullptr for entities
    CollisionType hitType;
};

class CollisionSystem {
    CollisionInfo CheckAABBSurface(AABB bounds, BSPSurface* surface);
    CollisionInfo CheckSweptAABBSurface(AABB bounds, Vector3 velocity, float deltaTime);
    CollisionInfo CheckMeshSurface(MeshData mesh, BSPSurface* surface); // Complex geometry
    CollisionInfo CheckAABBEntity(AABB bounds1, AABB bounds2);
    std::vector<CollisionInfo> CheckHybridCollisions(Entity* entity, Vector3 velocity, float deltaTime);
    void ResolveCollision(Entity* entity, const CollisionInfo& info);
    void UpdateSpatialGrid();
    std::vector<Entity*> QueryNearbyEntities(Vector3 pos, float radius);
};

Best Practices:

Broad-Phase: Spatial grid for entities, BSP for surfaces
Narrow-Phase: Precise AABB or mesh-based collision for accuracy
Debugging: Visualize bounds and contact points with raylib

5. Physics System (10 hours)
Objective: Enforce movement constraints and collision responses.
Requirements:

Validation: Check movements against surfaces and entities
Response: Slide along surfaces, stop at solids, knockback for entities
Editor Support: Physics properties (friction, bounciness) in .map

Implementation:
class PhysicsSystem : public System {
    void Update(float deltaTime) override {
        auto entities = registry_.view<Position, Velocity, Collidable>();
        for (auto entity : entities) {
            auto& pos = entities.get<Position>(entity);
            auto& vel = entities.get<Velocity>(entity);
            auto& col = entities.get<Collidable>(entity);
            auto collisions = collisionSystem_->CheckHybridCollisions(entity, vel.value, deltaTime);
            for (const auto& colInfo : collisions) {
                ResolveCollision(entity, colInfo);
            }
            pos.value += vel.value * deltaTime;
        }
    }
};

Best Practices:

Sliding: Vector projection for smooth surface sliding
Entity Rules: Team-based collision filtering

6. Raycasting Enhancements (10 hours)
Objective: Accurate ray intersections for surfaces and entities.
Requirements:

Surface Raycasting: Intersect with BSP surfaces
Entity Raycasting: Check dynamic entities
Hit Details: Distance, normal, UV, hit surface/entity
Debugging: Visualize rays in game and editor

Implementation:
struct RaycastHit {
    bool hit;
    float distance;
    Vector3 point;
    Vector3 normal;
    BSPSurface* surface;
    Entity* entity;
    Vector2 uv;
};

class Raycaster {
    RaycastHit CastRay(Vector3 origin, Vector3 direction, float maxDistance);
    std::vector<RaycastHit> CastRayAll(Vector3 origin, Vector3 direction, float maxDistance);
};

Best Practices:

Optimization: BSP traversal for surface hits
Entity Checks: Use spatial grid for efficiency

7. Floor/Ceiling Rendering (7 hours)
Objective: Render floors, ceilings, and other surfaces.
Requirements:

Geometry: Quad-based rendering for horizontal surfaces
Textures: UV-mapped textures from .map
Optimization: Batch by material

Implementation:
class Renderer {
    void RenderSurfaces(BSPNode* node, Camera camera) {
        for (const auto& surface : node->surfaces) {
            DrawTexturedMesh(surface.mesh, surface.material, surface.transform);
        }
    }
};

8. Skybox System (6 hours)
Objective: Render immersive background.
Requirements:

Geometry: Cube or sphere with cube map
Rendering: Draw after BSP, before entities
Editor Support: Preview in editor viewport

Implementation:
class Skybox {
    TextureCube cubeMap;
    Model skyboxModel;
    Shader skyboxShader;
    void Load(std::string texturePath);
    void Render(Camera camera);
};

9. Lighting System (8 hours)
Objective: Support scalable lighting with dynamic culling.
Requirements:

Light Types: Point, ambient, directional
Culling: Distance-based and frustum culling
Rendering: Apply to surfaces and entities
Editor Support: Visualize light properties

Implementation:
struct LightComponent : public Component {
    Vector3 position;
    Color color;
    float intensity;
    float radius;
    LightType type; // POINT, AMBIENT, DIRECTIONAL
};

class LightingSystem : public System {
    void Update(float deltaTime) override {
        auto lights = registry_.view<LightComponent>();
        std::vector<LightComponent*> activeLights;
        for (auto entity : lights) {
            auto& light = lights.get<LightComponent>(entity);
            if (IsLightInRange(light, camera.position)) {
                activeLights.push_back(&light);
            }
        }
        renderer_->ApplyLights(activeLights);
    }
};

Best Practices:

Dynamic Culling: Limit to 8-16 active lights per frame
Spatial Partitioning: Use BSP for light queries


🧪 Testing Strategy
Unit Tests (30% of time)

BSP Construction: Verify balanced splits and surface assignment
Map Loading: Parse .map with surfaces, lights, entities
Collision: Test AABB/mesh vs. surfaces and entities
Raycasting: Validate hit accuracy
Lighting: Check culling and light application
Mesh Simplification: Verify reduced vertex counts

Integration Tests (40% of time)

Level Loading: Load complex .map with diverse surfaces
Navigation: No clipping through surfaces or entities
Rendering: 60+ FPS with 100+ surfaces
Lighting: Dynamic culling works correctly

Manual Testing (30% of time)

Navigation: Walk through multi-room levels
Edge Cases: High-speed collisions, complex meshes
Visuals: No seams, proper lighting
Editor Prep: .map files editable in text


🏗️ Implementation Order
Week 1: Core Systems (48 hours)

Day 1-2: BSP Tree Implementation (14h)
Day 3: Map Loading System (10h)
Day 4-5: Collision Detection System (20h)
Day 6: Mesh Simplification Tools (8h)

Week 2: Rendering & Polish (32 hours)

Day 1-2: Physics System Integration (10h)
Day 3: Raycasting Enhancements (10h)
Day 4: Floor/Ceiling Rendering (7h)
Day 5: Skybox System (6h)
Day 6-7: Lighting System and Testing (8h)


🔗 Dependencies & Integration Points
ECS Integration

Components: Surface, LightComponent, Collidable
Systems: CollisionSystem, PhysicsSystem, LightingSystem, RenderSystem

Rendering Integration

BSP Traversal: Render surfaces efficiently
Skybox: Render before entities
Lighting: Apply culled lights to surfaces/entities

Editor Integration

Map Format: Support surfaces, lights, entities
Mesh Builder: Simplify complex geometry creation
Debugging: Visualize surfaces, collisions, lights


📊 Success Metrics
Functional Requirements

✅ Navigate BSP levels without clipping
✅ Smooth collision for surfaces and entities
✅ Accurate raycasting for hits
✅ Scalable lighting with culling
✅ Editor-friendly .map format

Performance Targets

Frame Rate: 60+ FPS with 100+ surfaces
Memory: < 50MB for typical level
Load Time: < 2 seconds
Collision Queries: < 1ms per frame

Quality Standards

Visuals: No seams, consistent lighting
Physics: Smooth responses
Reliability: No crashes on invalid .map
Editor Usability: Human-readable format


🎮 Milestone Deliverables
Week 1 Milestone: Basic Navigation

Load .map with surfaces, lights, entities
Navigate without clipping
BSP renders surfaces efficiently

Week 2 Milestone: Complete World System

Complex levels with diverse surfaces
Smooth collision and raycasting
Scalable lighting and skybox
Editor-ready .map format


🚨 Risks & Mitigations
Technical Risks

BSP Complexity: Start with simple splits
Collision Edge Cases: Unit test complex meshes
Lighting Performance: Cull lights aggressively

Scope Risks

Feature Creep: Focus on core systems
Editor Complexity: Defer advanced features

Timeline Risks

Debugging: Allocate extra time for collisions
Learning Curve: Reference Quake/Source BSPs


📚 References & Resources

BSP Theory: Quake BSP File Format
Collision Detection: Real-Time Collision Detection (Ericson)
Lighting: Unreal Engine Lighting Docs
Editor Design: Valve Hammer Editor Docs


✅ Phase 2 Sign-Off Checklist

 BSP tree with surfaces implemented
 .map loading with surfaces, lights, entities
 Hybrid collision detection
 Physics constraints working
 Raycasting for hits
 Floor/ceiling rendering
 Skybox and scalable lighting
 Mesh simplification tools
 All tests passing
 Performance targets met
 Code documented and editor-ready

