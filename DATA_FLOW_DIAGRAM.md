# PaintStrike Data Flow Diagram

## Overview
This document traces the complete data flow from YAML map parsing through to final rendering, highlighting where material assignment and texture issues occur.

## Data Flow Pipeline

### Phase 1: YAML Parsing → MapData
```
YAML File (.map)
    ↓
MapLoader::LoadMap()
    ↓
MapData struct {
    - materials: vector<MaterialInfo>  // id, name, diffuseMap
    - faces: vector<Face>             // vertices, uvs, materialId
    - brushes: vector<Brush>          // faces with materialId
}
```

### Phase 2: Material Loading → ECS Entities
```
MapData.materials
    ↓
WorldSystem::LoadTexturesAndMaterials()
    ↓
Creates ECS MaterialComponent entities
    ↓
worldMaterialEntityIds_ map: materialId → entityId
    ↓
Result: Material entities (IDs: 2, 3, 4, 5) with textures loaded
```

### Phase 3: Face Processing → Material Assignment
```
MapData.faces/brushes
    ↓
WorldSystem::BuildWorldGeometry()
    ↓
For each face:
    face.materialId (0,1,2,3) → lookup in worldMaterialEntityIds_
    ↓
    face.materialEntityId = entityId (2,3,4,5)
    ↓
mapData.faces = updated faces with materialEntityIds
```

### Phase 4: WorldGeometry Creation
```
Updated MapData
    ↓
worldGeometry_->faces = mapData.faces  ← **CRITICAL POINT**
    ↓
WorldSystem::BuildBSPTreeAfterMaterials()
    ↓
bspTreeSystem_->LoadWorld(worldGeometry_->faces)
    ↓
BSP construction with materialEntityId faces
```

### Phase 5: Batching → Render Groups
```
WorldGeometry.faces (with materialEntityIds)
    ↓
WorldGeometry::BuildBatchesFromFaces()
    ↓
Groups by materialEntityId:
    Batch 0: materialId=2 (entityId 2) - Green walls
    Batch 1: materialId=3 (entityId 3) - Light floor
    Batch 2: materialId=4 (entityId 4) - Dark ceiling
    Batch 3: materialId=5 (entityId 5) - Orange slopes/stairs
```

### Phase 6: Rendering
```
Batches (with materialEntityId)
    ↓
Renderer::RenderWorldGeometry()
    ↓
For each batch:
    materialId (2,3,4,5) → GetEngine().GetEntityById(materialId)
    ↓
    SetupMaterial(*materialEntity)
    ↓
    Bind texture and render faces
```

## Critical Bug Identified

### **The Root Cause**
In `WorldSystem::BuildWorldGeometry()`, the line:
```cpp
worldGeometry_->faces = mapData.faces;
```
was executed **BEFORE** materialEntityIds were assigned to faces.

### **The Fix**
Moved the assignment to **AFTER** materialEntityId assignment:
```cpp
// Assign materialEntityIds to faces first
for (auto& face : mutableFaces) {
    face.materialEntityId = worldMaterialEntityIds_[face.materialId];
}

// THEN set worldGeometry faces
worldGeometry_->faces = mapData.faces;  // Now has materialEntityIds
```

## Quake Code Analysis

### BSP Tree Construction
Quake's approach:
- Precompute BSP tree at compile time
- Store all faces in global `dfaces` array
- Each face has `texture` index into `dtex` array
- Faces reference into global surface array

Our adaptation:
- Runtime BSP construction from faces
- Faces maintain materialEntityId references
- Clustering for PVS (though simplified)

### Face Processing
Quake stores face data in lumps:
- `dface_t` - face data with texture index
- `texinfo_t` - texture coordinate info
- Global surface array for rendering

Our approach:
- Faces with embedded UVs from YAML
- Material entities for texture management
- UV calculation based on face normals

### Material Handling
Quake:
- Texture indices in BSP data
- Texture loading at startup
- Direct texture binding

Our ECS approach:
- MaterialComponent entities
- AssetSystem texture management
- Entity-based material lookup

## Current Issues & Fixes

### Issue 1: Material Assignment Timing
**Fixed**: Moved `worldGeometry_->faces = mapData.faces` after materialEntityId assignment.

### Issue 2: UV Coordinate System
**Fixed**: Transformed YAML UVs from [-0.5,0.5] to [0,1] range for OpenGL.

### Issue 3: Face Orientation UVs
**Fixed**: Implemented normal-based UV calculation:
- Y-faces: X,Z coordinates (floors/ceilings)
- X-faces: Y,Z coordinates (walls)
- Z-faces: X,Y coordinates (walls)
- Arbitrary: Tangent space projection

### Issue 4: Batching by Wrong ID
**Fixed**: Changed batching to group by `materialEntityId` instead of `materialId`.

## Expected Results

After fixes:
- All 127 faces should have proper materialEntityIds (2,3,4,5)
- Batches should be correctly grouped by material entity
- Renderer should find correct MaterialComponent entities
- All textures should render (green, light, dark, orange)
- Slopes should use orange material with proper UVs

## Testing Verification

Check logs for:
1. `Face assigned materialEntityId X (from materialId Y)` - All faces assigned
2. `Material ID 0: 48 faces` etc. - Correct face counts
3. `SetupMaterial called - diffuse texture handle path: '...'` - Textures loading
4. `SUCCESS: Got texture with ID X, binding it` - Texture binding working
5. `World geometry rendered - Surfaces: 127, Triangles: 496` - All geometry rendering
