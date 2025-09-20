# ECS Architecture Analysis: Current State & Refactoring Plan

## Executive Summary

The current architecture has deviated from pure ECS principles by mixing non-ECS concerns (material management, texture loading) into the ECS pipeline. This analysis identifies architectural issues and proposes a comprehensive refactoring to create a modular, decoupled ECS environment.

## Current Architecture Issues

### 1. **Material Storage in WorldGeometry (Non-ECS)**
- **Problem**: Materials are stored in `WorldGeometry` (non-ECS class)
- **Impact**: MeshComponent directly accesses WorldGeometry.materials[materialId]
- **Violation**: ECS entities should not depend on non-ECS data structures

### 2. **WorldSystem Overload**
- **Problem**: WorldSystem handles map loading, material creation, geometry building, AND entity creation
- **Impact**: Single responsibility principle violation, tight coupling
- **Current Responsibilities**:
  - MapData creation and processing
  - Texture loading via TextureManager
  - Material creation and storage in WorldGeometry
  - BSP tree building
  - Dynamic entity creation
  - Update loop for dynamic entities

### 3. **Mixed Rendering Pipeline**
- **Problem**: Static geometry uses WorldRenderer, dynamic entities use RenderSystem
- **Impact**: Inconsistent rendering approaches, duplicated material handling

### 4. **Direct Component Coupling**
- **Problem**: MeshComponent knows about WorldGeometry.materials
- **Impact**: Components should be data-only, logic should be in systems

## Current Pipeline Flow

```
Map Loading → WorldSystem → WorldGeometry (materials) → MeshComponent → RenderSystem
     ↓              ↓              ↓                      ↓              ↓
MapData    TextureManager    BSP Tree              Access materials    Draw
     ↓              ↓              ↓                      ↓              ↓
Textures    Materials        Static Faces          Direct coupling     Mixed rendering
```

## Proposed ECS Architecture

### Core Components

#### 1. **MaterialComponent**
```cpp
struct MaterialComponent : public Component {
    Color diffuseColor = WHITE;
    float shininess = 0.0f;
    EntityID textureEntityId = INVALID_ENTITY_ID; // References TextureComponent entity
    std::string materialName = ""; // For debugging/identification
};
```

#### 2. **TextureComponent**
```cpp
struct TextureComponent : public Component {
    Texture2D texture = {0};
    std::string texturePath = "";
    TextureWrap wrapMode = TEXTURE_WRAP_REPEAT;
    TextureFilter filterMode = TEXTURE_FILTER_BILINEAR;
    int referenceCount = 0; // For resource management
};
```

#### 3. **MeshComponent** (Refactored)
```cpp
class MeshComponent : public Component {
    std::vector<MeshVertex> vertices;
    std::vector<MeshTriangle> triangles;
    // Remove direct material/texture references
    // Systems will query for related components via entity relationships
};
```

#### 4. **TransformComponent** (Already exists as Position)
- Rename Position → TransformComponent for clarity
- Add scale and rotation support

### Component Relationships & Distinctions

#### Material vs Texture Components
- **TextureComponent**: Pure data - contains the actual image/texture resource
- **MaterialComponent**: References a TextureComponent entity + defines how it should be rendered
- **Why separate**: Materials can share textures, textures can be used by multiple materials, enables composition

#### Entity Relationships
```
MeshEntity ────→ MaterialEntity ────→ TextureEntity
     │                    │                    │
  TransformComponent   MaterialComponent   TextureComponent
  (position/scale)      (color/shininess)   (image data)
```

### Dedicated Systems

#### 1. **AssetSystem** (New - Consolidated)
- **Responsibilities**:
  - Load textures from disk (replaces TextureManager singleton)
  - Cache loaded textures (prevents duplicate loading)
  - Manage texture lifecycle and reference counting
  - Handle texture unloading when no longer referenced
  - Provide unified asset loading interface for all resource types
- **Components**: TextureComponent (and future asset components)
- **Decoupled from**: WorldSystem, RenderSystem, direct rendering
- **Why consolidated**: Eliminates redundancy between loading (TextureManager) and caching (ResourceManager)
- **Benefits**: Single source of truth for all asset management, prevents resource leaks, enables hot-reloading

#### 2. **MaterialSystem** (New)
- **Responsibilities**:
  - Create and manage material entities
  - Assign materials to renderable entities
  - Link materials to texture entities
  - Update material properties at runtime
  - Handle material presets/templates
- **Components**: MaterialComponent, TextureComponent
- **Decoupled from**: WorldGeometry, direct rendering

#### 3. **MeshSystem** (New)
- **Responsibilities**:
  - Generate procedural meshes
  - Load mesh data from files
  - Update mesh geometry
  - Handle mesh transformations
- **Components**: MeshComponent, TransformComponent
- **Decoupled from**: Rendering, materials

#### 4. **WorldSystem** (Refactored)
- **Responsibilities** (Reduced):
  - Load map data
  - Create static geometry entities
  - Initialize BSP tree
  - **Remove**: Material management, texture loading, dynamic entity creation
- **Components**: TransformComponent, MeshComponent, MaterialComponent
- **Decoupled from**: TextureManager, AssetSystem

#### 5. **RenderSystem** (Refactored)
- **Responsibilities**:
  - Collect renderable entities
  - Sort by depth/material
  - Submit to Renderer
  - Handle both static and dynamic rendering
- **Components**: TransformComponent, MeshComponent, MaterialComponent, TextureComponent
- **Decoupled from**: WorldGeometry materials

### Resource Management

#### 1. **AssetSystem** (Centralized Resource Management)
- **Purpose**: Unified asset loading and resource management
- **Manages**: All game assets (textures, meshes, sounds, materials)
- **Benefits**: Prevents duplicate loading, centralized lifecycle management
- **Features**:
  - Reference counting for automatic cleanup
  - Asset caching and sharing
  - Synchronous and asynchronous loading
  - Asset dependency management

## Data Flow Architecture

### New Pipeline Flow

```
Map Loading → WorldSystem → ECS Entities → Systems → Renderer
     ↓              ↓              ↓              ↓              ↓
MapData    Create Entities    Components    Process         Draw
     ↓              ↓              ↓              ↓              ↓
Geometry    Transform+Mesh    Material+Tex    RenderSystem    Screen
```

### System Communication

#### Event-Driven Approach
```cpp
// Systems communicate via events, not direct dependencies
AssetSystem::OnTextureLoaded → MaterialSystem::AssignTexture
WorldSystem::OnMapLoaded → RenderSystem::UpdateStaticGeometry
```

#### Component Queries
```cpp
// Systems query for component combinations
auto renderables = GetEntitiesWith<TransformComponent, MeshComponent>();
auto textured = GetEntitiesWith<MeshComponent, TextureComponent>();
```

## Migration Strategy

### Phase 1: Component Creation
1. Create MaterialComponent, TextureComponent
2. Refactor MeshComponent to remove direct references
3. Update existing Position → TransformComponent

### Phase 2: System Creation
1. Implement AssetSystem for texture management
2. Create MaterialSystem for material assignment
3. Implement MeshSystem for geometry management

### Phase 3: WorldSystem Refactor
1. Remove material/texture logic from WorldSystem
2. Move dynamic entity creation to dedicated system
3. Simplify to map loading and static geometry only

### Phase 4: RenderSystem Overhaul
1. Make RenderSystem handle all rendering (static + dynamic)
2. Remove WorldRenderer dependency
3. Implement unified material/texture binding

### Phase 5: Integration Testing
1. Test each system independently
2. Verify component queries work correctly
3. Ensure rendering pipeline functions

## Benefits of Refactored Architecture

### 1. **Pure ECS Compliance**
- All data in components
- All logic in systems
- No cross-contamination between ECS and non-ECS

### 2. **Modularity**
- Systems can be added/removed independently
- Components are reusable across different contexts
- Clear separation of concerns

### 3. **Testability**
- Each system can be unit tested in isolation
- Components are pure data structures
- Mock components/systems easily

### 4. **Scalability**
- Easy to add new component types
- Systems can process entities in parallel
- Resource management is centralized

### 5. **Maintainability**
- Clear ownership of responsibilities
- Reduced coupling between systems
- Easier debugging and profiling

## Risk Assessment

### High Risk Areas
1. **Rendering Pipeline**: Major refactor could break visual output
2. **Material Assignment**: Complex entity-to-material mapping
3. **Performance**: Additional component queries might impact performance

### Mitigation Strategies
1. **Incremental Migration**: Phase-by-phase rollout with testing
2. **Fallback Systems**: Keep old pipeline as backup during transition
3. **Performance Monitoring**: Profile each phase for regressions

## Implementation Priority

### Immediate (Next Session)
1. Create MaterialComponent and TextureComponent
2. Implement consolidated AssetSystem (texture loading + resource management)
3. Begin MeshComponent decoupling from WorldGeometry

### Short Term (1-2 Sessions)
1. Complete WorldSystem refactor (remove material/texture logic)
2. Implement MaterialSystem for material assignment
3. Update RenderSystem for unified component-based rendering

### Long Term (Future Sessions)
1. Add MeshSystem for procedural mesh generation
2. Implement asset streaming and async loading
3. Add material presets and templates system

## Conclusion

The current architecture has grown organically and accumulated technical debt through ECS violations. This refined refactoring plan consolidates overlapping systems (AssetSystem vs ResourceManager) and clarifies component distinctions (Material vs Texture) to create a truly modular ECS architecture.

**Key Insights Addressed:**
- **Material ≠ Texture**: Materials define *how* to render, textures provide *what* to render
- **Single AssetSystem**: Consolidates loading, caching, and lifecycle management into one cohesive system
- **Entity Relationships**: Components reference other entities instead of direct coupling
- **Pure ECS Compliance**: All data lives in components, all logic lives in systems

This plan maintains all current functionality while establishing proper separation of concerns and improving long-term maintainability.
