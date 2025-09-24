# BSP Rendering Culling Issues - Update and Fixes

## Issue Summary

The BSP rendering system was experiencing aggressive culling where geometry directly in front of the camera was being culled, even though materials were still visible. This indicated a problem with the PVS (Potentially Visible Set) or frustum culling logic.

## Root Cause Analysis

### Primary Issue: Broken Frustum Culling

The main problem was in the `IsPointInViewFrustum()` method in `BSPTreeSystem.cpp`. This method was:

1. **Extremely restrictive**: Only accepting points that were either:
   - Very close to the camera (< 0.1 units)
   - Had a very specific forward-facing angle (> -0.8 dot product)

2. **Not implementing proper frustum culling**: Instead of checking against the camera's actual view frustum (field of view cone), it was doing a basic "behind camera" check with an overly conservative threshold.

3. **Applied incorrectly**: The `IsFaceVisibleForRendering()` method required that **at least one vertex** of each face pass this restrictive test. Since most faces in a typical 3D scene don't have vertices extremely close to the camera or within the narrow acceptance cone, virtually all faces were being culled.

### Secondary Issues

1. **PVS Implementation**: While the PVS system was working, the aggressive frustum culling at the face level was masking visibility issues.

2. **Face-level culling logic**: The face visibility check was applied after PVS and node-level frustum culling, causing additional unnecessary rejection.

## Quake 3 Reference Implementation

Research into Quake 3's source code revealed the proper approach:

### Quake 3 Frustum Culling (`tr_world.c`)

```c
// Check each frustum plane
if ( planeBits & 1 ) {
    r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
    if (r == 2) {
        return; // culled - completely behind this frustum plane
    }
    if ( r == 1 ) {
        planeBits &= ~1; // all descendants will also be in front
    }
}
```

### BoxOnPlaneSide Function (`q_math.c`)

Returns:
- `1`: Box is completely on the front side of the plane
- `2`: Box is completely on the back side of the plane
- `3`: Box straddles the plane (intersects it)

The algorithm calculates the signed distance from the plane to the box's corners and determines if the entire box is on one side.

## Current Implementation Status

### Working Components
- ✅ BSP tree construction and traversal
- ✅ PVS (Potentially Visible Set) generation and marking
- ✅ Node-level frustum culling (using AABB bounds)
- ✅ Cluster-based visibility grouping
- ✅ Collision detection (works fine)

### Broken Components
- ❌ Face-level frustum culling (`IsPointInViewFrustum`)
- ❌ Face visibility determination logic

## Proposed Fixes

### 1. Implement Proper Frustum Planes

Create a function to extract 6 frustum planes from the `Camera3D`:
- Near plane
- Far plane
- Left plane
- Right plane
- Top plane
- Bottom plane

### 2. Replace Face-Level Culling

Replace the broken `IsPointInViewFrustum` with proper `BoxOnPlaneSide`-style frustum culling for AABBs.

### 3. Simplify Face Visibility Logic

For faces that pass PVS and node-level culling, only apply:
- Backface culling (optional, currently disabled for debugging)
- Basic distance/frustum checks (if needed)

### 4. Add Debug Visualization

Enhance PVS debug mode to show:
- Frustum planes
- Culled vs visible nodes
- Face-level culling statistics

## Implementation Status - COMPLETED ✅

### Phase 1: Fix Frustum Culling (High Priority) ✅
1. ✅ Implemented `ExtractFrustumPlanes()` function - Extracts 6 frustum planes from Camera3D
2. ✅ Implemented `BoxOnPlaneSide()` for AABB-plane testing - Direct port from Quake 3
3. ✅ Implemented `IsAABBVisibleInFrustum()` - Tests AABB against all frustum planes
4. ✅ Replaced broken `IsPointInViewFrustum()` with proper frustum culling at node level
5. ✅ Updated face-level culling to use AABB-based frustum testing
6. ✅ Project compiles successfully with all changes

### Key Changes Made

#### CRITICAL FIX: Visibility Marking Algorithm
**Problem**: The `MarkLeaves()` function was incorrectly checking cluster IDs on internal nodes, but only leaf nodes get cluster IDs assigned. Additionally, faces stored at internal nodes (coplanar faces) were not being marked as visible.

**Solution**: Fixed the algorithm to work like Quake 3:
1. Get visible clusters from PVS
2. Collect all leaf nodes in visible clusters
3. Mark the entire path from each visible leaf up to the root as visible
4. **CRITICAL**: Mark ALL descendants of marked nodes to ensure internal nodes with faces are visible

#### New Structures Added
- `FrustumPlane`: Represents a single frustum plane (like Quake 3's `cplane_s`)
- `Frustum`: Contains all 6 frustum planes (left, right, bottom, top, near, far)

#### New Methods Implemented
- `ExtractFrustumPlanes()`: Calculates frustum planes from Camera3D position/target/FOV
- `SetPlaneSignbits()`: Optimizes plane calculations (like Quake 3)
- `BoxOnPlaneSide()`: Tests AABB against single frustum plane
- `IsAABBVisibleInFrustum()`: Tests AABB against all 6 frustum planes

#### Integration Points
- Node-level frustum culling uses proper `BoxOnPlaneSide` testing (currently disabled for debugging)
- Face-level culling simplified to AABB-based frustum testing (currently disabled for debugging)
- Backface culling temporarily disabled for debugging (can be re-enabled later)

### Technical Details

The new implementation follows Quake 3's exact approach:
- Uses `BoxOnPlaneSide` to test AABBs against frustum planes
- Returns 1 (front), 2 (back), or 3 (spanning) for each plane test
- Node/AABB is culled if it's completely behind ANY frustum plane
- Much more accurate than the previous point-based approach

### Performance Impact
- More accurate culling should reduce over-culling
- Face-level checks simplified (no longer checking individual vertices)
- Node-level culling now uses optimized frustum planes

## Triangle vs Quad Geometry Issue

**Question**: Why are some textures appearing as triangles instead of full quads?

**Answer**: This is expected BSP behavior. During BSP tree construction, faces that span splitting planes get split into smaller polygons. A quad can become two triangles, or multiple smaller polygons depending on how many planes it crosses.

- **BSP Splitting**: When building the BSP tree, faces are split by partitioning planes
- **Result**: Original quads become triangles or more complex polygons
- **Rendering**: These split faces are triangulated for rendering, so you see triangles instead of the original quad

This is normal and correct for BSP-based rendering. The visual result is the same - the texture appears correctly, just rendered as triangles instead of a quad.

## Expected Results

After implementing proper visibility marking:
- Geometry should now render correctly (no more 100% culling)
- Materials should stop "popping" in and out as you move
- PVS should work correctly - only geometry in visible clusters should render
- Face splitting (quads→triangles) is normal BSP behavior

### Current State
- ✅ **Visibility marking algorithm fixed** - marks entire subtrees of visible clusters
- ✅ **Internal node faces now rendered** - addresses the 72 missing faces
- 🔄 **Frustum culling temporarily disabled** for testing
- 🔄 **Backface culling disabled** to show all geometry
- ✅ **PVS system working correctly**

## BSP FACE STORAGE ISSUE IDENTIFIED

### Critical Discovery
**Problem**: Only 55 faces are being rendered out of 127 total faces. The BSP tree is only creating 55 leaves, meaning 72 faces are being lost during construction.

**Root Cause Analysis**:
1. **Started with 127 faces** from map data ✅
2. **BSP construction creates only 55 leaves** ❌ (each with 1 face)
3. **72 faces are missing** ❌

### Recent Fixes Attempted
1. **Fixed face storage at internal nodes** - Removed coplanar faces from internal nodes, now only store at leaves
2. **Fixed coplanar face handling** - Coplanar faces now added to both front AND back branches
3. **Implemented spatial clustering** - Creates multiple clusters based on proximity instead of sequential grouping

### Current Status
- ✅ **Spatial clustering working** - Leaves are properly distributed across space
- ✅ **No faces stored at internal nodes** - All faces should be at leaves
- ❌ **Still only 55 faces rendered** - BSP construction is still losing faces
- 🔄 **Frustum culling disabled** for testing
- 🔄 **Backface culling disabled** for testing

### Next Steps Needed
**Debug BSP Construction**: Add logging to trace where faces are lost during recursive building. The issue is likely in:
- Face classification (front/back/coplanar)
- Face splitting logic
- Recursive distribution to child branches

## Testing Scenarios

1. **Direct front viewing**: Camera looking directly at geometry
2. **Peripheral viewing**: Geometry at edges of screen
3. **Distance culling**: Far vs near geometry
4. **Complex scenes**: Multiple clusters and PVS boundaries

## Files to Modify

- `src/world/BSPTreeSystem.cpp`: Fix frustum culling methods
- `src/world/BSPTreeSystem.h`: Add frustum plane structures
- `src/rendering/Renderer.cpp`: Update debug visualization if needed

## Risk Assessment

**Low Risk**: The current culling is too aggressive, so making it less restrictive should only add visible geometry, not break existing functionality.

**Performance Impact**: Proper frustum culling should improve performance by reducing unnecessary face-level checks.
