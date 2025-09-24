# Quake-Style BSP Rendering Pipeline - Implementation Gameplan

## Executive Summary

After analyzing Quake 3's renderer code, our current BSP implementation has fundamental architectural flaws. We need to refactor to follow Quake's proven approach: **PVS marking first, then recursive node traversal with hierarchical frustum culling**.

## Current Problems

1. **Wrong Order of Operations**: We try to find camera cluster first, then traverse faces - Quake marks entire subtrees visible first
2. **Missing PVS Marking**: No system to mark nodes as potentially visible before traversal
3. **Face-Level Culling**: We cull individual faces instead of node bounds
4. **Breadth-First Traversal**: We collect all leaves first instead of recursive tree traversal
5. **Degenerate Bounds**: Leaf bounds are computed from faces only, not spatial partitioning

## Quake 3 Rendering Pipeline Analysis

### Phase 1: PVS Visibility Determination (`R_MarkLeaves`)

```c
void R_AddWorldSurfaces(void) {
    // 1. Mark potentially visible leaves based on PVS
    R_MarkLeaves();

    // 2. Traverse BSP with frustum culling
    R_RecursiveWorldNode(tr.world->nodes, planeBits, dlightBits);
}
```

**Key Insights:**
- **PVS Happens FIRST**: Before any rendering traversal
- **Tree Marking**: Marks entire subtrees as visible based on cluster visibility
- **Visframe System**: Each node has a `visframe` counter to track visibility per frame

### Phase 2: Recursive BSP Traversal (`R_RecursiveWorldNode`)

```c
static void R_RecursiveWorldNode(mnode_t *node, int planeBits, int dlightBits) {
    do {
        // 1. PVS Check FIRST
        if (node->visframe != tr.visCount) return;

        // 2. Frustum Cull Node Bounds
        if (!r_nocull->integer) {
            // Test node AABB against 4 frustum planes
            r = BoxOnPlaneSide(node->mins, node->maxs, frustum_plane);
            if (r == 2) return; // Outside frustum
        }

        // 3. If leaf, add surfaces
        if (node->contents != -1) {
            for each surface in leaf:
                R_AddWorldSurface(surface, dlightBits);
            break;
        }

        // 4. Recurse to children
        R_RecursiveWorldNode(node->children[0]);
        node = node->children[1]; // Tail recursion
    } while (1);
}
```

**Key Insights:**
- **Hierarchical Culling**: Cull at node level, not face level
- **Depth-First Traversal**: Follow tree structure recursively
- **Early Termination**: Skip entire subtrees not in PVS
- **Efficient Recursion**: Tail recursion for back children

## Our Implementation Plan

### Phase 1: Add Visframe System ✅

**Status**: Completed
- Added `visframe` field to `BSPNode`
- Added `visCount_` counter to `BSPTree`
- Added `MarkLeaves()` method for PVS marking

### Phase 2: Implement PVS-First Marking

**Status**: In Progress

**Implementation:**
```cpp
void BSPTree::MarkLeaves(const Vector3& cameraPosition) {
    visCount_++;  // Increment frame counter

    // Find camera leaf (like R_PointInLeaf)
    const BSPNode* cameraLeaf = FindLeafForPoint(cameraPosition);

    // Get camera cluster
    int32_t cameraCluster = cameraLeaf->clusterId;

    // Mark all leaves in visible clusters
    for (int32_t clusterId : GetVisibleClusters(cameraCluster)) {
        for (BSPNode* leaf : clusters_[clusterId].leafNodes) {
            leaf->visframe = visCount_;
        }
    }
}
```

**Key Changes:**
- Add `FindLeafForPoint()` - proper tree traversal to find containing leaf
- Use existing `GetVisibleClusters()` for PVS lookup
- Mark leaf nodes directly (not entire subtrees for now)

### Phase 3: Recursive Node Traversal with Frustum Culling

**Status**: Pending

**Implementation:**
```cpp
void BSPTree::TraverseForRendering(const Camera3D& camera,
                                  std::function<void(const Face& face)> callback) {
    if (!root_) return;

    // Start recursive traversal from root
    TraverseNodeRecursive(root_.get(), camera, callback);
}

void BSPTree::TraverseNodeRecursive(const BSPNode* node, const Camera3D& camera,
                                   std::function<void(const Face& face)> callback) {
    // 1. PVS Check FIRST
    if (node->visframe != visCount_) return;

    // 2. Frustum cull node bounds
    if (!IsAABBInViewFrustum(node->bounds, camera)) return;

    // 3. If leaf, process faces
    if (node->IsLeaf()) {
        for (const Face& face : node->faces) {
            if (IsFaceVisibleForRendering(face, camera)) {
                callback(face);
            }
        }
        return;
    }

    // 4. Recurse to children
    if (node->front) TraverseNodeRecursive(node->front.get(), camera, callback);
    if (node->back) TraverseNodeRecursive(node->back.get(), camera, callback);
}
```

**Key Changes:**
- Replace breadth-first with depth-first recursive traversal
- Move frustum culling to node bounds level
- Keep face-level visibility checks for backface culling

### Phase 4: Update Renderer Integration

**Status**: Pending

**Renderer Changes:**
```cpp
void Renderer::RenderBSPGeometry() {
    // 1. Mark visible leaves first (PVS)
    worldGeometry_->bspTree->MarkLeaves(camera_.position);

    // 2. Traverse for rendering with frustum culling
    visibleFaces.clear();
    worldGeometry_->bspTree->TraverseForRendering(camera_,
        [this](const Face& face) {
            visibleFaces.push_back(&face);
        });

    // 3. Render visible faces
    for (const Face* face : visibleFaces) {
        RenderFace(*face);
    }
}
```

**Benefits:**
- Correct order: PVS → Frustum Culling → Face Processing
- Hierarchical culling prevents unnecessary work
- Matches Quake's proven architecture

## Critical Implementation Details

### 1. Visframe System
- Each `BSPNode` has `visframe` field
- `BSPTree` has `visCount_` counter
- Nodes marked with current `visCount_` are visible
- Prevents re-processing nodes across frames

### 2. Node Bounds Computation
**Current Problem:** Leaf bounds computed only from contained faces
**Solution:** Node bounds must encompass entire subtree

```cpp
// In BuildRecursiveFaces
node->bounds = ComputeBoundsForFaces(node->faces);
if (node->front) node->bounds.Encapsulate(node->front->bounds);
if (node->back)  node->bounds.Encapsulate(node->back->bounds);
```

### 3. Frustum Culling at Node Level
- Use `BoxOnPlaneSide()` equivalent for AABB vs frustum planes
- Test against all 4 frustum planes
- Cull entire subtrees outside view

### 4. Backface Culling at Face Level
- Keep existing backface culling logic
- Only applied to faces that pass node-level culling

## Expected Performance Improvements

1. **Early Termination**: PVS culling can skip entire subtrees
2. **Hierarchical Culling**: Node bounds culling prevents face-level checks
3. **Correct Order**: PVS before frustum prevents over-culling
4. **Memory Coherence**: Depth-first traversal is cache-friendly

## Testing and Validation

1. **PVS Marking**: Verify correct leaves marked visible
2. **Frustum Culling**: Test node-level culling accuracy
3. **Face Rendering**: Ensure all visible faces still render
4. **Performance**: Compare frame times and culling efficiency

## Fallback Strategies

1. **No PVS Data**: Mark all leaves visible
2. **Invalid Camera Cluster**: Mark all leaves visible
3. **Degenerate Bounds**: Skip node-level frustum culling
4. **Traversal Failures**: Fall back to direct face iteration

## Implementation Priority

1. **HIGH**: Add visframe system ✅
2. **HIGH**: Implement `MarkLeaves()` method
3. **HIGH**: Add `FindLeafForPoint()` for proper cluster finding
4. **MEDIUM**: Implement recursive `TraverseForRendering()`
5. **MEDIUM**: Move frustum culling to node bounds
6. **LOW**: Optimize node bounds computation
7. **LOW**: Add performance metrics and debugging

This refactor will align our engine with Quake's battle-tested BSP rendering architecture, providing proper hierarchical culling and significant performance improvements.
