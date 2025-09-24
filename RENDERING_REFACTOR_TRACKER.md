# Rendering System Refactor Tracker

## Project Overview
**Goal**: Optimize 3D rendering system from 30-50 FPS to stable 60+ FPS while expanding Raylib integration and improving asset management.

**Timeline**: 3 phases targeting incremental performance improvements
**Current Status**: Phase 1 Complete ⚠️ (Performance still 30-40 FPS, debug stats not updating properly)

---

## Phase 1: Immediate Optimizations (Target: 45-55 FPS) - COMPLETED ⚠️

### 1.1 Cache Raylib Models ✅ COMPLETED
**Priority**: Critical  
**Estimated Impact**: +10-15 FPS  
**Actual Impact**: Partial - still seeing 30s with jerks
**Files**: `src/rendering/RenderAssetCache.h/.cpp`, `src/rendering/Renderer.cpp`, `src/ecs/Systems/RenderSystem.cpp`

**Completed Tasks**:
- ✅ Created RenderAssetCache class with LRU eviction
- ✅ Modified Renderer::DrawMesh3D() to use cached models
- ✅ Implemented cache invalidation via MeshSystem
- ✅ Added cache statistics and hash-based change detection

**Code Changes Needed**:
```cpp
// New: src/rendering/RenderAssetCache.h/.cpp
class RenderAssetCache {
    std::unordered_map<uint64_t, Model> modelCache_;
public:
    Model* GetOrCreateModel(const MeshComponent& mesh);
    void InvalidateModel(uint64_t meshId);
    void ClearCache();
};
```

### 1.2 Implement Proper Frustum Culling ✅ COMPLETED ⚠️
**Priority**: High  
**Estimated Impact**: +5-8 FPS  
**Actual Impact**: Debug stats not updating, mesh count changes suggest it's working
**Files**: `src/rendering/Renderer.cpp`

**Completed Tasks**:
- ✅ Enhanced IsEntityVisible() with FOV-aware frustum calculations
- ✅ Added proper bounding box intersection testing
- ✅ Integrated culling statistics tracking
- ⚠️ **Issue**: Debug display not updating culling stats properly

**Code Changes Needed**:
```cpp
// In Renderer.cpp
bool Renderer::IsEntityVisible(const Vector3& position, float boundingRadius) {
    // Replace current distance-only check with:
    BoundingBox entityBounds = {
        {position.x - boundingRadius, position.y - boundingRadius, position.z - boundingRadius},
        {position.x + boundingRadius, position.y + boundingRadius, position.z + boundingRadius}
    };
    return CheckCollisionBoxFrustum(entityBounds, GetCameraFrustum(camera_));
}
```

### 1.3 Basic Material Batching ✅ COMPLETED
**Priority**: High  
**Estimated Impact**: +3-5 FPS  
**Actual Impact**: TBD - batching stats being tracked
**Files**: `src/ecs/Systems/RenderSystem.cpp`

**Completed Tasks**:
- ✅ Enhanced SortRenderCommands() with material-first sorting
- ✅ Added BatchingStats tracking (state changes, batch sizes, efficiency)
- ✅ Integrated batching metrics into debug display
- ✅ Implemented material state change minimization

**Code Changes Needed**:
```cpp
// In RenderSystem::SortRenderCommands()
void RenderSystem::SortRenderCommands() {
    // First sort by material, then by depth
    std::sort(renderCommands_.begin(), renderCommands_.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            if (a.material && b.material) {
                if (a.material->textureSystemId != b.material->textureSystemId) {
                    return a.material->textureSystemId < b.material->textureSystemId;
                }
            }
            return a.depth < b.depth; // Secondary sort by depth
        });
}
```

### 1.4 Activate LOD System ✅ COMPLETED
**Priority**: Medium  
**Estimated Impact**: +2-4 FPS  
**Actual Impact**: TBD - system activated and connected
**Files**: `src/core/Engine.cpp`, `src/ecs/Systems/LODSystem.cpp`

**Completed Tasks**:
- ✅ Added LOD system to Engine initialization  
- ✅ Set global LOD distances (10m, 25m, 50m)
- ✅ Connected camera position updates from PlayerSystem
- ✅ Enabled LOD system with proper interdependencies

---

## Phase 1 Assessment ⚠️
**Target**: 45-55 FPS | **Actual**: Still 30-40 FPS with jerks
**Issues Identified**:
- Debug stats not updating (frustum culling stats)
- Performance gains less than expected
- World geometry not optimized (BSP algorithms uncached)
- Mesh optimizations may not be addressing bottlenecks

---

## Phase 2: Critical Performance Bottlenecks (Target: 60+ FPS)

**URGENT**: Phase 1 optimizations insufficient. Performance still 30-40 FPS with jerks.
**Root Cause Analysis Needed**: Mesh optimizations may not be addressing the real bottlenecks.

**IMMEDIATE PRIORITIES** (Execute in this order):
1. **World Geometry Optimization** - Likely the biggest bottleneck (excluded from Phase 1 optimizations)
2. **BSP Tree Performance** - May be causing traversal overhead every frame  
3. **GPU Instancing** - Key for scenes with many repeated primitives
4. **Debug Stats Fix** - Frustum culling stats not updating properly

**Expected Impact**: If world geometry is the real bottleneck, we could see +20-30 FPS improvement.

### 2.1 GPU Instancing for Identical Primitives 🔴 CRITICAL PRIORITY
**Priority**: Critical (Key optimization identified)  
**Estimated Impact**: +8-12 FPS for scenes with many repeated objects  
**Files**: `src/rendering/Renderer.cpp`

**Tasks**:
- [ ] Replace fake instancing with true GPU DrawMeshInstanced()
- [ ] Create instance transform matrices for GPU upload
- [ ] Batch primitives by type (all cubes, all spheres, etc.)
- [ ] Implement instance culling before GPU upload
- [ ] **CRITICAL**: This may be the key to reaching 60+ FPS target

**Code Changes Needed**:
```cpp
// Replace current FlushInstances() implementation
void Renderer::FlushInstances() {
    for (const auto& group : instanceGroups_) {
        const std::string& meshName = group.first;
        const std::vector<InstanceData>& instances = group.second;
        
        // Convert to transform matrices
        std::vector<Matrix> transforms;
        for (const auto& instance : instances) {
            Matrix transform = MatrixMultiply(
                MatrixMultiply(MatrixScale(instance.scale.x, instance.scale.y, instance.scale.z),
                               QuaternionToMatrix(instance.rotation)),
                MatrixTranslate(instance.position.x, instance.position.y, instance.position.z)
            );
            transforms.push_back(transform);
        }
        
        // True GPU instancing
        Mesh* cachedMesh = GetCachedMesh(meshName);
        Material* material = GetCachedMaterial(meshName);
        DrawMeshInstanced(*cachedMesh, *material, transforms.data(), transforms.size());
    }
}
```

### 2.2 World Geometry Optimization 🔴 CRITICAL PRIORITY  
**Priority**: Critical (Likely major bottleneck)
**Estimated Impact**: +15-25 FPS (World geometry may be the real performance killer)
**Files**: `src/ecs/Systems/RenderSystem.cpp`, `src/world/BSPTree.cpp`, `src/rendering/Renderer.cpp`

**Issues Identified**:
- World geometry rendering via RenderWorldGeometryDirect() not optimized
- BSP tree algorithms may have caching opportunities  
- World geometry bypasses all Phase 1 optimizations (no caching, no batching)

**Tasks**:
- [ ] **URGENT**: Profile world geometry rendering performance
- [ ] Apply model caching to world geometry (currently excluded)
- [ ] Implement world geometry frustum culling (currently basic BSP bounds check)
- [ ] Cache BSP tree traversal results
- [ ] Consider world geometry instancing for repeated wall/floor segments

### 2.3 BSP Tree Performance Analysis 🔴 CRITICAL PRIORITY
**Priority**: Critical (Potential major bottleneck)
**Estimated Impact**: +10-20 FPS if BSP traversal is inefficient
**Files**: `src/world/BSPTree.cpp`, `src/ecs/Systems/RenderSystem.cpp`

**Tasks**:
- [ ] **URGENT**: Analyze BSP tree ContainsPoint() and traversal performance
- [ ] Add BSP traversal caching for repeated queries
- [ ] Profile BSP vs entity culling performance ratio
- [ ] Consider BSP leaf caching for world geometry
- [ ] Investigate if BSP tree is being rebuilt unnecessarily

### 2.4 Static Mesh Caching System 🔴 Not Started
**Priority**: High  
**Estimated Impact**: +5-8 FPS  
**Files**: `src/ecs/Systems/MeshSystem.cpp`, `src/rendering/Renderer.cpp`

**Tasks**:
- [ ] Create static mesh pre-conversion system
- [ ] Mark static entities in MeshComponent (isStatic flag exists)
- [ ] Pre-upload static meshes to GPU at scene load
- [ ] Separate rendering path for static vs dynamic meshes

### 2.5 Draw Call Batching 🔴 Not Started  
**Priority**: Medium  
**Estimated Impact**: +3-6 FPS  
**Files**: `src/rendering/Renderer.cpp`

**Tasks**:
- [ ] Implement vertex/index buffer batching for similar meshes
- [ ] Group meshes by shader/material properties
- [ ] Batch world geometry separately from entity geometry
- [ ] Add batch size limits to prevent memory issues

### 2.6 Texture Atlas System 🔴 Not Started
**Priority**: Low  
**Estimated Impact**: +2-3 FPS  
**Files**: `src/ecs/Systems/AssetSystem.cpp`

**Tasks**:
- [ ] Combine small textures into atlases
- [ ] Update UV coordinates for atlased textures
- [ ] Implement runtime atlas generation for entity textures
- [ ] Add atlas management to AssetSystem

---

## Phase 3: Advanced Culling (Target: 60+ FPS stable)

### 3.1 Hierarchical BSP Culling 🔴 Not Started
**Priority**: High  
**Estimated Impact**: +5-10 FPS in complex scenes  
**Files**: `src/world/BSPTree.cpp`, `src/ecs/Systems/RenderSystem.cpp`

**Tasks**:
- [ ] Implement BSP frustum culling for entities
- [ ] Use BSP tree for spatial queries in RenderSystem
- [ ] Add BSP leaf-based entity grouping
- [ ] Optimize BSP traversal for rendering queries

### 3.2 BSP Rendering Optimization 🟡 In Progress
**Priority**: High  
**Estimated Impact**: +15-20 FPS in complex scenes  
**Files**: `src/world/BSPTree.h/cpp`, `src/rendering/Renderer.cpp`

**Status**: See dedicated [BSP Rendering Tracker](BSP_RENDERING_REFACTOR.md) for detailed progress

**Key Focus Areas**:
- PVS-based visibility determination
- Optimized BSP tree traversal
- Advanced culling techniques

### 3.3 Occlusion Culling (Basic) 🔴 Not Started
**Priority**: Medium  
**Estimated Impact**: +5-10 FPS in complex scenes  
**Files**: `src/rendering/OcclusionCuller.h/cpp`, `src/rendering/Renderer.cpp`

**Planned Work**:
- [ ] Add occlusion query system for large occluders
- [ ] Integrate with BSP tree for occluder detection
- [ ] Add temporal coherence for occlusion results

### 3.3 Dynamic LOD Simplification 🔴 Not Started
**Priority**: Medium  
**Estimated Impact**: +2-5 FPS  
**Files**: `src/ecs/Systems/LODSystem.cpp`

**Tasks**:
- [ ] Implement automatic mesh simplification
- [ ] Add runtime triangle reduction algorithms
- [ ] Create adaptive LOD based on performance metrics
- [ ] Integrate with mesh generation system

### 3.4 Temporal Optimizations 🔴 Not Started
**Priority**: Low  
**Estimated Impact**: +1-3 FPS (smoother frametime)  
**Files**: `src/core/Engine.cpp`, `src/ecs/Systems/RenderSystem.cpp`

**Tasks**:
- [ ] Spread expensive operations across multiple frames
- [ ] Implement time-sliced culling updates
- [ ] Add adaptive quality based on framerate
- [ ] Implement frame pacing for consistent timing

---

## EntityFactory Integration

### Asset Type Support 🔴 Not Started
**Priority**: High for game content  
**Files**: `src/world/EntityFactory.cpp`

**Tasks**:
- [ ] Add model loading support (`CreateModelGameObject()`)
- [ ] Expand primitive support (spheres, cylinders, capsules)
- [ ] Implement composite prefab system
- [ ] Add sprite billboard support for enemies

**Code Changes Needed**:
```cpp
// New methods in EntityFactory
Entity* EntityFactory::CreateModelGameObject(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);
    
    // Add mesh component configured for model loading
    auto mesh = entity->AddComponent<MeshComponent>();
    mesh->meshType = MeshComponent::MeshType::MODEL;
    mesh->meshName = std::any_cast<std::string>(definition.properties.at("model_path"));
    
    // Set up material from definition
    AddMaterialComponent(entity, definition);
    
    return entity;
}
```

### Direct Material Binding 🔴 Not Started
**Priority**: High  
**Files**: `src/world/EntityFactory.cpp`

**Tasks**:
- [ ] Bind textures directly using material_id from map
- [ ] Remove deferred texture resolution
- [ ] Add texture handle validation
- [ ] Implement fallback textures for missing assets

---

## Testing & Validation Plan

### Performance Benchmarks
- [ ] Create test scene with 1000+ entities
- [ ] Add FPS monitoring with 1% and 0.1% lows
- [ ] Implement GPU timing for draw calls
- [ ] Add memory usage tracking
- [ ] Create automated performance regression tests

### Quality Assurance  
- [ ] Verify visual consistency after optimizations
- [ ] Test on multiple hardware configurations
- [ ] Validate LOD transitions
- [ ] Check material/texture correctness
- [ ] Test instancing visual accuracy

### Debug Tools
- [ ] Add rendering statistics overlay
- [ ] Implement wireframe/debug view modes
- [ ] Add culling visualization
- [ ] Create batch analysis tools
- [ ] Add frame timing analysis

---

## Risk Assessment

### High Risk Items
1. **Model Caching**: Complex memory management, potential for leaks
2. **GPU Instancing**: Raylib version compatibility, driver issues
3. **BSP Integration**: Complex spatial data structures, potential for bugs

### Mitigation Strategies
1. **Incremental Implementation**: Small, testable changes
2. **Fallback Paths**: Keep old rendering paths during transition
3. **Extensive Testing**: Performance and visual validation at each step
4. **Profiling**: Continuous performance monitoring

---

## Success Metrics

### Performance Targets
- **Phase 1**: 45-55 FPS stable (Current: 30-50 FPS)
- **Phase 2**: 55-65 FPS stable  
- **Phase 3**: 60+ FPS stable with room for growth

### Technical Goals
- Reduce draw calls by 80-90%
- Implement proper frustum culling (60-90% object rejection)
- Cache 100% of static mesh data
- Support 1000+ entities with stable performance

### Code Quality
- Maintain ECS architecture principles
- Keep Raylib integration clean and optimal
- Preserve existing game functionality
- Add comprehensive debug/profiling tools

---

**Last Updated**: 2025-09-21  
**Next Review**: After Phase 1 completion