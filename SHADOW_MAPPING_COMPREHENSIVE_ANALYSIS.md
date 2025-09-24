# Comprehensive Shadow Mapping Analysis

## Executive Summary

Our shadow mapping implementation is failing because we're not following the correct Raylib shadow mapping pattern. The issue is **light camera positioning and matrix capture**. Both reference implementations position the light camera relative to the main camera, not based on light direction.

## Reference Implementation Comparison

### Substack Reference (Raylib)

#### Camera Setup
```c
Camera camera_light = {0};
camera_light.target = camera_main.position;  // Target = main camera position
camera_light.position = Vector3Add(camera_main.position, (Vector3){400,400,400}); // Offset from main camera
camera_light.up = (Vector3){0,1,0};
camera_light.projection = CAMERA_ORTHOGRAPHIC;
```

#### Matrix Capture
```c
BeginTextureMode(shadowmap);
    BeginMode3D(camera_light);
        // Render scene
        Matrix matLightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    EndMode3D();
EndTextureMode();
```

#### Shader Usage
```c
rlEnableShader(shader_shadowmap.id);
rlSetUniformMatrix(GetShaderLocation(shader_shadowmap,"matLightVP"), matLightVP);
SetShaderValueTexture(shader_shadowmap, GetShaderLocation(shader_shadowmap,"texture_shadowmap"), shadowmap.texture);
```

### GameMaker Tutorial (General Concept)

#### Depth Map Creation
```glsl
// Vertex shader
v_depth = gl_Position.z;

// Fragment shader
gl_FragColor = vec4(v_depth, 0, 0, 1);
```

#### Shadow Sampling
```glsl
// Vertex shader
v_shadow = u_sha_view * gm_Matrices[MATRIX_WORLD] * vec4(in_Position, 1);

// Fragment shader
vec2 uv = p.xy / p.w * vec2(0.5,-0.5) + 0.5;
float dif = (texture2D(u_sha_map, uv).r - p.z) / p.w;
return clamp(dif * FADE + 2.0, 0.0, 1.0);
```

## Our Current Implementation Issues

### ❌ Critical Issue: Light Camera Positioning

**Our Code (WRONG)**:
```cpp
// Position based on light direction - INCORRECT for directional lights
Vector3 lightDir = Vector3Normalize((Vector3){ 0.35f, -1.0f, -0.35f });
lightCamera_.position = Vector3Scale(lightDir, -25.0f);
lightCamera_.target = Vector3Zero();
```

**Correct Approach (from Substack)**:
```cpp
// Position relative to main camera - CORRECT
lightCamera.position = Vector3Add(mainCamera.position, (Vector3){400,400,400});
lightCamera.target = mainCamera.position;
```

**Why This Matters**: Directional lights are infinite - they don't have a true "position". The light camera should be positioned to capture the view frustum from the main camera's perspective.

### ❌ Matrix Capture Issue

**Our Code**: We calculate matrices manually in `SetupLightCamera()`:
```cpp
Matrix lightView = GetCameraMatrix(lightCamera_);
Matrix lightProj = MatrixOrtho(-40.0f, 40.0f, -40.0f, 40.0f, 0.1f, 200.0f);
lightViewProj_ = MatrixMultiply(lightView, lightProj);
```

**Correct Approach (Substack)**: Capture matrices after `BeginMode3D()`:
```cpp
BeginMode3D(camera_light);
// Render scene
Matrix matLightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
EndMode3D();
```

**Why This Matters**: Raylib's `BeginMode3D()` sets up the actual OpenGL matrices. We need to capture them after this setup, not calculate them manually.

### ❌ Depth Buffer Setup

**Our Code**: Uses color attachment + depth attachment
**Substack**: Uses color attachment + depth texture

Both should work, but let me verify our depth buffer setup matches.

### ✅ What's Working

- Depth shader creation
- Shadow map rendering pipeline
- Texture binding and uniform setting
- Shader logic (mostly correct)

## Required Fixes

### 1. **Fix Light Camera Setup**
```cpp
void LightSystem::SetupLightCamera(Entity* directionalLightEntity) {
    // Get main camera position (we need to access this)
    Vector3 mainCamPos = GetMainCameraPosition(); // Need to implement this
    
    // Position light camera relative to main camera
    lightCamera_.position = Vector3Add(mainCamPos, (Vector3){400, 400, 400});
    lightCamera_.target = mainCamPos;
    lightCamera_.up = {0.0f, 1.0f, 0.0f};
    lightCamera_.projection = CAMERA_ORTHOGRAPHIC;
    lightCamera_.fovy = 20.0f;
}
```

### 2. **Fix Matrix Capture**
Move matrix capture to `RenderShadowsToTexture()`:
```cpp
BeginTextureMode(lightSystem->GetShadowMap());
    ClearBackground(WHITE);
    
    BeginMode3D(lightCamera);
        // Capture the actual matrices Raylib sets up
        Matrix lightView = rlGetMatrixModelview();
        Matrix lightProj = rlGetMatrixProjection();
        Matrix lightViewProj = MatrixMultiply(lightView, lightProj);
        
        // Set the matrix in the lighting shader
        int lightVPLoc = GetShaderLocation(*lightingShader, "lightVP");
        if (lightVPLoc != -1) {
            SetShaderValueMatrix(*lightingShader, lightVPLoc, lightViewProj);
        }
        
        // Render scene with depth shader
        // ... render entities ...
    EndMode3D();
EndTextureMode();
```

### 3. **Update Shader Approach**

The Substack approach uses a single shader that handles both lighting and shadows. Our current approach with separate shaders should work, but let me verify the matrix passing.

### 4. **Add Main Camera Access**

We need a way to get the main camera position. This requires accessing the renderer's camera.

## Implementation Plan

1. **Add main camera access to LightSystem**
2. **Update `SetupLightCamera()` to position relative to main camera**
3. **Move matrix capture to `RenderShadowsToTexture()`**
4. **Test with debug visualization**
5. **Verify shadow map contents**

## Expected Outcome

With these fixes, shadows should appear because:
- Light camera will be positioned to capture the relevant scene area
- Matrices will be correctly captured from Raylib's setup
- Shadow map will contain proper depth data
- Sampling will work correctly

## Alternative: Single Shader Approach

The Substack reference uses one shader for everything. We could simplify by:
1. Removing the separate depth shader
2. Using the lighting shader for both passes
3. Setting a uniform to indicate depth-only rendering

But our current approach should work once the camera/matrix issues are fixed.</content>
</xai:function_call">SHADOW_MAPPING_COMPREHENSIVE_ANALYSIS.md
