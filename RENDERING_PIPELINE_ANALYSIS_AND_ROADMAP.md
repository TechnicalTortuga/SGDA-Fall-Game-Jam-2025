# Rendering Pipeline Analysis & Roadmap

## Current Status Analysis

### 🔍 Problem Identified: Shader-LightSystem Mismatch

**Root Cause**: Our `LightSystem` is correctly parsing, caching, and managing lights, but our `basic.fs` shader doesn't support dynamic lighting uniforms.

**Evidence from Logs**:
- ✅ Lights are being parsed: "main_ceiling_light", "accent_light_blue", "dramatic_spot", "ambient_sun"
- ✅ LightSystem is updating: "LightCache cache HIT for ID 1,2,3,4"
- ✅ UpdateShaderLights is being called: "🔆 Updated lighting for basic shader"
- ❌ **But the basic.fs shader only has hardcoded lighting**

**Current basic.fs shader problems**:
```glsl
// HARDCODED - No dynamic lights support
vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Fixed light direction
float ambient = 0.3; // Fixed ambient
```

**LightSystem expects these uniforms (from Raylib examples)**:
```glsl
uniform int numOfLights;
uniform Light lights[MAX_LIGHTS]; // Array of dynamic lights
uniform vec3 ambientColor;
uniform float ambient;
```

---

## Current Architecture Assessment

### ✅ What's Working Well

1. **ECS Architecture**: Clean separation of concerns
   - `LightComponent`: Pure data storage
   - `LightSystem`: Logic and processing
   - `CacheSystem`: Performance optimization

2. **Light Data Flow**: Excellent pipeline
   ```
   YAML → MapLoader → EntityFactory → LightComponent → LightSystem → CacheSystem
   ```

3. **Cache Integration**: Proper deduplication and performance
   - `LightCacheKey` for unique identification
   - `CachedLightData` with `RaylibLight` conversion
   - Reference counting and efficient lookups

4. **System Integration**: Well-coordinated
   - `WorldSystem` registers lights with `LightSystem`
   - `RenderSystem` calls `UpdateShaderLights()`
   - Proper initialization order in `Engine.cpp`

### ❌ Critical Issues

1. **Shader Compatibility Gap**
   - Basic shader: Hardcoded lighting only
   - PBR shader: Falls back to basic shader
   - No dynamic lighting uniform arrays

2. **Missing Lighting Shaders**
   - No proper lighting fragment shaders
   - No instancing support for lights
   - No PBR implementation

3. **Rendering Pipeline Gaps**
   - Forward rendering without proper light accumulation
   - No deferred rendering option
   - No post-processing integration

---

## Raylib Lighting Best Practices Analysis

Based on our `Advanced Raylib Examples` and research:

### 1. Raylib Light Structure
```c
typedef struct {
    int type;           // LIGHT_DIRECTIONAL, LIGHT_POINT, LIGHT_SPOT
    int enabled;
    Vector3 position;
    Vector3 target;
    float color[4];     // RGBA normalized (0.0-1.0)
    float intensity;
    
    // Shader uniform locations (cached)
    int typeLoc;
    int enabledLoc;
    int positionLoc;
    int targetLoc;
    int colorLoc;
    int intensityLoc;
} Light;
```

### 2. Proper Shader Uniforms
```glsl
#define MAX_LIGHTS 4

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
    float intensity;
};

uniform Light lights[MAX_LIGHTS];
uniform int numOfLights;
uniform vec3 ambientColor;
uniform float ambient;
uniform vec3 viewPos;
```

### 3. Light Creation Pattern
```c
// From Raylib examples:
Light CreateLight(int type, Vector3 position, Vector3 target, Color color, float intensity, Shader shader) {
    // Get shader uniform locations
    light.enabledLoc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", lightCount));
    light.typeLoc = GetShaderLocation(shader, TextFormat("lights[%i].type", lightCount));
    // ... etc
    UpdateLight(shader, light);
}
```

---

## Modern Rendering Pipeline Architecture

### Forward vs Deferred Rendering

**Current State**: Basic forward rendering with single light
**Recommendation**: Enhanced forward rendering with multiple lights

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Geometry      │───▶│  Lighting Pass   │───▶│ Post-Processing │
│   Pass          │    │  (Forward/       │    │    Pass        │
│                 │    │   Deferred)      │    │                │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
    ┌─────────┐            ┌─────────┐            ┌─────────┐
    │ Meshes  │            │ Lights  │            │ Effects │
    │Materials│            │Shadows  │            │ HDR     │
    │Textures │            │ PBR     │            │ Bloom   │
    └─────────┘            └─────────┘            └─────────┘
```

### Recommended Pipeline Flow

#### 1. **Geometry Stage** (Current - Working)
- BSP culling ✅
- Mesh rendering ✅
- Material application ✅
- Texture binding ✅

#### 2. **Lighting Stage** (Needs Fix)
- Light uniform updates ❌ (shader mismatch)
- Multi-light accumulation ❌
- Shadow mapping ❌
- PBR calculations ❌

#### 3. **Post-Processing Stage** (Not Started)
- HDR tone mapping ❌
- Bloom effects ❌
- Ambient occlusion ❌
- Anti-aliasing ❌

---

## Immediate Action Plan

### Phase 1: Fix Basic Lighting (High Priority)

1. **Update basic.fs shader** to support dynamic lights:
```glsl
#version 330

#define MAX_LIGHTS 4

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
    float intensity;
};

uniform Light lights[MAX_LIGHTS];
uniform int numOfLights;
uniform vec3 ambientColor;
uniform float ambient;
uniform vec3 viewPos;

// ... rest of shader with proper light calculations
```

2. **Update LightSystem::UpdateShaderLight()** to match uniform names:
```cpp
// Fix uniform location names to match shader
GetShaderLocation(shader, TextFormat("lights[%i].enabled", lightIndex));
GetShaderLocation(shader, TextFormat("lights[%i].type", lightIndex));
// etc.
```

3. **Validate light count and limits**:
```cpp
// Ensure MAX_LIGHTS consistency between C++ and GLSL
static_assert(LightSystem::MAX_LIGHTS == 4, "Light count mismatch");
```

### Phase 2: PBR Implementation (Medium Priority)

1. **Create proper PBR shader**:
   - Based on Raylib PBR example
   - Support for albedo, metallic, roughness, normal maps
   - Proper light accumulation

2. **Material system enhancements**:
   - PBR material properties
   - Texture slot management
   - Shader parameter binding

3. **Advanced lighting features**:
   - Shadow mapping
   - Point/spot light attenuation
   - Directional light cascaded shadows

### Phase 3: Post-Processing Pipeline (Low Priority)

1. **Framebuffer management**:
   - HDR render targets
   - Multi-sample anti-aliasing
   - Depth/stencil buffers

2. **Effect implementations**:
   - Bloom (for emissive materials)
   - Tone mapping (HDR → LDR)
   - Gamma correction
   - Screen-space ambient occlusion

---

## Performance Considerations

### Current Performance Profile
- **BSP Culling**: ✅ Efficient (0% culled in test scene)
- **Material Batching**: ✅ Working
- **Model Caching**: ✅ Cache hits for all meshes
- **Light Caching**: ✅ Cache hits for all lights

### Optimization Opportunities

1. **Instanced Rendering**:
   - Multiple objects with same mesh/material
   - Transform arrays uploaded to GPU
   - Single draw call for many instances

2. **Light Culling**:
   - Frustum culling for lights
   - Distance-based attenuation cutoff
   - Light occlusion queries

3. **Shader Optimization**:
   - Uber-shader variants
   - Dynamic branching minimization
   - Uniform buffer objects

---

## Integration with Game Systems

### BSP Integration
- **Current**: BSP geometry renders separately from dynamic objects
- **Recommendation**: Apply same lighting to both BSP and dynamic meshes
- **Implementation**: Ensure both use same shader uniforms

### Material System Integration
- **Current**: Materials work with basic textures
- **Enhancement**: PBR material properties
- **Future**: Paint system integration for gameplay

### Cache System Optimization
- **Current**: Separate caches for models, materials, lights
- **Enhancement**: Cross-cache dependencies
- **Future**: GPU-side caching for instancing

---

## Debugging and Validation Tools

### Immediate Tools Needed

1. **Light Visualization**:
```cpp
// Render debug spheres at light positions
void RenderLightDebugSpheres() {
    for (auto light : activeLights_) {
        Vector3 pos = GetLightPosition(light);
        DrawSphere(pos, 0.2f, light->color);
    }
}
```

2. **Shader Uniform Validation**:
```cpp
// Log all light uniform locations
void ValidateShaderUniforms(Shader& shader) {
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        int enabledLoc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", i));
        LOG_DEBUG("Light[" + std::to_string(i) + "].enabled location: " + std::to_string(enabledLoc));
    }
}
```

3. **Performance Profiling**:
```cpp
// Time critical rendering sections
auto start = std::chrono::high_resolution_clock::now();
UpdateShaderLights(shader);
auto end = std::chrono::high_resolution_clock::now();
LOG_DEBUG("Light update time: " + std::to_string(duration.count()) + "ms");
```

---

## Next Steps

### Immediate (This Session)
1. ✅ **Analysis Complete**: Identified shader-light system mismatch
2. 🔄 **Create Updated basic.fs**: Add dynamic lighting support
3. 🔄 **Fix LightSystem uniforms**: Match shader expectations
4. 🔄 **Test lighting**: Verify lights appear in scene

### Short Term (Next Session)
1. **PBR shader implementation**: Full lighting model
2. **Shadow mapping**: Basic directional shadows
3. **Performance validation**: Ensure 60fps with 4 lights

### Long Term (Future Sessions)
1. **Post-processing pipeline**: HDR, bloom, tone mapping
2. **Advanced features**: Instanced rendering, light culling
3. **Paint system integration**: Gameplay-specific lighting

---

## Conclusion

Our lighting architecture is **fundamentally sound** - the ECS design, caching system, and data flow are excellent. The issue is simply a **shader compatibility gap** that can be resolved quickly.

The `LightSystem` is working perfectly and following Raylib best practices. We just need the shaders to match the expected uniform interface.

**Priority 1**: Fix basic.fs shader to support dynamic lights
**Priority 2**: Implement proper PBR shader
**Priority 3**: Add post-processing pipeline

This will give us a robust, performant, and extensible rendering system suitable for Paint Strike's gameplay needs.
