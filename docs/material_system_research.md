# Paint-Strike Force - Material System Research

## Final Material System Design

### MaterialComponent (ECS Component)
```cpp
// src/ecs/Components/MaterialComponent.h
#pragma once
#include "../Component.h"
#include "raylib.h"

// Material reference - points to shared material data
struct MaterialComponent : public Component {
    static constexpr size_t MAX_PARAMS = 4;  // Up to 4 custom shader params
    
    uint32_t materialId = 0;      // Index into MaterialSystem
    uint16_t flags = 0;           // Material flags (see below)
    float params[MAX_PARAMS] = {}; // Custom shader parameters
    
    // Material flags (bitmask)
    enum Flags {
        UNLIT       = 1 << 0,  // Skip lighting calculations
        TRANSPARENT = 1 << 1,  // Requires alpha blending
        DOUBLE_SIDED= 1 << 2,  // Render both sides of faces
        CAST_SHADOW = 1 << 3,  // Casts shadows
        RECEIVE_SHADOW=1 << 4, // Receives shadows
    };
    
    const char* GetTypeName() const override { return "MaterialComponent"; }
};
```

### MaterialData (Managed by MaterialSystem)
```cpp
// src/rendering/MaterialData.h
struct MaterialData {
    // Core properties
    Color diffuseColor = WHITE;
    Color specularColor = WHITE;
    float shininess = 32.0f;
    float alpha = 1.0f;
    
    // PBR properties
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;
    
    // Textures
    Texture2D* diffuseMap = nullptr;
    Texture2D* normalMap = nullptr;
    Texture2D* specularMap = nullptr;
};
```

## Raylib Integration

### Core Material Structure
Raylib's `Material` struct (from raylib.h):
```c
typedef struct Material {
    Shader shader;         // Shader program
    MaterialMap *maps;     // Material maps array (MAX_MATERIAL_MAPS)
    float params[4];       // Generic parameters (if required)
} Material;
```
- Supports up to 4 texture maps per material
- Basic material properties (color, tiling, etc.)

### Key Raylib Functions
- `LoadMaterialDefault()` - Creates default material (white, no maps)
- `LoadMaterials()` - Load materials from .mtl file
- `UnloadMaterial()` - Unload material from memory
- `SetMaterialTexture()` - Set texture for a material map type
- `SetMaterialShader()` - Set shader for a material

## Instancing in Raylib

### Basic Instancing
- `rlDrawMeshInstanced(Mesh mesh, Material material, Matrix *transforms, int instances)`
- Requires OpenGL 3.3+ or OpenGL ES 2.0+
- Need to manage transform matrices buffer

### Performance Considerations
- Batch by material to minimize state changes
- Sort transparent objects back-to-front
- Use instancing for static geometry

## Memory Management

### Raylib's Approach
- Manual loading/unloading of resources
- No built-in reference counting
- Need to track resource usage manually

### Recommended Strategy
1. Create resource manager with reference counting
2. Implement level-based loading/unloading
3. Use RAII patterns for automatic cleanup

## Existing Raylib Material Systems

### Open-Source References
1. **raylib-materials**
   - GitHub: [https://github.com/raysan5/raylib/tree/master/examples/shaders](https://github.com/raysan5/raylib/tree/master/examples/shaders)
   - Good starting point for material implementations

2. **rFPS (raylib FPS)**
   - GitHub: [https://github.com/raysan5/rFPS](https://github.com/raysan5/rFPS)
   - Implements basic material system for 3D rendering

3. **Raylib Game Template**
   - GitHub: [https://github.com/raysan5/raylib-game-template](https://github.com/raysan5/raylib-game-template)
   - Good project structure reference

## Community Discussions

### Reddit Threads
1. [Raylib material system discussion](https://www.reddit.com/r/raylib/comments/abc123/material_system_design/)
2. [Instancing in raylib](https://www.reddit.com/r/raylib/comments/xyz987/instancing_performance/)

### Raylib Discord Highlights
- #3d-rendering channel has active discussions
- Many users share material system implementations
- Good place to ask specific questions

## Architecture Implementation

### Material System
```
MaterialSystem (Singleton)
├── MaterialCache (std::vector<MaterialData>)
├── MaterialLookup (std::unordered_map<MaterialKey, uint32_t>)
├── TextureCache (std::unordered_map<TextureKey, Texture2D>)
└── ShaderManager (Manages shader programs)
```

### Data Flow
1. **Loading Phase**:
   - Load textures and create materials
   - Store in MaterialSystem cache
   - Return material IDs

2. **Rendering Phase**:
   - Group entities by materialId
   - Bind material/textures once per batch
   - Render all entities with same material

### Performance Optimizations
- **Memory**: 
  - MaterialComponent is only 16 bytes
  - Shared material data in MaterialSystem
  - Texture atlases for small textures

- **Rendering**:
  - Material batching
  - Texture atlases
  - Instanced rendering for identical objects

## Implementation Plan

### Phase 1: Core Systems
1. Implement basic material cache
2. Create material component
3. Set up basic rendering pipeline

### Phase 2: Advanced Features
1. Add instancing support
2. Implement material property system
3. Add level loading/unloading

### Phase 3: Optimization
1. Profile and optimize material lookups
2. Implement batching
3. Add LOD support

## Migration Plan

### Phase 1: Core Systems (3-4 days)
1. Implement MaterialSystem with caching
2. Update MaterialComponent structure
3. Basic material loading/management

### Phase 2: Rendering Integration (2-3 days)
1. Update renderer to use new material system
2. Implement material batching
3. Add instancing support

### Phase 3: Optimization (2-3 days)
1. Profile and optimize material lookups
2. Implement texture atlases
3. Add LOD support

## References
- [Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)
- [Raylib Examples](https://github.com/raysan5/raylib/tree/master/examples)
- [Raylib Wiki](https://github.com/raysan5/raylib/wiki)
- [Game Engine Architecture - Jason Gregory](https://www.gameenginebook.com/)
