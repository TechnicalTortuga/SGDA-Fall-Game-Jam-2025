# BSP Rendering Optimization Tracker

## Overview
**Goal**: Optimize BSP rendering with PVS-based visibility determination and improved culling
**Current Status**: Phase 1 In Progress
**Performance Target**: 2-3x improvement in rendering performance for complex scenes

---

## Phase 1: Core PVS System Implementation
**Status**: 🟡 In Progress
**Target Completion**: 2025-10-01

### 1.1 PVS Data Structures
- [x] Define cluster structure for visibility sets
- [ ] Implement bit vector compression (RLE) - Currently using simple bit vector
- [ ] Add PVS serialization/deserialization

### 1.2 PVS Generation (Build Tool)
- [x] Create PVS generator for level compilation
- [x] Implement cluster-to-cluster visibility testing
- [ ] Add PVS compression

### 1.3 Integration with Renderer
- [x] Add PVS lookup in renderer (DebugDrawClusterPVS implemented)
- [x] Implement PVS-based cluster culling in TraverseForRenderingFaces
- [x] Combine with existing frustum culling (partial - frustum culling implemented but not PVS-based)

---

## Phase 2: BSP Tree Enhancements
**Status**: 🔴 Not Started
**Target Completion**: 2025-10-15

### 2.1 Leaf/Cluster System
- [x] Implement cluster-based leaf organization
- [x] Add cluster-based visibility queries (ContainsPoint uses tree traversal)
- [ ] Optimize leaf merging for better PVS efficiency

### 2.2 Detail Brush Support
- [ ] Add detail brush flag to BSP faces
- [ ] Modify BSP generation to skip splitting on detail brushes
- [ ] Add detail culling based on distance/LOD

---

## Phase 3: Advanced Features
**Status**: 🔴 Not Started
**Target Completion**: 2025-10-31

### 3.1 Area Portals
- [ ] Define area portal entity/component
- [ ] Implement dynamic portal activation
- [ ] Update PVS based on portal states

### 3.2 Occlusion Culling
- [ ] Add occluder entity type
- [ ] Implement software occlusion culling
- [ ] Add debug visualization

---

## Phase 4: Performance & Debugging
**Status**: 🔴 Not Started
**Target Completion**: 2025-11-07

### 4.1 Profiling
- [x] Add detailed render stats (batching stats in RenderSystem)
- [ ] Profile PVS lookup performance
- [ ] Optimize hot paths

### 4.2 Debug Tools
- [x] Add PVS visualization (DebugDrawClusterPVS implemented)
- [x] Implement cluster boundary rendering (DebugDrawAllClusterBounds implemented)
- [x] Add culling debug overlay (render stats displayed)

---

## Performance Metrics
| Metric | Before | Target | Current |
|--------|--------|--------|---------|
| FPS in Complex Scene | 30-40 | 60+ | TBD |
| CPU Time per Frame | XXms | <8ms | TBD |
| Visible Faces | X,XXX | 50% reduction | TBD |
| Memory Usage | XX MB | <10% increase | TBD |

---

## Implementation Notes
- **PVS Format**: Using RLE compression for visibility data
- **Cluster Size**: Target 8-16 leaves per cluster
- **Memory Budget**: Keep PVS data under 2MB per level

## Related Issues
- #123 - BSP Culling Too Aggressive
- #124 - Missing Textures in BSP Rendering

## References
- [Quake 3 BSP Format](https://github.com/id-Software/Quake-3-Archive)
- [Source Engine PVS Documentation](https://developer.valvesoftware.com/wiki/PVS)
- [BSP Tree Optimization Guide](http://www.cs.utah.edu/~jsnider/SeniorProj/BSP/)
