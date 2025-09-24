# Quake 3 BSP/PVS/Rendering Pipeline - Complete Analysis

## Executive Summary

The Quake 3 engine uses a **precomputed BSP tree with PVS data** that is **loaded from disk**, not constructed at runtime. The pipeline is:

1. **Compile-time (q3map)**: Build BSP tree, compute PVS, save to .bsp file
2. **Load-time**: Load precomputed BSP + PVS data from disk
3. **Runtime**: Use precomputed data for visibility culling and rendering

Our current implementation incorrectly tries to build BSP trees at runtime, which is fundamentally wrong.

## Key Files Analysis

### 1. BSP Compilation (q3map/*.c)

#### `q3map/bsp.c` - BSP Tree Construction
- **Function**: `ProcessWorldModel()` - Main BSP building function
- **Input**: Brushes from .map file
- **Output**: BSP tree with nodes and leaves
- **Key Data Structures**:
  ```c
  typedef struct node_s {
      int             planenum;       // Splitting plane
      struct node_s   *children[2];   // Front/back children
      int             firstface;      // Index into face array
      int             numfaces;       // Number of faces
  } node_t;
  ```

#### `q3map/vis.c` - PVS Generation
- **Function**: `CalcVis()` - Compute Potentially Visible Sets
- **Algorithm**: Flood fill through portals between leaves
- **Output**: Visibility data (bit arrays showing which leaves can see each other)

### 2. Runtime Loading (code/renderer/*.c)

#### `tr_bsp.c` - BSP Loading
- **Function**: `RE_LoadWorldMap()` - Load .bsp file
- **Data Loaded**:
  - BSP tree nodes/leaves
  - Face data (vertices, textures, etc.)
  - PVS data (precomputed visibility)
  - Lightmaps, entities, etc.

- **Key Structures**:
  ```c
  typedef struct world_s {
      char            name[MAX_QPATH];
      int             numShaders;
      dshader_t       *shaders;
      int             numModels;
      bmodel_t        *models;
      int             numplanes;
      cplane_t        *planes;
      int             numnodes;
      mnode_t         *nodes;         // BSP tree
      int             numsurfaces;
      msurface_t      *surfaces;      // All faces
      int             nummarksurfaces;
      msurface_t      **marksurfaces; // References to visible surfaces
      int             numClusters;
      int             clusterBytes;
      byte            *vis;           // PVS data
  } world_t;
  ```

#### `tr_world.c` - Runtime Rendering

##### `R_MarkLeaves()` - Visibility Determination
```c
static void R_MarkLeaves (void) {
    // Get current cluster from camera position
    leaf = R_PointInLeaf(tr.viewParms.pvsOrigin);
    cluster = leaf->cluster;

    // Get PVS for this cluster
    vis = R_ClusterPVS(cluster);

    // Mark all nodes that are visible from this cluster
    for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
        cluster = leaf->cluster;
        if (cluster >= 0 && cluster < tr.world->numClusters) {
            if (vis[cluster>>3] & (1<<(cluster&7))) {
                // Mark path from this leaf to root
                parent = leaf;
                do {
                    if (parent->visframe == tr.visCount) break;
                    parent->visframe = tr.visCount;
                    parent = parent->parent;
                } while (parent);
            }
        }
    }
}
```

##### `R_RecursiveWorldNode()` - BSP Traversal & Rendering
```c
static void R_RecursiveWorldNode(mnode_t *node, int planeBits, int dlightBits) {
    do {
        // PVS culling first
        if (node->visframe != tr.visCount) return;

        // Frustum culling
        if (!r_nocull->integer) {
            r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
            if (r == 2) return; // Outside frustum
        }

        if (node->contents != -1) break; // Leaf node

        // Recurse to children
        R_RecursiveWorldNode(node->children[0], planeBits, newDlights[0]);
        node = node->children[1];
    } while (1);

    // Leaf: Add all surfaces in this leaf
    for (c = 0 ; c < node->nummarksurfaces ; c++) {
        surf = node->firstmarksurface[c];
        // Add surface to render list
    }
}
```

## Critical Insights for Our Implementation

### 1. **Precomputed vs Runtime Construction**
- **Quake**: BSP tree and PVS are precomputed and stored in .bsp file
- **Our current code**: Tries to build BSP tree at runtime from faces ❌
- **Correct approach**: Build BSP tree once, save/load it, or build it properly at load time

### 2. **Face Storage Model**
- **Quake**: Faces are stored in `world_t->surfaces[]` array, leaves reference them via `firstmarksurface[]`
- **Our current code**: Stores faces directly in BSP nodes ❌
- **Correct approach**: Store all faces in a global array, have leaves reference them

### 3. **PVS Data Structure**
- **Quake**: PVS is a byte array where `vis[cluster>>3] & (1<<(cluster&7))` tests visibility
- **Our current code**: Uses bit arrays but implementation is complex ❌
- **Correct approach**: Use Quake's simple byte array approach

### 4. **Node Structure**
- **Quake**: `mnode_t` has both node and leaf data in union-style structure
- **Our current code**: Separate BSPNode class ❌
- **Correct approach**: Use similar unified structure or proper inheritance

## Correct Implementation Plan

### Phase 1: Data Structures (HIGH PRIORITY)
1. **World Structure**:
   ```cpp
   struct World {
       std::vector<Face> surfaces;           // All faces
       std::vector<BSPNode> nodes;           // BSP tree
       std::vector<uint8_t> visData;         // PVS data
       int numClusters;
       int clusterBytes;
   };
   ```

2. **BSP Node Structure**:
   ```cpp
   struct BSPNode {
       int contents;              // -1 for nodes, leaf contents for leaves
       int visframe;              // Visibility frame
       Vector3 mins, maxs;        // Bounds
       BSPNode* parent;
       BSPNode* children[2];      // Node specific
       int cluster;               // Leaf specific
       int area;
       std::vector<int> surfaceIndices; // Indices into world->surfaces
   };
   ```

### Phase 2: BSP Construction (HIGH PRIORITY)
1. **Input**: Faces from map data (already parsed correctly)
2. **Process**: Build BSP tree using standard algorithm
3. **Output**: BSP tree with all faces properly distributed to leaves

### Phase 3: PVS Generation (MEDIUM PRIORITY)
1. **Input**: BSP tree with leaves
2. **Process**: Generate visibility data between leaf clusters
3. **Output**: PVS byte array

### Phase 4: Runtime Rendering (HIGH PRIORITY)
1. **MarkLeaves**: Mark visible nodes using PVS
2. **RecursiveWorldNode**: Traverse marked nodes, frustum cull, render surfaces

## Key Fixes Needed

### 1. **Fix Face Storage**
- Move all faces to `world.surfaces` vector
- Have BSP leaves store indices into this vector
- Remove faces from internal BSP nodes

### 2. **Fix BSP Construction**
- Ensure all faces end up in leaves (not lost during splitting)
- Handle coplanar faces correctly (add to one side, not both)
- Ensure proper tree structure

### 3. **Fix PVS Implementation**
- Use simple byte array like Quake
- Simplify cluster generation
- Fix visibility marking

### 4. **Fix Rendering Pipeline**
- Implement proper `R_MarkLeaves` equivalent
- Implement proper `R_RecursiveWorldNode` equivalent
- Ensure faces are rendered from the global surface array

## Current Problems Identified

1. **Face Loss**: 127 faces → 55 leaves = 72 faces lost during BSP construction
2. **Wrong Face Storage**: Faces stored in BSP nodes instead of global array
3. **Complex PVS**: Over-engineered PVS system instead of simple Quake approach
4. **Runtime BSP Building**: Trying to build BSP at runtime instead of load-time

## Immediate Action Items

1. **Audit BSP Construction**: Add logging to track where faces go during splitting
2. **Fix Face Storage**: Move faces to global array, reference by index
3. **Simplify PVS**: Implement Quake-style byte array PVS
4. **Fix Rendering**: Implement proper MarkLeaves + RecursiveWorldNode

This analysis shows that our current approach is fundamentally flawed. We need to implement the Quake pipeline correctly, not patch the broken runtime BSP construction approach.

