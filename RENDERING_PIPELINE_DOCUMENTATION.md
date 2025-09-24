# PaintSplash Rendering Pipeline Documentation

## Overview

This document outlines the complete rendering pipeline for PaintSplash, from texture loading to final rendering. The system uses Raylib as the underlying graphics library with a custom ECS (Entity-Component-System) architecture for managing game objects and rendering.

## Table of Contents

1. [Core Systems Architecture](#core-systems-architecture)
2. [Texture Loading Pipeline](#texture-loading-pipeline)
3. [Asset System Integration](#asset-system-integration)
4. [World Geometry Rendering](#world-geometry-rendering)
5. [Entity-Based Rendering](#entity-based-rendering)
6. [Material System](#material-system)
7. [Raylib Integration Points](#raylib-integration-points)
8. [Debugging and Troubleshooting](#debugging-and-troubleshooting)

## Core Systems Architecture

### ECS Components

- **MaterialComponent**: Central material system containing texture handles, colors, and rendering properties
- **TextureComponent**: Contains AssetSystem texture handles for entity-specific textures
- **TransformComponent**: Position, rotation, scale for rendering transforms

### Key Systems

- **AssetSystem**: Manages texture loading and caching
- **WorldSystem**: Handles world geometry and BSP tree rendering
- **RenderSystem**: Orchestrates the entire rendering pipeline
- **Renderer**: Low-level Raylib integration and drawing commands
- **TextureManager**: Raylib texture loading and GPU management

## Texture Loading Pipeline

### 1. Asset Root Configuration

```cpp
// AssetSystem.cpp - Constructor
std::string exeDir = Utils::GetExecutableDir();
assetRootPath_ = fs::absolute(fs::path(exeDir).parent_path().parent_path() / "assets").string();
```

**Path Resolution:**
- Executable: `/Users/.../build/bin/paintsplash`
- Asset Root: `/Users/.../assets/` (source directory)
- Texture Path: `textures/devtextures/Dark/proto_wall_dark.png`
- Full Path: `/Users/.../assets/textures/devtextures/Dark/proto_wall_dark.png`

### 2. Texture Loading Sequence

#### AssetSystem::LoadTexture()
```cpp
bool AssetSystem::LoadTexture(const std::string& path) {
    std::string absPath = GetAssetPath(path);  // Convert relative to absolute
    auto& textureManager = TextureManager::Get();
    Texture2D texture = textureManager.Load(absPath);  // Load via Raylib
    // Store in cache with reference counting
}
```

#### TextureManager::Load()
```cpp
Texture2D TextureManager::Load(const string& path) {
    vector<string> attempts = {
        absPath,                                    // Absolute path first
        fs::path(exeDir) / path,                   // Relative to executable
        path,                                      // CWD-relative
        fs::path(exeDir).parent_path().parent_path() / path // Project root
    };

    for (const auto& attempt : attempts) {
        texture = LoadTexture(attempt.c_str());  // Raylib LoadTexture
        if (texture.id != 0) break;
    }
}
```

### 3. Raylib Texture Loading

The actual texture loading happens through Raylib's `LoadTexture()` function:

```cpp
// Called from TextureManager::LoadTexture()
Texture2D LoadTexture(const char* fileName) {
    // Raylib internal implementation:
    // - Opens file using stb_image
    // - Uploads to GPU via OpenGL
    // - Returns Texture2D with ID, width, height, format
}
```

## Asset System Integration

### WorldSystem Texture Loading

```cpp
void WorldSystem::LoadTexturesAndMaterials(MapData& mapData) {
    for (const auto& textureInfo : mapData.textures) {
        std::string texturePath = "textures/" + textureInfo.name;

        if (assetSystem->LoadTexture(texturePath)) {
            // Success - texture is cached in AssetSystem
            LOG_INFO("SUCCESS: AssetSystem loaded texture: " + texturePath);
        } else {
            LOG_ERROR("FAILED: AssetSystem could not load texture: " + texturePath);
        }
    }
}
```

### MaterialComponent Creation

```cpp
// For each world material
MaterialComponent material;
material.materialName = "world_material_" + std::to_string(materialId);
material.textures.diffuse = assetSystem->GetTextureHandle(texturePath);
material.diffuseColor = Color{255, 255, 255, 255}; // White tint
material.alpha = 1.0f;
material.doubleSided = false;
material.depthTest = true;
material.depthWrite = true;
```

## World Geometry Rendering

### BSP Tree Structure

```cpp
struct WorldGeometry {
    std::vector<Face> faces;
    std::vector<WorldMaterial> materials;
    BSPTree bspTree;
};

struct Face {
    std::vector<Vertex> vertices;
    int materialId;
    Vector3 normal;
};

struct WorldMaterial {
    Color diffuseColor;
    Texture2D texture;  // Legacy - being replaced
    bool hasTexture;
    std::string textureName;
};
```

### Rendering Flow

#### 1. RenderSystem::Render()
```cpp
void RenderSystem::Render() {
    // 1. Clear screen
    BeginDrawing();
    ClearBackground(BLACK);

    // 2. Setup 3D camera
    BeginMode3D(camera);

    // 3. Render world geometry (BSP)
    RenderWorldGeometryDirect();

    // 4. Render entities
    RenderEntities();

    // 5. Render skybox
    RenderSkybox();

    EndMode3D();
    EndDrawing();
}
```

#### 2. World Geometry Rendering
```cpp
void Renderer::RenderWorldGeometryDirect() {
    // For each face in BSP tree
    for (const auto& face : worldGeometry_->faces) {
        // Get material for this face
        const auto& worldMaterial = worldGeometry_->materials[face.materialId];

        // Convert to MaterialComponent format
        MaterialComponent faceMaterial = ConvertWorldMaterial(worldMaterial);

        // Setup material (textures, colors, blending)
        SetupMaterial(faceMaterial);

        // Render the face
        RenderFace(face, faceMaterial);
    }
}
```

#### 3. Face Rendering
```cpp
void Renderer::RenderFace(const Face& face, const MaterialComponent& material) {
    // Determine primitive type
    int vertexCount = face.vertices.size();
    int primitiveType = (vertexCount == 4) ? RL_QUADS : RL_TRIANGLES;

    // Begin texture binding
    if (material.textures.diffuse) {
        rlSetTexture(material.textures.diffuse->id);
    }

    // Draw vertices
    rlBegin(primitiveType);
    for (const auto& vertex : face.vertices) {
        rlColor4ub(material.diffuseColor.r, material.diffuseColor.g,
                   material.diffuseColor.b, material.diffuseColor.a);
        rlTexCoord2f(vertex.texCoord.x, vertex.texCoord.y);
        rlVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
    }
    rlEnd();

    // Unbind texture
    rlSetTexture(0);
}
```

### Texture Coordinate Calculation

```cpp
void CalculateUVs(Face& face, const Vector3& textureSize) {
    // Based on face normal and dimensions
    bool isHorizontal = (abs(face.normal.y) > 0.9f);

    for (auto& vertex : face.vertices) {
        if (isHorizontal) {
            // Floor/ceiling UV mapping
            vertex.texCoord.x = vertex.position.x / textureSize.x;
            vertex.texCoord.y = vertex.position.z / textureSize.z;
        } else {
            // Wall UV mapping
            vertex.texCoord.x = vertex.position.x / textureSize.x;
            vertex.texCoord.y = vertex.position.y / textureSize.z;
        }
    }
}
```

## Entity-Based Rendering

### Mesh Rendering

```cpp
void Renderer::RenderMesh(const Mesh& mesh, const MaterialComponent& material,
                         const Matrix& transform) {
    // Setup material properties
    SetupMaterial(material);

    // Apply transform
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(transform));

    // Render each submesh
    for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
        rlBegin(RL_TRIANGLES);

        for (int j = 0; j < 3; j++) {
            int idx = i + j;
            rlTexCoord2f(mesh.texcoords[idx].x, mesh.texcoords[idx].y);
            rlNormal3f(mesh.normals[idx].x, mesh.normals[idx].y, mesh.normals[idx].z);
            rlVertex3f(mesh.vertices[idx].x, mesh.vertices[idx].y, mesh.vertices[idx].z);
        }

        rlEnd();
    }

    rlPopMatrix();
}
```

### Optimized Batching

```cpp
void Renderer::AddMeshToBatch(const Mesh& mesh, const MaterialComponent& material) {
    // Group meshes by material to reduce state changes
    auto& batch = materialBatches_[material.materialName];

    // Combine vertex data
    batch.vertices.insert(batch.vertices.end(),
                         mesh.vertices.begin(), mesh.vertices.end());
    batch.texcoords.insert(batch.texcoords.end(),
                          mesh.texcoords.begin(), mesh.texcoords.end());
    batch.normals.insert(batch.normals.end(),
                        mesh.normals.begin(), mesh.normals.end());
}
```

## Material System

### MaterialComponent Structure

```cpp
struct MaterialComponent {
    std::string materialName;
    struct {
        AssetSystem::TextureHandle diffuse;
        AssetSystem::TextureHandle normal;
        AssetSystem::TextureHandle specular;
    } textures;

    Color diffuseColor = WHITE;
    Color specularColor = WHITE;
    float alpha = 1.0f;
    float shininess = 0.0f;
    bool doubleSided = false;
    bool depthTest = true;
    bool depthWrite = true;
};
```

### Material Setup

```cpp
void Renderer::SetupMaterial(const MaterialComponent& material) {
    // 1. Texture binding
    if (material.textures.diffuse) {
        rlEnableTexture(material.textures.diffuse->id);
    } else {
        rlDisableTexture();  // Use default white texture
    }

    // 2. Blending mode
    if (material.alpha < 1.0f) {
        BeginBlendMode(BLEND_ALPHA);
    } else {
        BeginBlendMode(BLEND_OPAQUE);
    }

    // 3. Depth testing
    if (material.depthTest) {
        rlEnableDepthTest();
    } else {
        rlDisableDepthTest();
    }

    // 4. Depth writing
    if (material.depthWrite) {
        rlEnableDepthMask();
    } else {
        rlDisableDepthMask();
    }

    // 5. Face culling
    if (material.doubleSided) {
        rlDisableBackfaceCulling();
    } else {
        rlEnableBackfaceCulling();
    }
}
```

## Raylib Integration Points

### Core Raylib Functions Used

1. **Texture Management:**
   - `LoadTexture()` - Load image from file to GPU
   - `UnloadTexture()` - Free GPU texture memory
   - `SetTextureFilter()` - Set bilinear/trilinear filtering
   - `SetTextureWrap()` - Set repeat/clamp wrapping

2. **Rendering Pipeline:**
   - `BeginDrawing()` / `EndDrawing()` - Frame setup/cleanup
   - `BeginMode3D()` / `EndMode3D()` - 3D camera setup
   - `ClearBackground()` - Clear frame buffer
   - `BeginBlendMode()` / `EndBlendMode()` - Transparency setup

3. **Low-level Rendering (rlgl):**
   - `rlBegin()` / `rlEnd()` - Begin/end primitive drawing
   - `rlVertex3f()` - Submit vertex position
   - `rlTexCoord2f()` - Submit texture coordinates
   - `rlColor4ub()` - Submit vertex color
   - `rlNormal3f()` - Submit vertex normal
   - `rlSetTexture()` - Bind texture to current context

### State Management

```cpp
// Raylib maintains internal state for:
- Current texture binding (rlSetTexture)
- Current blending mode (BeginBlendMode)
- Depth testing state (rlEnableDepthTest)
- Face culling state (rlEnableBackfaceCulling)
- Matrix stack (rlPushMatrix/rlPopMatrix)
```

## Debugging and Troubleshooting

### Common Issues

#### 1. White Triangles (No Textures)

**Symptoms:**
```
WorldMaterial for face X, materialId: Y has no texture
SetupMaterial called - diffuse texture handle valid: NO
No diffuse texture handle, using untextured rendering
```

**Root Causes:**
1. AssetSystem path configuration incorrect
2. Texture files missing from expected location
3. AssetSystem::LoadTexture() failing
4. MaterialComponent texture handle not set

**Debug Steps:**
```cpp
// 1. Verify asset root path
LOG_INFO("AssetSystem created with asset root: " + assetRootPath_);

// 2. Check texture file existence
std::filesystem::path fullPath = assetRootPath_ / texturePath;
LOG_INFO("File exists: " + std::string(std::filesystem::exists(fullPath) ? "YES" : "NO"));

// 3. Verify Raylib texture loading
Texture2D tex = LoadTexture(path.c_str());
LOG_INFO("Raylib LoadTexture result - ID: " + std::to_string(tex.id));
```

#### 2. Black/Pink Textures

**Symptoms:**
- Textures appear as solid black or magenta

**Root Causes:**
1. Texture file corrupted
2. Incorrect texture format
3. Raylib failed to load texture data

**Debug Steps:**
```cpp
// Check texture properties after loading
LOG_INFO("Texture loaded - ID: " + std::to_string(texture.id) +
         ", Width: " + std::to_string(texture.width) +
         ", Height: " + std::to_string(texture.height) +
         ", Format: " + std::to_string(texture.format));
```

#### 3. Memory Leaks

**Symptoms:**
- GPU memory usage grows over time
- Application becomes slow

**Root Causes:**
1. Textures not unloaded when no longer needed
2. AssetSystem reference counting broken

**Debug Steps:**
```cpp
// Monitor texture cache size
LOG_INFO("Texture cache size: " + std::to_string(cache_.size()));

// Check reference counts
for (const auto& entry : cache_) {
    LOG_DEBUG("Texture " + entry.first + " refs: " + std::to_string(entry.second->refCount));
}
```

### Performance Optimization

#### 1. Texture Atlasing
```cpp
// Combine multiple small textures into single large texture
// Reduces texture state changes during rendering
```

#### 2. Mipmapping
```cpp
// Enable automatic mipmap generation
SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
```

#### 3. Texture Compression
```cpp
// Use compressed texture formats (DXT, ETC2)
// Reduces GPU memory usage and bandwidth
```

### Log Analysis

#### Key Debug Messages

```
AssetSystem created with asset root: /path/to/assets
Attempting to load texture: 'textures/file.png' for material 0
File exists: YES
Raylib LoadTexture result - ID: 123
SUCCESS: AssetSystem loaded texture: textures/file.png
SetupMaterial called - diffuse texture handle valid: YES
Processing face 0 with 4 vertices
Rendering face with 4 vertices
```

## Current Issues and Fixes

### Issue: Textures Not Loading

**Problem:** AssetSystem was looking in build directory instead of source assets

**Fix Applied:**
```cpp
// OLD: Wrong path
assetRootPath_ = fs::absolute(fs::path(Utils::GetExecutableDir()) / "assets").string();

// NEW: Correct path
std::string exeDir = Utils::GetExecutableDir();
assetRootPath_ = fs::absolute(fs::path(exeDir).parent_path().parent_path() / "assets").string();
```

### Issue: Quad vs Triangle Rendering

**Problem:** World geometry was rendering as triangles instead of quads

**Fix Applied:**
```cpp
int primitiveType = (vertexCount == 4) ? RL_QUADS : RL_TRIANGLES;
```

### Issue: MaterialComponent Integration

**Problem:** Legacy WorldMaterial system not using new MaterialComponent

**Current State:** Hybrid system where WorldSystem creates MaterialComponents but Renderer still uses legacy WorldMaterial for world geometry

**Future Fix:** Complete migration to MaterialComponent for all rendering

## Conclusion

The PaintSplash rendering pipeline is a complex system that integrates:

1. **Asset Management:** AssetSystem with TextureManager for loading/caching
2. **Material System:** MaterialComponent as central material definition
3. **Geometry Rendering:** BSP tree traversal with face-based rendering
4. **Raylib Integration:** Low-level OpenGL abstraction through rlgl
5. **Performance Optimization:** Batching, state management, and texture caching

The current hybrid approach (legacy WorldMaterial + new MaterialComponent) works but should be fully migrated to use MaterialComponent consistently across all rendering paths for better maintainability and features.
