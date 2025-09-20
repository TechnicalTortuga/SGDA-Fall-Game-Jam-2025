# PaintSplash - Advanced 3D FPS Engine

## Project Overview

**PaintSplash** is a high-performance 3D first-person shooter engine built with C++ and raylib. Currently featuring a **production-ready FPS controller** with industry-standard collision physics, advanced world rendering, and comprehensive testing capabilities.

### 🎯 **Current Status: Phase 2 COMPLETED** ✅

**All core systems fully implemented and production-ready!**

## ✅ **Key Achievements**

### **World Systems**
- **BSP Tree Implementation**: Complete binary space partitioning with efficient rendering and collision detection
- **Enhanced Test Map**: Multi-room environment with platforms, slopes, stairs, and comprehensive testing areas
- **Texture Rendering**: Full UV-mapped BSP surfaces with material system and skybox support
- **Map Loading**: Robust .map file parsing with fallback path detection

### **Advanced Physics & Collision** 
- **🏆 ZERO-JITTER COLLISION**: Constraint-based system inspired by classic FPS engines (Quake)
- **Corner Navigation**: Smooth sliding around corners without getting stuck
- **Automatic Stair Stepping**: Walk up obstacles ≤0.5 units high automatically  
- **Multi-Surface Collision**: Handles floors, walls, ceilings, and complex geometry
- **No Repositioning Artifacts**: Players cannot move into collision zones (no post-collision correction)

### **Professional FPS Controller**
- **Smooth Movement**: Acceleration/deceleration with momentum physics
- **Full 3D Movement**: WASD + mouse look with spherical coordinates (no gimbal lock)
- **State Management**: Ground/air/crouching states with proper transitions
- **Jump Mechanics**: Variable height jumping with gravity and terminal velocity
- **Player Physics**: Realistic gravity (-30 units/s²), friction, and air resistance

### **Advanced Architecture**
- **High-Performance ECS**: Archetype bitmasks for cache-friendly component queries
- **Event-Driven Systems**: Decoupled architecture with pub-sub messaging
- **Engine-Centric Design**: Clean separation between game logic and core systems
- **Developer Console**: Runtime debugging with commands (`noclip`, `render_bounds`, etc.)

## 🎮 **Controls**

| Key | Action |
|-----|--------|
| **W/S** | Move Forward/Backward |
| **A/D** | Strafe Left/Right |
| **Space** | Jump |
| **Ctrl** | Crouch |
| **Shift** | Sprint (2x speed) |
| **Mouse** | Look Around (360°) |
| **~** | Toggle Developer Console |
| **ESC** | Pause |

### **Console Commands**
- `noclip` - Toggle collision detection
- `render_bounds` - Show collision boundaries  
- `help` - List all commands
- `clear` - Clear console history

## 🏗️ **Enhanced Test Environment**

The current test map includes:

### **Room Layout**
- **Room 1**: Starting area (open roof for skybox testing)
- **Corridor**: Connects to second room with ceiling
- **Room 2**: Medium testing space
- **Room 3**: Large testing arena with comprehensive features

### **Testing Features**
- **Low Platform (1m)**: Tests automatic step-up collision
- **Medium Platform (2m)**: Requires jumping
- **High Platform (3.5m)**: Advanced jump testing
- **Simulated Slopes**: Series of stepped platforms creating slope effect
- **Stair Sequence**: 5 steps with 0.4m height each (perfect for step-up testing)
- **Corner Areas**: Various concave/convex corners for collision testing

## 🚀 **Quick Start**

### **Build & Run**
```bash
# Clone and build
git clone <repo-url>
cd SGDA-Fall-Game-Jam-2025
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Run the game
cd bin
./paintsplash
```

### **Testing the Collision System**
1. **Movement Testing**: Walk around using WASD - notice smooth acceleration/deceleration
2. **Wall Collision**: Run into walls - **zero jittering or visual artifacts**
3. **Corner Navigation**: Walk around corners - smooth sliding without getting stuck
4. **Platform Testing**: 
   - Walk onto the gray platform (automatic step-up)
   - Jump onto yellow platform (medium height)
   - Jump onto red platform (high challenge)
5. **Stair Testing**: Walk up the brown stairs in the corner area
6. **Console Testing**: Press `~` and try `noclip` to fly through walls

## 📊 **Technical Specifications**

### **Performance**
- **Target**: 60 FPS on mid-range hardware
- **Collision Detection**: O(log n) BSP traversal with 17+ faces per frame
- **Memory**: Efficient archetype-based ECS with minimal allocations
- **Rendering**: Batched draw calls with texture caching

### **Collision System Details**
- **Detection Method**: AABB-triangle intersection with early rejection
- **Response System**: Constraint-based (prevents movement vs. repositioning)
- **Step-up Height**: 0.5 units (configurable)
- **Wall Friction**: 0.995 for smooth sliding
- **Contact Tolerance**: 0.001 units for stable surface contact

### **Architecture Highlights**
- **Component Systems**: Position, Velocity, Collidable, Player, Sprite
- **Core Systems**: Render, Physics, Collision, Input, Player, World, Console
- **Event Management**: Decoupled pub-sub system for system communication
- **State Management**: Finite state machine for game states

## 📁 **Project Structure**

```
src/
├── core/                   # Engine, StateManager
├── ecs/
│   ├── Components/         # Position, Velocity, Player, Collidable...
│   └── Systems/           # Render, Physics, Collision, Input...
├── world/                 # BSPTree, MapLoader, WorldGeometry
├── physics/               # PhysicsSystem with constraint-based collision
├── rendering/             # WorldRenderer, Skybox, TextureManager
├── input/                 # Input mapping and handling
├── ui/                    # Developer console system
└── utils/                 # Logger, PathUtils

assets/
├── maps/                  # .map files (test_level.map)
├── textures/              # Wall, floor, skybox textures
│   ├── devtextures/       # Development texture set
│   └── skyboxcubemaps/    # Skybox cubemap images
└── shaders/               # GLSL shaders for rendering
```

## 🔮 **What's Next (Phase 3)**

**Ready for implementation:**
- **Weapons System**: Paint gun mechanics with projectile physics
- **Paint Decals**: Dynamic texture modification for paint splatters  
- **Audio System**: 3D positional audio for shots and impacts
- **Multiplayer**: P2P networking for 2-8 players
- **Team System**: Color-based teams with territory scoring

## 🛠️ **Development**

### **Contributing**
- Fork the repository and create feature branches
- Follow the existing code style and architecture patterns
- Test thoroughly using the enhanced test map
- Submit PRs with detailed descriptions

### **Tech Stack**
- **Language**: C++17+
- **Graphics/Audio**: raylib 5.5
- **UI**: raygui (immediate mode)
- **Build**: CMake (cross-platform)
- **Math**: Raylib math library with custom extensions

### **Key Design Principles**
- **Performance First**: Cache-friendly data structures and minimal allocations
- **Constraint-Based Collision**: Prevent invalid movement rather than correct after penetration
- **Component Composition**: Modular entity design for flexibility
- **Event-Driven**: Loosely coupled systems for maintainability

## 📝 **License**

MIT License - Free to use and modify for any purpose.

---

**Current Achievement**: ✅ **Production-ready FPS controller with zero-jitter collision system**

The engine now provides smooth, professional-grade first-person movement comparable to modern game engines, with comprehensive collision detection and a robust testing environment.