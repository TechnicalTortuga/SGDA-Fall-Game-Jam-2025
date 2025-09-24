# World Geometry Rendering Pipeline

## Overview
This document details the complete pipeline from map parsing to BSP tree creation, PVS generation, and final rendering in the PaintStrike game engine.

## ⚠️ ARCHITECTURAL ISSUE: Runtime BSP Compilation

**CRITICAL ISSUE**: The current implementation builds BSP trees and generates PVS data at runtime from YAML map files. This is inefficient and not how professional game engines work.

### Traditional Engine Pipeline (Source/Quake)
```
Map Editor (.vmf) → BSP Compiler Tool → Binary .bsp file → Game Runtime (fast loading)
     ↓                ↓                     ↓                    ↓
  Source Format   Precompute BSP,       Optimized binary     Load & render
  Human-editable  PVS, Lighting         format with all      precomputed data
                  UVs, etc.             precomputed data
```

### Current Implementation (Incorrect)
```
YAML Map File → Runtime Parsing → Runtime BSP Build → Runtime PVS Gen → Rendering
     ↓                ↓                     ↓                    ↓
  Still source       Parse YAML          Build BSP tree      Generate visibility
  format, not        every launch         every launch         every launch
  optimized
```

### Problems with Current Approach
- **Slow loading**: BSP compilation happens every launch
- **Runtime overhead**: PVS generation blocks game startup
- **Memory waste**: Source format data kept in memory
- **Not scalable**: Complex maps become unusable
- **Debugging difficulty**: No precomputed optimization validation

### Correct Implementation Needed
1. **BSP Compiler Tool**: Command-line tool that compiles YAML → .bsp
2. **BSP File Format**: Binary format with precomputed data
3. **Runtime Loader**: Fast loading of precomputed .bsp files
4. **Development Workflow**: Edit YAML → Compile → Test

### Game Jam Mitigation
For the game jam deadline, runtime compilation is acceptable as a proof-of-concept, but this architecture must be fixed for production use.

## Pipeline Stages

### 1. Map Parsing & Loading
**Location**: `src/world/MapLoader.cpp`, `src/ecs/Systems/WorldSystem.cpp`

**Process**:
1. **Map Format Parsing**: Reads YAML-based map files (`.map` files)
   - Parses entities, brushes, faces, materials, and textures
   - Validates map structure and dependencies

2. **Asset Loading**: `src/ecs/Systems/AssetSystem.cpp`
   - Loads textures referenced by materials
   - Loads skybox cubemaps
   - Caches assets for runtime use

3. **MapData Structure Creation**:
   ```cpp
   struct MapData {
       std::vector<EntityData> entities;
       std::vector<Brush> brushes;
       std::vector<Face> faces;
       std::vector<MaterialData> materials;
       SkyboxData skybox;
   };
   ```

**Key Files**:
- `assets/maps/test_level.map` - Source map file
- `src/world/MapLoader.cpp` - YAML parsing logic

### 2. World Geometry Construction
**Location**: `src/world/WorldGeometry.cpp`, `src/ecs/Systems/WorldSystem.cpp`

**Process**:
1. **Material Resolution**: Convert material references to entity IDs
2. **Face Processing**: Calculate UV coordinates for all faces
3. **Batch Creation**: Group faces by material for efficient rendering
4. **BSP Tree Building**: Call `BuildBSPFromFaces()`

**Key Methods**:
```cpp
void WorldSystem::BuildWorldGeometry(const MapData& mapData)
void WorldGeometry::BuildBSPFromFaces(const std::vector<Face>& faces)
void WorldGeometry::CalculateFaceUVs(Face& face)
```

### 3. BSP Tree Construction
**Location**: `src/world/BSPTree.cpp`

**Process**:
1. **Face Collection**: Store all faces in `allFaces_` vector
2. **Recursive BSP Building**:
   - Select best split plane from face planes
   - Partition faces into front/back sets
   - Create child nodes recursively
   - Store faces in leaf nodes

3. **Tree Structure**:
   ```cpp
   struct BSPNode {
       Plane planeNormal;     // Split plane
       std::unique_ptr<BSPNode> front, back;
       std::vector<Face> faces;  // Only in leaf nodes
       AABB bounds;
       int32_t clusterId;     // -1 for internal nodes
   };
   ```

**Key Methods**:
```cpp
void BSPTree::BuildFromFaces(const std::vector<Face>& faces)
BSPNode* BSPTree::BuildRecursiveFaces(const std::vector<Face>& faces, int depth)
Plane BSPTree::ChooseSplitPlane(const std::vector<Face>& faces)
```

### 4. Leaf Node & Cluster Creation
**Location**: `src/world/BSPTree.cpp`

**Process**:
1. **Leaf Identification**: Nodes with no children are leaves
2. **Cluster Assignment**: Each leaf gets a unique cluster ID
3. **Bounds Calculation**: Compute AABB for each cluster
4. **Visibility Points**: Generate points for PVS calculations

**Data Structures**:
```cpp
struct BSPCluster {
    int32_t id;
    AABB bounds;
    std::vector<BSPNode*> leafNodes;
    std::vector<Vector3> visibilityPoints;
};
```

**Key Methods**:
```cpp
void BSPTree::AssignClustersToLeaves()
void BSPTree::CreateClusters()
```

### 5. PVS (Potentially Visible Set) Generation
**Location**: `src/world/BSPTree.cpp`

**Process**:
1. **Visibility Testing**: For each cluster pair, test if they can see each other
   - Cast rays between visibility points
   - Check for occlusion by BSP geometry

2. **Data Compression**: Store visibility in bit vector format
   ```cpp
   struct PVSData {
       int32_t numClusters;
       std::vector<uint8_t> visibilityData;  // Compressed bit vector
   };
   ```

3. **Memory Layout**: `numClusters × numClusters` bits, 8 bits per byte

**Key Methods**:
```cpp
void BSPTree::GeneratePVS()
void BSPTree::GeneratePVSData()
bool BSPTree::AreClustersVisible(int32_t clusterA, int32_t clusterB)
```

### 6. Runtime Rendering Pipeline
**Location**: `src/ecs/Systems/RenderSystem.cpp`, `src/rendering/Renderer.cpp`

**Process**:
1. **Camera Position Query**: Determine which cluster contains the camera
2. **PVS Lookup**: Get list of visible clusters from camera cluster
3. **BSP Traversal**: Only visit nodes belonging to visible clusters
4. **Frustum Culling**: Additional culling within visible clusters
5. **Face Rendering**: Render visible faces with material batching

**Rendering Order**:
```
BeginMode3D(camera)
├── RenderSkybox()           // Never culled
├── RenderBSPGeometry()      // PVS + frustum culled
│   └── TraverseForRenderingFaces()
│       ├── FindClusterForPoint(camera.position)
│       ├── GetVisibleClusters(cameraCluster)
│       └── Traverse only visible cluster nodes
└── RenderPVSDebug()         // Optional debug visualization
EndMode3D()
```

**Key Methods**:
```cpp
void RenderSystem::RenderWorldGeometryDirect()
void Renderer::RenderWorldGeometry()
void BSPTree::TraverseForRenderingFaces(const Camera3D& camera, std::vector<const Face*>& visibleFaces)
int32_t BSPTree::FindClusterForPoint(const Vector3& point)
std::vector<int32_t> BSPTree::GetVisibleClusters(int32_t clusterId)
```

### 7. Material & Texture Management
**Location**: `src/ecs/Components/MaterialComponent.h`, `src/rendering/Renderer.cpp`

**Process**:
1. **Material Caching**: Cache MaterialComponent pointers to avoid entity lookups
2. **Texture Binding**: Bind appropriate textures for each face
3. **State Management**: Minimize OpenGL state changes

**Key Features**:
- Material component caching per frame
- Texture ID to MaterialComponent mapping
- Default material fallbacks

### 8. Debug & Profiling
**Location**: `src/rendering/Renderer.cpp`, `src/world/BSPTree.cpp`

**Features**:
- PVS visualization (F3 to toggle)
- Cluster boundary rendering
- Culling statistics display
- Performance metrics (faces rendered, batches, etc.)

## Performance Optimizations

### BSP Tree Benefits
- **Spatial Partitioning**: O(log n) face queries vs O(n)
- **Hierarchical Culling**: Early rejection of subtrees
- **Cache Coherence**: Related geometry grouped together

### PVS Benefits
- **Visibility Culling**: Only render potentially visible geometry
- **Overdraw Reduction**: Significant frame time improvement
- **Scalability**: Performance independent of total geometry count

### Material Batching
- **State Change Reduction**: Group faces by material
- **Draw Call Batching**: Single draw call per material group
- **GPU Efficiency**: Minimize texture binding overhead

## Common Issues & Debugging

### No Geometry Visible
1. **Check PVS Generation**: Ensure clusters and PVS data exist
2. **Camera Cluster Detection**: Verify `FindClusterForPoint()` works
3. **PVS Traversal**: Check if visible clusters are found
4. **Frustum Culling**: Ensure camera frustum is properly set

### Performance Issues
1. **Too Many Clusters**: Can increase PVS computation time
2. **PVS Memory Usage**: Monitor memory consumption
3. **Traversal Depth**: Deep BSP trees can hurt cache performance

### Debug Tools
- Press F3: Toggle PVS visualization
- Check console logs for cluster counts and visibility stats
- Monitor render statistics overlay

## File Dependencies

```
MapLoader.cpp ──┐
                ├──► WorldSystem.cpp ──┐
AssetSystem.cpp ─┘                    ├──► WorldGeometry.cpp ──┐
                                      │                        ├──► BSPTree.cpp ──┐
                                      │                        │                  ├──► Renderer.cpp
                                      │                        │                  └──► RenderSystem.cpp
                                      └──► EntityFactory.cpp ──┘
```

## Configuration Options

- `worldRenderingEnabled_`: Master toggle for world geometry
- `enableFrustumCulling_`: Additional frustum culling on top of PVS
- `showPVSDebug_`: Enable PVS debug visualization
- `wireframeMode_`: Render geometry in wireframe mode

## Future Enhancements

1. **PVS Compression**: Implement RLE compression for memory efficiency
2. **Dynamic PVS**: Update PVS when doors/portals change
3. **Area Portals**: Support for dynamic visibility changes
4. **LOD Integration**: Distance-based detail reduction
5. **Occlusion Culling**: Hardware-accelerated occlusion queries
