# README.md: PaintSplash - P2P Pseudo-3D Paint-Shooter Prototype

## Project Overview

**PaintSplash** (tentative) is a multiplayer, pseudo-3D (Doom-like raycasting) paint-shooter built with raylib in C++. Players form teams and shoot paint projectiles that splatter and colorize a gray, blockout-style world. The game emphasizes emergent creativity (e.g., marking territories, vandalizing foes) in a P2P setup for 2-8 players. Core mechanics include movement, aiming/firing, team scoring, and dynamic world modification via decals.

### Key Features

  - **Core Gameplay**: First-person raycasting view with 2D sprites for players/enemies. Paint gun fires projectiles that apply persistent decals to walls, floors, players, and objects.
  - **Multiplayer**: Networking design is TBD. A networking lead will integrate the chosen approach into the current engine (inputs, replication, and session management will be coordinated later).
  - **World Building**: BSP-partitioned levels for efficient rendering. Features a **robust, filter-based hybrid collision system** (BSP geometry + dynamic entities).

**Architectural refactoring in progress**: Currently resolving design issues with BSP→entity conversion. See `docs/PHASE2_ARCHITECTURAL_REFACTORING_PLAN.md` for details.
  - **Systems**:
      - **High-Performance ECS**: A data-oriented Entity-Component-System using **archetype bitmasks** for fast, cache-friendly queries.
      - **Event-Driven**: Decoupled systems for triggers (e.g., hit → decal + sound + score).
      - **Developer Console**: An in-game console for debugging, visualization toggles, and commands (`noclip`, etc.).
      - **Simple Lighting & Audio**: Raylib-based lighting and spatial audio for shots/splats.
      - **UI**: Raygui for menus, HUD, and overlays.
  - **Editor**: Standalone BSP blockout tool for level creation (brushes → CSG → export to .map).
  - **Gamestates**: MENU → LOBBY → LOADING (auth/assets) → GAME (with pause overlay).

### Tech Stack

  - **Language**: C++ (C++17+).
  - **Rendering/Input**: raylib (2D/3D primitives, textures, audio).
  - **Networking**: TBD (to be decided by networking lead).
  - **UI**: Raygui (immediate-mode panels).
  - **Build**: CMake (cross-platform: Windows/Linux/Mac).
  - **Data**: Simple text/binary formats (.map for levels, JSON for config).
  - **No External Deps**: Avoid heavy libs; raylib handles most (e.g., no FMOD, use built-in audio).

## Phase 1 Progress - 3D FPS Foundation (Completed)

## Phase 2 Status - World, Physics & Debugging (Architectural refactoring required)

**Current Status**: 85% Complete - Major Architecture Issues Discovered

### Completed Systems:
- **ECS Architecture**: Full Entity-Component-System implementation
- **Collision System**: Hybrid BSP + Entity collision detection
- **Physics System**: Player state management, gravity, crouching
- **Console System**: Developer tools with noclip, render_bounds commands
- **Input System**: Enhanced mouse/keyboard controls
- **BSP Tree**: Efficient spatial partitioning and raycasting

### ❌ **Critical Architecture Issues:**
- **BSP→Entity Conversion**: Static world geometry incorrectly converted to entities
- **Rendering Pipeline**: Mixed responsibilities between world/entity rendering
- **Entity Registration**: Timing issues preventing proper system integration
- **Performance**: Entity overhead for static geometry

### 📋 **Refactoring Plan:**
See `docs/PHASE2_ARCHITECTURAL_REFACTORING_PLAN.md` for complete architectural refactoring plan.

**Next Steps**: Awaiting user approval to proceed with WorldRenderer separation.

### 🎮 Current Controls:

| Key | Action |
| :--- | :--- |
| **W/S** | Move Forward/Backward |
| **A/D** | Strafe Left/Right |
| **Space** | Fly Up |
| **Cmd/Alt/C** | Fly Down |
| **Shift** | Run (2x speed) |
| **Ctrl** | Crouch (0.5x speed) |
| **Mouse** | Look around (full 360°) |
| **ESC** | Pause |

### 🔧 Technical Achievements:

  - **Spherical Coordinates**: Eliminates camera gimbal lock.
  - **Remappable Input**: Action-based system for easy control customization.
  - **3D Movement Vectors**: Proper forward/backward/strafe relative to look direction.
  - **Component System**: Modular entity architecture ready for expansion.

### Project Structure

```
project_root/
├── CMakeLists.txt              # Root build
├── assets/                     # Game assets
│   ├── maps/
│   ├── textures/
│   │   └── skyboxcubemaps/     # 3x4 cross skybox images
│   └── sounds/
├── src/                        # Game source
│   ├── CMakeLists.txt
│   ├── main.cpp                # Entry point
│   ├── Game.h / Game.cpp       # Main loop orchestration
│   ├── core/                   # Engine, StateManager
│   ├── ecs/
│   │   ├── Components/         # Position, Velocity, Sprite, Player, Collidable, ...
│   │   └── Systems/            # RenderSystem, PlayerSystem, WorldSystem, PhysicsSystem, InputSystem, ...
│   ├── world/                  # BSPTree, WorldGeometry, MapLoader
│   ├── rendering/              # Renderer, WorldRenderer, Skybox, TextureManager
│   │   └── shaders/skybox/     # skybox.vs, skybox.fs (copied to build/bin)
│   ├── physics/                # PhysicsSystem
│   ├── input/                  # Input and mappings
│   ├── ui/                     # UI/console
│   ├── loading/                # Loading
│   ├── audio/                  # Audio
│   ├── teams/                  # Teams
│   ├── lighting/               # Lighting (planned)
│   ├── events/                 # EventManager
│   └── utils/                  # Logger, PathUtils
├── editor/                     # BSP Editor tool
│   ├── CMakeLists.txt          # Editor build configuration
│   ├── main_editor.cpp
│   ├── Editor.h/cpp            # Core loop
│   ├── Brush.h/cpp             # Primitives
│   ├── CSG.h/cpp               # Operations
│   ├── BSPCompiler.h/cpp       # To BSP tree
│   ├── UI.h/cpp                # Panels
│   ├── Viewport.h/cpp          # Cameras/grid
│   └── MapExporter.h/cpp       # .map output
├── config/                     # Optional: JSON settings
├── docs/                       # This README, architecture.md
└── README.md                   # You are here!
```

### Jam Snapshot (Current State)

- Skybox using a 3x4 cross cubemap; renders at infinite distance.
- BSP surfaces render with textures and basic UVs (via WorldRenderer).
- First-person controller with smooth accel/decel, jumping, and ground snap.
- Minimal on-screen debug (Meshes count, Camera position). No physics spam.
- Assets load correctly when run from terminal or app double-click (robust path fallbacks).
- Networking not specified yet; will be integrated later.

### Quick Start

1.  **Build**:

    ```
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j4
    ```

      - Output: `paintsplash` in `build/bin/`.

2.  **Run Game**:

    ```
    cd build/bin
    ./paintsplash
    ```

3.  **Run Editor**: (not included in jam snapshot)


4.  **Test Multiplayer**: Run two instances; one hosts, other joins via IP.

### Development Guidelines

  - **Conventions**: RAII for resources (e.g., UnloadTexture() in dtors). Use Logger for debug.
  - **Perf Targets**: 60 FPS on mid-range hardware; limit entities (\<500), decals per surface (\<20).
  - **Multiplayer Rules**: Host authoritative for teams/auth; client prediction for movement/shots.
  - **Future Roadmap**: Particles, AI bots, shader lighting, save states.

### Contributing

  - Fork, branch (e.g., `feat/weapons`), PR with tests.
  - Issues: Tag [bug], [feat], [multiplayer].

### License

MIT - Free to use/modify.

-----

# Implementation Plan with Testing

## Overview

This plan outlines a phased rollout for **PaintSplash**, prioritizing a playable prototype in 4-6 weeks (assuming 1-2 devs). Each phase includes milestones, tasks, dependencies, and testing strategies. Focus: Iterative builds, event-driven integration, and P2P validation. Total estimate: 200-300 hours.

Use Git for versioning (branches per phase). Tools: CMake for builds, raylib examples for baselines, Valgrind for leaks, unit tests via Google Test (add to CMake).

## Phase 1: Core Engine & ECS (Week 1 | \~40 hours)

**Goal**: Basic loop with movement and rendering skeleton.

| Task | Description | Dependencies | Est. Time |
| :--- | :--- | :--- | :--- |
| Setup Project | CMake root/editor; raylib/raygui/ENet integration; basic main.cpp with window. | None | 4h |
| Implement Engine & StateManager | FSM for MENU/GAME; delegate updates. Add EventManager (pub-sub queue). | Raylib | 8h |
| **High-Performance ECS** | Entity/Component/System with **archetype bitmasks** for fast, cache-friendly queries. | None | 12h |
| Input & Movement | InputSystem (keys/mouse); MovementSystem updates Position. | ECS | 8h |
| Basic Renderer | Raycaster for empty view; project simple sprites. | ECS, Raylib | 8h |

**Testing**:

  - **Unit**: Google Test for ECS queries (e.g., add/get components, **verify bitmask filtering**); Event post/subscribe.
  - **Integration**: Manual: Run loop, verify WASD movement in empty scene (60 FPS check).
  - **Edge**: Infinite loop crash test; memory leaks via Valgrind.
  - **Milestone**: Playable single-player walk-around in gray void.

## ✅ Phase 2: World & Physics - COMPLETED (January 2025)

**Goal**: Loadable BSP levels with a robust, filter-based hybrid collision system, advanced player physics, and a skybox environment.

### 🎯 **COMPLETED FEATURES**:

#### **World Systems** ✅
- **BSP Tree Implementation**: Complete binary space partitioning with efficient ray casting and collision detection
- **Map Loading System**: .map file parsing with texture loading and BSP tree construction
- **WorldSystem**: Dedicated system for managing map entities and world geometry
- **Default Test Map**: Two-room layout connected by corridor (second room open-roofed for skybox testing)

#### **Collision & Physics** ✅
- **Hybrid Collision System**: BSP geometry + entity-to-entity collision with bitmask filtering
- **Enhanced Collidable Component**: Collision layers (PLAYER, ENEMY, WORLD) and masks for selective collision
- **Advanced Physics**: State-based gravity, crouching mechanics, wall sliding, and collision response
- **Player State Management**: ON_GROUND, IN_AIR, CROUCHING with proper transitions

#### **Developer Tools** ✅
- **Developer Console**: Tilde (~) key toggle with command parsing and history
- **Console Commands**: `noclip`, `render_bounds`, `help`, `clear`, `echo`, `list`
- **Enhanced Raycasting**: Hit detection against both BSP world and dynamic entities

#### **Architecture Refactoring** ✅
- **Engine-Centric Design**: Engine now manages EventManager, StateManager, and all systems
- **Simplified Game Class**: Game class focuses only on main loop orchestration
- **PlayerSystem**: Dedicated system for player entity creation and camera management
- **WorldSystem**: Dedicated system for map entity management
- **Proper Separation**: Clean separation between game logic, engine systems, and rendering

### 🧪 **TESTING CAPABILITIES**:
- Load and navigate test map with collision detection
- Player physics with gravity, jumping, and crouching
- Console commands for debugging (noclip mode, bounds visualization)
- BSP-based world geometry with efficient rendering

### 📋 **CURRENT STATUS**:
- **Mouse Input**: ✅ Fixed - PlayerSystem properly connected to InputSystem
- **FPS Counter**: ✅ Moved to top right corner, white text
- **Code Cleanup**: ✅ Removed Doxygen comments, consolidated .inl files
- **World Rendering**: ⚠️ **DEBUGGING** - Map data created but entities not displaying properly
- **Ready for Phase 3**: Core systems functional, world loading working

### 🔧 **CURRENT DEBUGGING FOCUS**:
The map data is being created successfully and BSP trees are building, but the world geometry is not rendering. This appears to be an issue with the RenderSystem not properly registering or displaying the world entities. The PlayerSystem and other systems are working correctly.

## Phase 3: Weapons, Decals & Audio (Week 2-3 | \~60 hours)

**Goal**: Firing and painting mechanics.

| Task | Description | Dependencies | Est. Time |
| :--- | :--- | :--- | :--- |
| WeaponSystem | Add Weapon/Projectile components; fire on input, spawn entities. | ECS, Input | 12h |
| DecalSystem | ImageDrawCircle() for splatters on textures; UV mapping for BSP. | Renderer, Events | 15h |
| AudioSystem | AudioEmitter component; play spatial sounds on events (e.g., shot/splat). | Raylib Audio, Events | 10h |
| Integrate Events | Wire Weapon → Event: Hit → Decal/Audio. | EventManager | 8h |
| Editor CSG/Export | Brush union/subtract; compile to BSP; .map output. | Editor Basics | 15h |

**Testing**:

  - **Unit**: Projectile trajectory math; decal blend (mock Image, assert pixel changes).
  - **Integration**: Fire paint; verify splat on wall (screenshot diff) and sound plays.
  - **Edge**: Max projectiles (50); overlapping decals (no overwrite bugs).
  - **Milestone**: Shoot and paint a wall; audio cues; editor builds multi-room level.

## Phase 4: Multiplayer & Teams (Week 3-4 | \~50 hours)

**Goal**: P2P sessions with teams.

| Task | Description | Dependencies | Est. Time |
| :--- | :--- | :--- | :--- |
| Networking Basics | NetworkManager: Peers, UDP packets, delta sync for Position/Velocity. | ENet | 12h |
| SyncSystem | ECS delta serialization; broadcast changes. | ECS, Networking | 10h |
| TeamSystem | Team component; assign/balance; friendly checks in Weapon. | Events | 8h |
| LoadingManager | Async asset load (progress UI); readiness/auth handshake (hashes/tokens). | Networking, MapLoader | 10h |
| UI Integration | UIManager: MENU (join IP), LOBBY (team select), HUD (score). | Raygui, States | 10h |

**Testing**:

  - **Unit**: Packet serialization (round-trip mock data); team assign logic.
  - **Integration**: Two instances: Join, move in sync, paint shared world (network replay tool).
  - **Edge**: High latency (simulate 200ms ping); desync rollback (assert positions match after correction).
  - **Milestone**: 2-player team match; paint affects both; auth prevents mismatched maps.

## Phase 5: Polish & Systems (Week 4-5 | \~40 hours)

**Goal**: Full features with usability.

| Task | Description | Dependencies | Est. Time |
| :--- | :--- | :--- | :--- |
| LightingSystem | Raylib lights (point/ambient); attenuation on BSP/sprites via events. | Renderer, Events | 10h |
| PauseSystem | ESC toggle; overlay menu (resume/quit); pause ECS/network heartbeats. | UI, States | 6h |
| ConfigManager | JSON load/save for keys/volumes/FOV. | None | 8h |
| Editor Polish | Grid snapping, multi-viewport, entity placeholders (e.g., spawn points). | Editor | 10h |
| Full Integration | Wire all events; optimize (e.g., entity pooling). | All | 6h |

**Testing**:

  - **Unit**: Light modulation (mock distance, assert color fade).
  - **Integration**: End-to-end: Menu → Load → Game → Pause → Multi-paint → Score.
  - **Edge**: Config invalid (fallback defaults); low FPS (throttle entities).
  - **Milestone**: Complete session with lighting/audio; editor builds test level used in multiplayer.

## Phase 6: Testing & Optimization (Week 5-6 | \~30 hours)

**Goal**: Stable prototype.

| Task | Description | Dependencies | Est. Time |
| :--- | :--- | :--- | :--- |
| Comprehensive Tests | Expand units (80% coverage); add integration scripts (e.g., bot sim for P2P). | All | 10h |
| Perf Profiling | Raylib traces; cap at 60 FPS, \<16ms/frame. | Builds | 8h |
| Bug Hunt | Cross-platform (Win/Linux); multiplayer edge (NAT, disconnects). | All | 8h |
| Documentation | Update README; add in-code comments. | All | 4h |

**Testing Strategies** (Overall):

  - **Unit (Google Test)**: 70% coverage; run pre-commit (CMake target).
  - **Integration**: Custom harness (e.g., headless mode for net tests); raylib screenshots for visual diffs.
  - **Manual/Playtest**: Weekly sessions (2+ players); track bugs in GitHub Issues. Utilize the **developer console** during sessions to quickly diagnose issues and test states.
  - **Automated**: CI via GitHub Actions (build/test on push); perf benchmarks (e.g., FPS in large maps).
  - **Multiplayer-Specific**: Localhost sim (multiple instances); tools like Wireshark for packet inspection.
  - **Coverage Tools**: gcovr for reports; Valgrind for leaks.

## Risks & Mitigations

  - **P2P Sync Issues**: Early net prototype; fallback to localhost.
  - **BSP Complexity**: Start hardcoded; unit-test splits.
  - **Raylib Limits**: Monitor lights (max 4); shader fallback if needed.
  - **Scope Creep**: Lock features post-Phase 3; defer particles/AI.

**Success Metrics**: Playable 4-player match with painting/scoring; editor builds valid levels; 60 FPS average.

Track progress in GitHub Projects. Questions? Open an issue\!