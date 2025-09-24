# Shadow Mapping Implementation Flow Analysis

## Raylib Official Example Flow

### 1. **Initialization Phase**
```c
// Create shadow map render texture
RenderTexture2D shadowMap = LoadShadowmapRenderTexture(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);

// Load shadow shader
Shader shadowShader = LoadShader("shadowmap.vs", "shadowmap.fs");

// Set up light camera
Camera3D lightCamera = { 0 };
lightCamera.position = Vector3Scale(lightDir, -15.0f);
lightCamera.target = Vector3Zero();
lightCamera.projection = CAMERA_ORTHOGRAPHIC;
lightCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
lightCamera.fovy = 20.0f;
```

### 2. **Depth Buffer Creation** (`LoadShadowmapRenderTexture`)
```c
static RenderTexture2D LoadShadowmapRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };
    target.id = rlLoadFramebuffer(); // Create FBO

    // Create depth texture (NO COLOR ATTACHMENT)
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19; // DEPTH_COMPONENT_24BIT
    target.depth.mipmaps = 1;

    // Attach depth texture to FBO
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH,
                       RL_ATTACHMENT_TEXTURE2D, 0);

    if (rlFramebufferComplete(target.id)) {
        TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created", target.id);
    }
    return target;
}
```

### 3. **Render Loop - PASS 1: Shadow Map Generation**
```c
BeginTextureMode(shadowMap);
    ClearBackground(WHITE); // Clear to white (far depth)
    BeginMode3D(lightCamera);
        // Render all geometry from light's perspective
        DrawScene(cube, robot); // Uses regular shader, but renders to depth-only FBO
    EndMode3D();
EndTextureMode();

// Calculate light view-projection matrix
lightViewProj = MatrixMultiply(lightView, lightProj);
```

### 4. **Render Loop - PASS 2: Final Rendering**
```c
BeginDrawing();
    // Bind shadow map texture
    rlActiveTextureSlot(10);
    rlEnableTexture(shadowMap.depth.id);
    rlSetUniform(shadowMapLoc, &textureSlot, SHADER_UNIFORM_INT, 1);

    // Set light VP matrix
    SetShaderValueMatrix(shadowShader, lightVPLoc, lightViewProj);

    BeginMode3D(camera);
        // Render scene with shadow shader
        DrawScene(cube, robot);
    EndMode3D();
EndDrawing();
```

## Our Current Implementation Flow

### 1. **Initialization Phase**
```cpp
// Create shadow map (LightSystem::Initialize)
shadowMap_.id = rlLoadFramebuffer();
shadowMap_.depth.id = rlLoadTextureDepth(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, false);
shadowMap_.depth.format = 19;
rlFramebufferAttach(shadowMap_.id, shadowMap_.depth.id, RL_ATTACHMENT_DEPTH,
                   RL_ATTACHMENT_TEXTURE2D, 0);
```

### 2. **Light Camera Setup** (BROKEN)
```cpp
// WRONG: Using light entity position
lightCamera_.position = lightTransform->position; // ❌ Wrong!
lightCamera_.target = Vector3Add(lightTransform->position, {0.0f, -1.0f, 0.0f});

// FIXED: Now using proper directional light setup
lightCamera_.position = Vector3Scale(lightDir, -25.0f); // ✅ Correct
lightCamera_.target = Vector3Zero();
```

### 3. **Render Loop - PASS 1: Shadow Map Generation**
```cpp
// Begin shadow rendering
BeginShadowMode(*depthShader); // Switch to depth shader

BeginTextureMode(lightSystem->GetShadowMap());
    ClearBackground(BLANK); // Clear to black (near depth)
    rlEnableDepthTest();
    rlEnableDepthMask();

    // Render entities with depth shader
    for (Entity* entity : shadowCastingEntities) {
        RenderCommand cmd(entity, transform, nullptr, mesh, nullptr, RenderType::MESH_3D);
        renderer_.DrawMesh3D(cmd); // Uses depth shader automatically
    }
EndTextureMode();
EndShadowMode();
```

### 4. **Render Loop - PASS 2: Final Rendering**
```cpp
// Bind shadow texture
rlActiveTextureSlot(10);
rlEnableTexture(shadowMap.depth.id);
rlSetUniform(shadowMapLoc, &textureSlot, SHADER_UNIFORM_INT, 1);

// Set uniforms
SetShaderValueMatrix(lightingShader, lightVPLoc, lightVPMatrix);

// Render with lighting shader
// Shader samples shadowMap texture and applies shadows
```

## Key Differences and Issues

### ✅ **What's Working**
- Depth buffer creation (matches Raylib)
- Shadow map texture binding
- Uniform setting
- Two-pass rendering approach

### ❌ **Critical Issues Found**

#### 1. **Depth Shader Missing from Source**
**Problem**: We reference `"depth/depth.vs"` and `"depth/depth.fs"` but these files don't exist in `src/shaders/`
**Impact**: Depth rendering may fail or use wrong shader
**Solution**: Create proper depth shaders in source

#### 2. **Light Camera Positioning (FIXED)**
**Problem**: Was using light entity position instead of proper directional light positioning
**Raylib**: `lightCamera.position = Vector3Scale(lightDir, -15.0f)`
**Our Fix**: Now uses `Vector3Scale(lightDir, -25.0f)`

#### 3. **Depth Clear Color**
**Raylib**: `ClearBackground(WHITE)` (far depth = white)
**Our Code**: `ClearBackground(BLANK)` (near depth = black)
**Impact**: May cause incorrect depth values

#### 4. **Shader Approach**
**Raylib**: Single shader handles both lighting and shadows
**Our Code**: Separate depth shader + lighting shader with shadows
**Status**: Both approaches valid, but ours is more complex

#### 5. **Texture Coordinate Flipping**
**Issue**: OpenGL texture coordinates may need Y-flip
**Our Fix**: Added `sampleCoords.y = 1.0 - sampleCoords.y`

## Required Fixes

### 1. **Create Depth Shaders in Source**
Need to create:
- `src/shaders/depth/depth.vs`
- `src/shaders/depth/depth.fs`

### 2. **Fix Depth Clear Color**
Change `ClearBackground(BLANK)` to `ClearBackground(WHITE)` to match Raylib convention.

### 3. **Verify Shadow Map Resolution**
Ensure `SHADOW_MAP_RESOLUTION` matches between shader and C++ code.

### 4. **Add Shadow Map Debugging**
Add code to visualize shadow map contents for debugging.

## Test Cases

1. **Shadow Map Content**: Add debug rendering of shadow map to screen
2. **Light Camera Frustum**: Visualize light camera position/orientation
3. **Depth Buffer Values**: Check if depth values are reasonable
4. **Coordinate Systems**: Verify light space transformations

## Success Criteria

- ✅ Shadow map contains valid depth data
- ✅ Light camera positioned correctly for scene coverage
- ✅ Shadow sampling coordinates are valid
- ✅ Shadow factor calculations produce 0.0-1.0 range
- ✅ Visual shadows appear on geometry behind occluders</content>
</xai:function_call">SHADOW_MAPPING_FLOW_ANALYSIS.md
