# BSP Production Refactor Roadmap

## Executive Summary

This document outlines the complete architectural transformation from the current runtime BSP compilation approach to a proper production-ready BSP pipeline following Quake/Source engine principles. The current implementation builds BSP trees and generates PVS data at runtime from YAML files, which is fundamentally flawed for game development.

## Current Architecture Problems

### ❌ Runtime Compilation Issues
- BSP trees built every game launch from source YAML files
- PVS generation happens during gameplay startup
- Source format data remains in memory alongside compiled structures
- No precomputed optimizations or validation
- Unacceptable performance for serious game development

### ❌ Cluster Management Issues
- Each BSP leaf becomes its own cluster (too granular)
- Excessive PVS computation complexity (O(n²) where n = leaf count)
- Memory waste from over-segmentation
- Poor cache performance

### ❌ Visibility System Issues
- PVS uses only AABB intersection testing (too conservative)
- No actual line-of-sight calculations
- Renders significantly more geometry than necessary
- No PVS compression (high memory usage)

## Target Architecture (Quake/Source Model)

### ✅ Precomputation Pipeline
```
YAML Map → BSP Compiler Tool → Binary BSP File → Game Runtime → Fast Loading
     ↓              ↓                     ↓              ↓
Source Format  Precompute BSP,        Optimized binary  Load & render
Human-editable PVS, Lighting, UVs     format with all   precomputed data
               Line-of-sight testing  precomputed data
```

### ✅ Production Benefits
- **Performance**: Fast loading, optimized rendering
- **Scalability**: Handles complex maps efficiently
- **Memory**: Only optimized data in memory
- **Workflow**: Edit → Compile → Test cycle
- **Debugging**: Precomputed optimization validation

## Phase 1: Immediate Runtime Fixes (Week 1-2)

### 1.1 Fix Cluster Management
**Current**: Each leaf = 1 cluster (1,000+ clusters for complex maps)
**Target**: Max 64 leaves per cluster (15-20 clusters for complex maps)
**Quake Reference**: Uses portal-based clustering in `q3map/vis.c`

#### Implementation Steps:
1. **Modify BuildClusters()** in `BSPTree.cpp`:
   ```cpp
   // Based on Quake 3's portal-based clustering approach
   // Instead of: each leaf becomes a cluster
   // New approach: merge leaves into clusters of max 64 leaves
   void BSPTree::BuildClusters() {
       const int32_t MAX_LEAVES_PER_CLUSTER = 64;

       // Collect all leaf nodes (similar to Quake's leaf collection)
       std::vector<BSPNode*> leaves = CollectLeafNodes();

       // Merge leaves into clusters using spatial proximity
       // Quake uses portals between leaves to determine cluster boundaries
       for (size_t i = 0; i < leaves.size(); i += MAX_LEAVES_PER_CLUSTER) {
           BSPCluster cluster;
           cluster.id = static_cast<int32_t>(clusters_.size());

           size_t endIdx = std::min(i + MAX_LEAVES_PER_CLUSTER, leaves.size());
           for (size_t j = i; j < endIdx; ++j) {
               cluster.leafNodes.push_back(leaves[j]);
               leaves[j]->clusterId = cluster.id;
           }

           // Compute cluster bounds from all leaf bounds
           cluster.bounds = ComputeClusterBounds(cluster.leafNodes);

           // Generate visibility points (8 corners + center, like Quake)
           cluster.visibilityPoints = GenerateVisibilityPoints(cluster);

           clusters_.push_back(cluster);
       }
   }

   // Generate visibility test points (based on Quake 3's approach)
   std::vector<Vector3> BSPTree::GenerateVisibilityPoints(const BSPCluster& cluster) {
       std::vector<Vector3> points;

       // Add all 8 corners of cluster AABB (Quake uses corner testing)
       const Vector3& min = cluster.bounds.min;
       const Vector3& max = cluster.bounds.max;
       points.push_back({min.x, min.y, min.z});
       points.push_back({max.x, min.y, min.z});
       points.push_back({min.x, max.y, min.z});
       points.push_back({max.x, max.y, min.z});
       points.push_back({min.x, min.y, max.z});
       points.push_back({max.x, min.y, max.z});
       points.push_back({min.x, max.y, max.z});
       points.push_back({max.x, max.y, max.z});

       // Add cluster center for additional testing
       points.push_back(cluster.bounds.GetCenter());

       return points;
   }
   ```

2. **Add Cluster Merging Logic**:
   ```cpp
   AABB BSPTree::ComputeClusterBounds(const std::vector<BSPNode*>& leafNodes) {
       AABB bounds;
       for (const auto* leaf : leafNodes) {
           bounds.Encapsulate(leaf->bounds);
       }
       return bounds;
   }

   std::vector<Vector3> BSPTree::GenerateVisibilityPoints(const BSPCluster& cluster) {
       std::vector<Vector3> points;

       // Add all 8 corners of cluster AABB
       const Vector3& min = cluster.bounds.min;
       const Vector3& max = cluster.bounds.max;
       points.push_back({min.x, min.y, min.z});
       points.push_back({max.x, min.y, min.z});
       // ... add all 8 corners

       // Add cluster center
       points.push_back(cluster.bounds.GetCenter());

       return points;
   }
   ```

### 1.2 Implement Proper Line-of-Sight PVS Generation
**Current**: AABB intersection only (renders ~80-90% of geometry)
**Target**: Actual visibility testing (renders ~20-40% of geometry)
**Quake Reference**: Portal-based flood fill in `q3map/vis.c::CalcVis()`

#### Quake 3 PVS Algorithm Overview:
1. **Portal Generation**: Create portals between adjacent BSP leaves
2. **Base Portal Visibility**: `BasePortalVis()` - determine which portals are visible from each portal
3. **Portal Flow**: `PortalFlow()` - flood fill visibility through portal connections
4. **Cluster Merging**: `ClusterMerge()` - compress portal visibility into cluster visibility

#### Implementation Steps:
1. **Create Portal-Based Line-of-Sight Testing** (Based on Quake 3's approach):
   ```cpp
   // Based on Quake 3's portal system (q3map/portals.c, vis.c)
   struct Portal {
       Plane plane;              // Portal plane
       std::vector<Vector3> points; // Portal winding points
       int32_t clusterA, clusterB;   // Connected clusters
       bool mightSee;            // Can potentially see through
   };

   bool BSPTree::TestLineOfSight(const Vector3& start, const Vector3& end) const {
       // Cast ray through BSP tree (like Quake's ray casting)
       Vector3 direction = Vector3Normalize(Vector3Subtract(end, start));
       float distance = Vector3Length(Vector3Subtract(end, start));

       // Check for occlusion by BSP geometry using existing CastRay
       float hitDistance = CastRay(start, direction, distance);
       return hitDistance >= distance; // No hit means clear line of sight
   }
   ```

2. **Implement Portal-Based PVS Generation** (Inspired by Quake 3's `CalcVis()`):
   ```cpp
   void BSPTree::GeneratePVSData() {
       // Step 1: Generate portals between adjacent clusters
       std::vector<Portal> portals = GenerateClusterPortals();

       // Step 2: Calculate base portal visibility (like Quake's BasePortalVis)
       CalculateBasePortalVisibility(portals);

       // Step 3: Flood fill visibility through portals (like Quake's PortalFlow)
       FloodFillPortalVisibility(portals);

       // Step 4: Compress portal visibility into cluster PVS
       CompressPortalToClusterVisibility(portals);
   }

   // Generate portals between adjacent clusters (simplified from Quake)
   std::vector<Portal> BSPTree::GenerateClusterPortals() {
       std::vector<Portal> portals;

       // For each pair of adjacent clusters, create portals
       // In full Quake implementation, this analyzes BSP face adjacencies
       for (size_t i = 0; i < clusters_.size(); ++i) {
           for (size_t j = i + 1; j < clusters_.size(); ++j) {
               if (AreClustersAdjacent(clusters_[i], clusters_[j])) {
                   Portal portal = CreatePortalBetweenClusters(clusters_[i], clusters_[j]);
                   portals.push_back(portal);
               }
           }
       }

       return portals;
   }

   bool BSPTree::TestClusterVisibility(int32_t clusterA, int32_t clusterB) const {
       if (clusterA == clusterB) return true;

       const auto& clusterFrom = clusters_[clusterA];
       const auto& clusterTo = clusters_[clusterB];

       // Test multiple visibility points (like Quake's corner testing)
       for (const Vector3& pointA : clusterFrom.visibilityPoints) {
           for (const Vector3& pointB : clusterTo.visibilityPoints) {
               if (TestLineOfSight(pointA, pointB)) {
                   return true; // Found clear line of sight
               }
           }
       }
       return false;
   }
   ```

3. **Add Portal Flood Fill Algorithm** (Based on Quake 3's PortalFlow):
   ```cpp
   // Simplified version of Quake 3's portal flood fill
   void BSPTree::FloodFillPortalVisibility(std::vector<Portal>& portals) {
       // Mark all portals as potentially visible initially
       for (auto& portal : portals) {
           portal.mightSee = true;
       }

       // For each portal, flood fill to find actually visible portals
       for (size_t i = 0; i < portals.size(); ++i) {
           FloodFillFromPortal(portals, i);
       }
   }

   void BSPTree::FloodFillFromPortal(std::vector<Portal>& portals, size_t startPortal) {
       std::vector<bool> visited(portals.size(), false);
       std::queue<size_t> portalQueue;

       portalQueue.push(startPortal);
       visited[startPortal] = true;

       while (!portalQueue.empty()) {
           size_t currentIdx = portalQueue.front();
           portalQueue.pop();

           const Portal& current = portals[currentIdx];

           // Test visibility to other portals
           for (size_t j = 0; j < portals.size(); ++j) {
               if (!visited[j] && CanSeeThroughPortals(current, portals[j])) {
                   visited[j] = true;
                   portalQueue.push(j);
               }
           }
       }
   }
   ```

### 1.3 Implement PVS Compression
**Current**: Uncompressed bit vector (high memory usage)
**Target**: RLE compression (following Quake 3 format)
**Quake Reference**: Visibility lump compression in `common/bspfile.c`

#### Quake 3 PVS Compression Overview:
- **Format**: Run-Length Encoding (RLE) with visibility bytes
- **Storage**: `numClusters × ((numClusters + 7) / 8)` bits → compressed bytes
- **Location**: LUMP_VISIBILITY in BSP file (17 total lumps)

#### Implementation Steps:
1. **Add Compression Structures** (Based on Quake 3's format):
   ```cpp
   // Based on Quake 3's dvis_t structure and visibility lump
   struct CompressedPVSData {
       int32_t numClusters;
       int32_t bytesPerCluster;  // (numClusters + 7) / 8
       std::vector<uint8_t> compressedData;  // RLE compressed visibility data

       // Decompression methods (like Quake 3's DecompressVis)
       bool IsVisible(int32_t fromCluster, int32_t toCluster) const;
       std::vector<int32_t> GetVisibleClusters(int32_t clusterId) const;
       std::vector<uint8_t> DecompressCluster(int32_t clusterId) const;
   };
   ```

2. **Implement RLE Compression** (Following Quake 3's algorithm):
   ```cpp
   class PVSCompressor {
   public:
       // Compress visibility data (like Quake 3's CompressVis)
       static std::vector<uint8_t> Compress(const std::vector<uint8_t>& bitVector,
                                          int32_t numClusters);

       // Decompress visibility data for a cluster
       static std::vector<uint8_t> Decompress(const std::vector<uint8_t>& compressed,
                                            int32_t clusterId, int32_t numClusters);

   private:
       // Quake 3 style RLE: alternating runs of 0s and 1s
       // 0 = invisible (solid), 1 = visible (can see through)
       static constexpr uint8_t SOLID_RUN = 0x00;   // Run of invisible clusters
       static constexpr uint8_t VIS_RUN = 0xFF;     // Run of visible clusters
   };

   // Based on Quake 3's visibility decompression
   std::vector<uint8_t> CompressedPVSData::DecompressCluster(int32_t clusterId) const {
       std::vector<uint8_t> decompressed(bytesPerCluster, 0);

       // Decompress RLE data for this cluster (like Quake 3's DecompressVis)
       size_t compressedOffset = clusterId * GetCompressedSizeForCluster(clusterId);
       size_t outputOffset = 0;

       while (outputOffset < bytesPerCluster) {
           uint8_t runLength = compressedData[compressedOffset++];
           uint8_t runValue = compressedData[compressedOffset++];

           // Fill run in output
           for (uint8_t i = 0; i < runLength && outputOffset < bytesPerCluster; ++i) {
               decompressed[outputOffset++] = runValue;
           }
       }

       return decompressed;
   }
   ```

3. **Integration with BSP File Format**:
   ```cpp
   // Based on Quake 3's LUMP_VISIBILITY (lump 16)
   void BSPTree::WritePVSData(BinaryWriter& writer) const {
       // Write compressed PVS data to visibility lump
       CompressedPVSData compressedPVS = CompressPVSData();

       // Write PVS header
       writer.WriteInt32(compressedPVS.numClusters);
       writer.WriteInt32(compressedPVS.bytesPerCluster);

       // Write compressed visibility data
       writer.WriteBytes(compressedPVS.compressedData);
   }

   CompressedPVSData BSPTree::CompressPVSData() const {
       CompressedPVSData result;
       result.numClusters = GetClusterCount();
       result.bytesPerCluster = (result.numClusters + 7) / 8;

       // Compress each cluster's visibility data
       for (int32_t clusterId = 0; clusterId < result.numClusters; ++clusterId) {
           std::vector<uint8_t> clusterVis = GetUncompressedVisibilityForCluster(clusterId);
           std::vector<uint8_t> compressed = PVSCompressor::Compress(clusterVis, result.numClusters);
           result.compressedData.insert(result.compressedData.end(),
                                      compressed.begin(), compressed.end());
       }

       return result;
   }
   ```

4. **Memory Target**: <2MB compressed PVS data per level (matching Quake 3)

## Phase 2: BSP Compiler Tool Development (Week 3-4)

### 2.1 Create BSP Compiler Tool Structure
**Location**: `tools/bsp_compiler/`
**Quake Reference**: `q3map/` directory structure and command-line interface

#### Directory Structure (Based on Quake 3's q3map):
```
tools/bsp_compiler/
├── CMakeLists.txt
├── main.cpp                    # Compiler entry point (like q3map.c)
├── BSPCompiler.h/.cpp          # Main compiler class
├── MapLoader.h/.cpp           # YAML map parsing (vs Quake's .map parsing)
├── BSPBuilder.h/.cpp          # BSP tree construction (like tree.c)
├── PVSGenerator.h/.cpp        # Visibility computation (like vis.c)
├── BSPWriter.h/.cpp           # Binary file output (like writebsp.c)
├── Portals.h/.cpp             # Portal generation (like portals.c)
├── qbsp.h                     # Shared BSP definitions
└── tests/                      # Unit tests
```

#### Main Compiler Interface (Inspired by Quake 3's q3map):
```cpp
// Based on Quake 3's main compilation pipeline
class BSPCompiler {
public:
    bool Compile(const std::string& inputMapPath, const std::string& outputBSPPath);

private:
    // Stage 1: Map Loading (like Quake's Map_Load)
    bool LoadMap(const std::string& mapPath);

    // Stage 2: BSP Construction (like Quake's ProcessWorldModel)
    bool BuildBSP();

    // Stage 3: Portal Generation (like Quake's MakeTreePortals)
    bool GeneratePortals();

    // Stage 4: PVS Generation (like Quake's CalcVis)
    bool GeneratePVS();

    // Stage 5: BSP File Output (like Quake's WriteBSPFile)
    bool WriteBSPFile(const std::string& outputPath);

    // Data structures (similar to Quake's global structures)
    std::unique_ptr<MapData> mapData_;
    std::unique_ptr<BSPTree> bspTree_;
    std::vector<Portal> portals_;           // Like Quake's portal system
    std::unique_ptr<CompressedPVSData> pvsData_;
};
```

#### Command-Line Interface (Following Quake 3's q3map):
```bash
# Basic compilation (like q3map -convert input.map output.bsp)
./bsp_compiler -input level1.map -output level1.bsp

# With PVS options (like q3map -vis -fastvis)
./bsp_compiler -input level1.map -output level1.bsp \
               -vis \                    # Generate PVS
               -fastvis \               # Use fast approximate PVS
               -v \                     # Verbose output

# Multiple processing stages (like q3map -onlyents, -onlytextures)
./bsp_compiler -input level1.map -output level1.bsp \
               -onlyents \             # Only process entities
               -onlybsp \              # Only build BSP tree
               -onlyvis               # Only generate visibility
```

### 2.2 Binary BSP File Format
**Current**: No binary format (runtime compilation)
**Target**: Fast-loading binary format with precomputed data
**Quake Reference**: `common/qfiles.h` - Complete BSP file format specification

#### File Structure (Following Quake 3's dheader_t):
```cpp
// Based directly on Quake 3's dheader_t structure
struct BSPHeader {
    char magic[4];           // "IBSP" (big-endian, like Quake 3)
    int32_t version;         // 46 for Quake 3 compatibility
    lump_t lumps[HEADER_LUMPS]; // 17 lumps, like Quake 3
};

#define HEADER_LUMPS 17      // Same as Quake 3

// Lump structure (from Quake 3's lump_t)
typedef struct {
    int fileofs, filelen;    // Offset and size in bytes
} lump_t;
```

#### Lump Definitions (Matching Quake 3's LUMP_* constants):
```cpp
enum BSPLumpType {
    LUMP_ENTITIES = 0,      // Map entities (entity string)
    LUMP_SHADERS = 1,       // dshader_t array (materials)
    LUMP_PLANES = 2,        // dplane_t array (BSP split planes)
    LUMP_NODES = 3,         // dnode_t array (BSP tree nodes)
    LUMP_LEAFS = 4,         // dleaf_t array (BSP tree leaves)
    LUMP_LEAFSURFACES = 5,  // int array (surface indices per leaf)
    LUMP_LEAFBRUSHES = 6,   // int array (brush indices per leaf)
    LUMP_MODELS = 7,        // dmodel_t array (brush models)
    LUMP_BRUSHES = 8,       // dbrush_t array (brushes)
    LUMP_BRUSHSIDES = 9,    // dbrushside_t array (brush sides)
    LUMP_DRAWVERTS = 10,    // drawVert_t array (drawing vertices)
    LUMP_DRAWINDEXES = 11,  // int array (drawing indices)
    LUMP_SURFACES = 12,     // dsurface_t array (drawing surfaces)
    LUMP_LIGHTMAPS = 13,    // byte array (lightmap data)
    LUMP_LIGHTGRID = 14,    // byte array (light grid)
    LUMP_VISIBILITY = 15,   // byte array (compressed PVS data)
    LUMP_LIGHTARRAY = 16    // byte array (light array)
};
```

#### Key Data Structures (From Quake 3's qfiles.h):
```cpp
// BSP Tree Nodes (dnode_t)
typedef struct {
    int planeNum;         // Index into planes lump
    int children[2];      // Front/back child: negative = leaf + 1
    int mins[3], maxs[3]; // Bounds for frustum culling
} dnode_t;

// BSP Leaves (dleaf_t)
typedef struct {
    int cluster;          // PVS cluster, -1 = opaque
    int area;             // Area for portals
    int mins[3], maxs[3]; // Bounds for frustum culling
    int firstLeafSurface; // Index into LUMP_LEAFSURFACES
    int numLeafSurfaces;  // Number of surfaces in this leaf
    int firstLeafBrush;   // Index into LUMP_LEAFBRUSHES
    int numLeafBrushes;   // Number of brushes in this leaf
} dleaf_t;

// BSP Planes (dplane_t)
typedef struct {
    float normal[3];      // Plane normal
    float dist;           // Distance from origin
} dplane_t;
```

### 2.3 BSP Compiler Command-Line Interface
```bash
# Basic compilation
./bsp_compiler -input level1.map -output level1.bsp

# With options
./bsp_compiler -input level1.map -output level1.bsp \
               -verbose \
               -max_clusters 256 \
               -pvs_samples 16

# Batch compilation
./bsp_compiler -batch levels.txt -output_dir compiled_maps/
```

## Phase 3: Runtime BSP Loading System (Week 5-6)

### 3.1 Fast BSP Loading Architecture
**Current**: Build BSP from YAML every launch
**Target**: Load precomputed binary BSP file

#### Loading Pipeline:
```cpp
class BSPLoader {
public:
    std::unique_ptr<BSPTree> LoadBSP(const std::string& bspPath);

private:
    bool ValidateHeader(const BSPHeader& header);
    bool LoadBSPNodes(BinaryReader& reader, BSPTree& tree);
    bool LoadClusters(BinaryReader& reader, BSPTree& tree);
    bool LoadPVS(BinaryReader& reader, BSPTree& tree);
    bool LoadFaces(BinaryReader& reader, BSPTree& tree);
    bool LoadMaterials(BinaryReader& reader, BSPTree& tree);
};
```

### 3.2 Memory-Mapped Loading (Performance Optimization)
```cpp
class MemoryMappedBSP {
public:
    bool Load(const std::string& bspPath);

    // Direct access to precomputed data
    const BSPNode* GetNode(int32_t index) const;
    const BSPCluster* GetCluster(int32_t index) const;
    bool IsVisible(int32_t fromCluster, int32_t toCluster) const;

private:
    std::unique_ptr<MemoryMappedFile> mappedFile_;
    BSPHeader* header_;
    BSPNode* nodes_;
    BSPCluster* clusters_;
    uint8_t* pvsData_;
};
```

### 3.3 Runtime Integration
**Modify WorldSystem to load BSP files instead of building:**
```cpp
void WorldSystem::LoadWorldGeometry(const std::string& bspPath) {
    // Load precomputed BSP instead of building from YAML
    auto bspLoader = std::make_unique<BSPLoader>();
    worldGeometry_->bspTree = bspLoader->LoadBSP(bspPath);

    // BSP tree is now ready for rendering
    LOG_INFO("Loaded BSP: " + std::to_string(worldGeometry_->bspTree->GetClusterCount()) + " clusters");
}
```

## Phase 4: Advanced Features & Optimization (Week 7-8)

### 4.1 Area Portals (Dynamic Visibility)
```cpp
struct AreaPortal {
    int32_t clusterA, clusterB;  // Connected clusters
    AABB bounds;                 // Portal bounds
    bool isOpen;                 // Dynamic state

    // Portal-specific visibility testing
    bool CanSeeThrough() const;
};
```

### 4.2 Detail Brushes (LOD System)
```cpp
struct DetailBrush {
    AABB bounds;
    float maxVisibleDistance;  // Distance-based culling
    bool isDetail;             // Don't split BSP on detail brushes

    // LOD transitions
    void SetLODLevel(int level);
};
```

### 4.3 Hardware Occlusion Culling
```cpp
class HardwareOccluder {
public:
    void SetupOcclusionQueries(const Camera3D& camera);
    bool IsOccluded(const AABB& bounds) const;

private:
    std::vector<GLuint> occlusionQueries_;
    std::unordered_map<AABB, GLuint> queryCache_;
};
```

## Phase 5: Development Workflow & Tools (Week 9-10)

### 5.1 Build System Integration
**CMake Integration**:
```cmake
# tools/bsp_compiler/CMakeLists.txt
add_executable(bsp_compiler main.cpp BSPCompiler.cpp ...)
target_link_libraries(bsp_compiler yaml-cpp BSPTree PVSCompressor)

# Custom build target
add_custom_target(compile_maps
    COMMAND ${CMAKE_BINARY_DIR}/tools/bsp_compiler/bsp_compiler
            -input ${CMAKE_SOURCE_DIR}/assets/maps/level1.map
            -output ${CMAKE_BINARY_DIR}/assets/maps/level1.bsp
    DEPENDS bsp_compiler
)
```

### 5.2 Development Workflow Script
**build_maps.sh**:
```bash
#!/bin/bash
# Automated map compilation script

MAP_DIR="assets/maps"
OUTPUT_DIR="build/assets/maps"
COMPILER="./build/tools/bsp_compiler/bsp_compiler"

# Compile all .map files
for map_file in $MAP_DIR/*.map; do
    if [[ -f "$map_file" ]]; then
        filename=$(basename "$map_file" .map)
        output_file="$OUTPUT_DIR/$filename.bsp"

        echo "Compiling $map_file -> $output_file"
        $COMPILER -input "$map_file" -output "$output_file" -verbose

        if [[ $? -ne 0 ]]; then
            echo "ERROR: Failed to compile $map_file"
            exit 1
        fi
    fi
done

echo "All maps compiled successfully"
```

### 5.3 Debug Tools Enhancement
**Enhanced BSP Visualizer**:
```cpp
class BSPDebugger {
public:
    void VisualizePVS(const BSPTree& tree, int32_t clusterId);
    void ShowClusterConnectivity(const BSPTree& tree);
    void DisplayMemoryUsage(const BSPTree& tree);

    // Performance profiling
    void ProfilePVSLookup(const BSPTree& tree, const Camera3D& camera);
    void ProfileRendering(const BSPTree& tree, const std::vector<Face>& faces);
};
```

## Performance Targets & Metrics

### Phase 1 Targets (Immediate Runtime Fixes)
- **Cluster Count**: Reduce from 1000+ to <50 clusters for complex maps
- **PVS Accuracy**: Improve from 80-90% to 20-40% geometry rendered
- **Memory Usage**: <5MB PVS data (vs current uncompressed)
- **Loading Time**: No change (still runtime compilation)

### Phase 2-3 Targets (Production Pipeline)
- **Loading Time**: <2 seconds for complex maps (vs current 10-30 seconds)
- **Memory Usage**: <10MB total BSP data (optimized binary format)
- **Startup Performance**: 60+ FPS immediately (no compilation delay)
- **Build Time**: <30 seconds for complex maps

### Long-term Targets (Phase 4-5)
- **Frame Time**: <4ms BSP traversal + culling
- **Memory Budget**: <2MB compressed PVS, <8MB total BSP
- **Scalability**: Support 10,000+ faces efficiently
- **Development Speed**: Sub-second iteration time (edit → compile → test)

## Risk Mitigation

### Rollback Plan
- **Phase 1**: Can be implemented without breaking existing functionality
- **Phase 2-3**: Keep YAML loading as fallback during transition
- **Testing**: Extensive automated tests for BSP loading/rendering

### Compatibility
- **Save File Compatibility**: BSP files include version numbers
- **Map Format**: YAML maps remain source format
- **Runtime Fallback**: Can fall back to YAML loading if BSP loading fails

## Implementation Timeline

| Phase | Duration | Deliverables | Risk Level |
|-------|----------|--------------|------------|
| 1. Runtime Fixes | 1-2 weeks | Better clustering, PVS, compression | Low |
| 2. Compiler Tool | 2 weeks | BSP compiler executable | Medium |
| 3. Runtime Loading | 2 weeks | Binary BSP loading system | Medium |
| 4. Advanced Features | 2 weeks | Portals, LOD, occlusion | High |
| 5. Workflow Tools | 1 week | Build scripts, debug tools | Low |

## Success Criteria

### Functional Requirements
- [ ] BSP compiler successfully compiles YAML maps to binary format
- [ ] Runtime loads binary BSP files in <2 seconds
- [ ] PVS culling reduces rendered geometry by 60-80%
- [ ] Memory usage <10MB for complex maps
- [ ] Development workflow: edit → compile → test <30 seconds

### Performance Requirements
- [ ] 60+ FPS in complex scenes
- [ ] <4ms frame time for BSP operations
- [ ] <2MB compressed PVS data
- [ ] Startup time <5 seconds (including loading)

### Quality Requirements
- [ ] No visual artifacts from culling
- [ ] Consistent behavior with runtime-compiled version
- [ ] Comprehensive error handling and validation
- [ ] Full test coverage for critical paths

---

## Airstrafing Algorithm Preservation

**Important Note**: As requested, the airstrafing algorithm (bunny hopping mechanics) should be kept mostly one-to-one from the current implementation. This is a core gameplay feature that defines the "Counter-Strike" feel of "PaintStrike". The physics/movement system should remain largely unchanged during the BSP refactor.

---

## Next Steps

1. **Start with Phase 1**: Implement cluster merging and proper line-of-sight PVS
2. **Create BSP Compiler Foundation**: Set up tool structure and basic compilation
3. **Design Binary Format**: Define BSP file format specification
4. **Implement Loading System**: Create fast binary loading pipeline
5. **Add Development Tools**: Build scripts and debug visualization

## Key Quake 3 References Now Available

With the Quake source code cloned, you now have direct access to:
- **PVS Implementation**: `references/quake3_src/q3map/vis.c::CalcVis()`
- **BSP Construction**: `references/quake3_src/q3map/tree.c`
- **Binary Format**: `references/quake3_src/common/qfiles.h`
- **Compiler Tool**: `references/quake3_src/q3map/` directory

This roadmap transforms your BSP system from a game jam prototype into a production-ready implementation following industry standards established by id Software's Quake 3 engine.
