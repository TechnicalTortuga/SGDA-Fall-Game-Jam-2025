# PaintSplash Rendering Issue Analysis

## Problem Summary
- Stairs (material 0) appear white instead of showing the dark texture
- Walls (material 0) show the dark texture correctly
- Switching stairs to material 1 (light texture) fixes the issue
- Other geometries work correctly with their assigned materials

## Code Flow Analysis

### 1. Map Loading & Processing
```cpp
// WorldSystem::ProcessMapData()
BuildWorldGeometry(mapData);  // Creates BSP, builds initial batches with hasTexture=false
LoadTexturesAndMaterials(mapData);  // Loads textures, rebuilds batches with hasTexture=true
```

### 2. Initial Batch Creation (BuildWorldGeometry)
- `WorldGeometry::Clear()` - Clears materials map
- BSP tree building splits faces (potentially creating triangles from quads)
- `BuildBatchesFromFaces()` - Creates render batches with `hasTexture=false` for all materials

### 3. Texture Loading (LoadTexturesAndMaterials)
- Loads textures via `TextureManager::Load()`
- Sets `material.hasTexture = true` and `material.texture = loadedTexture`
- Rebuilds batches with `BuildBatchesFromFaces()` again

### 4. Rendering (RenderBatches)
- For each batch: `SetupMaterial(*mat)` binds the texture
- Renders geometry using batch colors (should be WHITE for textured surfaces)

## Key Findings

### Material Handling
- Materials are stored in `WorldGeometry::materials` map (int -> WorldMaterial)
- Material 0: dark texture (should work)
- Material 1: light texture (known to work)
- Material 2: green texture (known to work)

### Batch Building Process
```cpp
// BuildBatchesFromFaces()
for each face:
    const WorldMaterial* mat = GetMaterial(f.materialId);
    if (mat && mat->hasTexture) {
        finalColor = {255, 255, 255, 255};  // WHITE
    }
    // Set colors for vertices
    batch.colors.push_back(finalColor);  // Quads: 4 times
    // OR for triangles: 3 times with potentially different colors
```

### Rendering Differences
- **Quads**: Use `batch.colors[vertexIndex]` for all 4 vertices
- **Triangles**: Use `batch.colors[vertexIndex]`, `[vertexIndex+1]`, `[vertexIndex+2]`

## Potential Issues Identified

### 1. BSP Face Splitting
- BSP building splits faces at planes, potentially converting quads to triangles
- Stairs may be getting split into triangles, walls may remain as quads
- Triangle rendering uses different color indices than quad rendering

### 2. Batch Color Assignment During Rebuilding
- When batches are rebuilt after texture loading, colors are set correctly for the current face
- But if triangles and quads are mixed in the same batch, the color indexing might be inconsistent

### 3. Material State During Batch Building
- `GetMaterial()` is called during batch building to check `hasTexture`
- If materials aren't properly loaded at rebuild time, `hasTexture` would be false
- But logs show materials are loaded before rebuilding

### 4. Texture Binding State
- `SetupMaterial()` binds textures per batch
- If texture binding fails or gets corrupted between batches, geometry would appear white
- But logs show correct texture IDs are being bound

## Critical Code Paths

### WorldSystem::LoadTexturesAndMaterials()
```cpp
// Load textures
for (const auto& textureInfo : mapData.textures) {
    Texture2D texture = TextureManager::Get().Load(texturePath);
    if (texture.id != 0) {
        WorldMaterial& material = worldGeometry_->materials[textureInfo.index];
        material.texture = texture;
        material.hasTexture = true;  // CRITICAL: This must be set
    }
}

// Rebuild batches - CRITICAL POINT
if (!worldGeometry_->faces.empty()) {
    worldGeometry_->BuildBatchesFromFaces(worldGeometry_->faces);
}
```

### WorldRenderer::RenderBatches()
```cpp
for (const auto& batch : batches) {
    const WorldMaterial* mat = worldGeometry_->GetMaterial(batch.materialId);
    if (mat) {
        SetupMaterial(*mat);  // CRITICAL: Texture binding
        // Render geometry...
    }
}
```

## Root Cause Hypothesis

The issue appears to be related to **face splitting during BSP building** combined with **batch rebuilding after texture loading**.

1. Initial batch creation: BSP splits some faces (stairs) into triangles, others (walls) remain quads
2. Texture loading: Materials get proper `hasTexture = true`
3. Batch rebuilding: Faces are processed again, but the batch color assignment might be inconsistent between triangles and quads
4. Rendering: Triangle rendering uses different color indices than quad rendering

## Root Cause Found & Fixed

**CRITICAL BUG**: The `RenderBatches()` function was not using the batch indices for rendering. It assumed positions were stored in sequential groups of 4 (quads) or 3 (triangles), but `BuildBatchesFromFaces()` was creating proper indexed geometry.

The rendering code was trying to detect quad vs triangle patterns in the indices but then ignoring the indices and rendering positions sequentially. This caused:
- Incorrect vertex indexing for triangles created by BSP splitting
- Inconsistent color application between different geometry types
- Stairs (split into triangles) not getting proper texture rendering

## Fix Implemented

### 1. Proper Indexed Triangle Rendering
Replaced the flawed sequential rendering with proper indexed triangle rendering that uses the `batch.indices` array to reference vertices correctly.

### 2. Unified Triangulation
Simplified `BuildBatchesFromFaces()` to always create indexed triangles using fan triangulation, eliminating the quad vs triangle distinction that was causing rendering inconsistencies.

### 3. Consistent Vertex Coloring
Ensured all vertices in textured surfaces get WHITE coloring consistently, regardless of how the geometry was triangulated.

## Test Cases Needed
- Verify walls (material 0, quads) render correctly
- Verify stairs (material 0, potentially triangles) render correctly
- Test switching stairs to material 1 (should work)
- Test all materials on all geometry types
