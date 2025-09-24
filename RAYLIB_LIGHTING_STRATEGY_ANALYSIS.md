# Raylib Lighting Strategy Analysis
## Comprehensive Guide for PaintStrike Engine Integration

### Current Status
- **Entity meshes**: Not showing colors/materials correctly
- **Lights**: Purple debug light parsed but not visible 
- **BSP geometry**: Completely unlit, not connected to shader system
- **Problem**: RenderBSPGeometry bypasses our shader/lighting pipeline

---

## Raylib Lighting Approaches

### 1. **Raylib Built-in Lighting** ⭐ RECOMMENDED START
```c
// Method 1: rlgl immediate mode lighting  
rlLights lights[MAX_LIGHTS];
rlLight light = CreateLight(LIGHT_POINT, position, target, color, shader);
UpdateLightValues(shader, light);

// Method 2: Direct shader uniforms
SetShaderValue(shader, GetShaderLocation(shader, "lights[0].position"), &position, SHADER_UNIFORM_VEC3);
SetShaderValue(shader, GetShaderLocation(shader, "lights[0].color"), &color, SHADER_UNIFORM_VEC4);
```

**Pros:**
- ✅ Simple to implement
- ✅ Raylib handles uniform management
- ✅ Works with our existing shader system
- ✅ Good performance for 4-8 lights

**Cons:**
- ❌ Limited to ~8 lights max
- ❌ Forward rendering only
- ❌ No built-in light culling

**Integration with our ECS:**
```cpp
// In LightSystem::UpdateShaderLights()
for (auto& light : activeLights_) {
    SetShaderValue(shader, light.positionLoc, &light.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, light.colorLoc, &light.color, SHADER_UNIFORM_VEC4);
}
```

---

### 2. **Custom Forward Rendering** ⭐⭐ CURRENT APPROACH
Our current `lighting.fs` shader with light arrays.

**Pros:**
- ✅ Full control over lighting calculations
- ✅ Custom light types and effects
- ✅ Works with ECS architecture
- ✅ Supports instancing

**Cons:**
- ❌ More complex to debug
- ❌ Requires manual uniform management
- ❌ Performance scales with light count

**Current Issues:**
- Uniforms not being set correctly
- BSP geometry bypasses lighting shader
- Material colors not applying

---

### 3. **Deferred Lighting** ⭐⭐⭐ FUTURE CONSIDERATION
```c
// G-Buffer approach
RenderTexture2D gBuffer = LoadRenderTexture(screenWidth, screenHeight);
// Render geometry to G-Buffer, then lights in screen space
```

**Pros:**
- ✅ Excellent for many lights (100+)
- ✅ Constant fragment cost per light
- ✅ Good for complex scenes

**Cons:**
- ❌ Complex implementation
- ❌ Memory intensive
- ❌ Transparency challenges
- ❌ Overkill for current needs

---

### 4. **Lightmaps** ⭐⭐⭐ IDEAL FOR BSP
Pre-baked lighting textures for static geometry.

**Quake-style Implementation:**
```c
// Each BSP face has lightmap UV coordinates
struct BSPFace {
    Vector2 lightmapUV[4];
    int lightmapTextureId;
};
```

**Pros:**
- ✅ Perfect for BSP static geometry
- ✅ Handles complex lighting (shadows, GI)
- ✅ Excellent performance
- ✅ Supports any number of lights

**Cons:**
- ❌ Only for static geometry
- ❌ Complex lightmap generation
- ❌ Large texture memory usage
- ❌ No dynamic lighting

**Hybrid Approach:**
- Lightmaps for BSP world geometry
- Dynamic lights for entities and effects

---

### 5. **Light Clustering/Tiled** ⭐⭐ ADVANCED OPTION
Divide screen into tiles, assign lights per tile.

**Pros:**
- ✅ Scales to hundreds of lights
- ✅ GPU-friendly culling
- ✅ Good for large open areas

**Cons:**
- ❌ Very complex implementation
- ❌ Requires compute shaders
- ❌ Overkill for arena shooter

---

## Specific BSP Integration Strategies

### Option A: **Shader Integration** (Immediate Fix)
```cpp
// In Renderer::RenderBSPGeometry()
void Renderer::RenderFace(const BSPFace& face) {
    if (hasCurrentShader_) {
        rlSetShader(currentShader_->id, currentShader_->locs);
    }
    
    // Render face with lighting shader
    rlBegin(RL_TRIANGLES);
    // ... vertex data with normals ...
    rlEnd();
}
```

### Option B: **Lightmap System** (Long-term)
```cpp
class LightmapSystem {
    std::unordered_map<int, Texture2D> lightmaps_;
    void BakeLightmaps(const std::vector<LightComponent*>& lights);
    void ApplyLightmapToFace(int faceId, const BSPFace& face);
};
```

### Option C: **Hybrid Approach** (Best of both)
- Static lightmaps for world geometry
- Dynamic forward lighting for entities
- Light probes for entity lighting in world

---

## Recommended Implementation Path

### Phase 1: **Fix Current System** (1-2 days)
1. ✅ Fix entity material/color application
2. ✅ Connect BSP geometry to shader system  
3. ✅ Verify lighting shader uniforms are set
4. ✅ Test with simple directional light

### Phase 2: **Improve Forward Lighting** (3-5 days)
1. Implement proper light culling
2. Add shadow mapping for key lights
3. Optimize uniform updates
4. Add more light types (area, etc.)

### Phase 3: **BSP Lightmaps** (1-2 weeks)
1. Generate lightmap UVs for BSP faces
2. Implement lightmap baking
3. Runtime lightmap updates for dynamic lights
4. Hybrid rendering pipeline

### Phase 4: **Advanced Features** (Future)
1. Deferred rendering option
2. Light clustering for large scenes
3. Global illumination
4. Post-processing pipeline

---

## Immediate Action Items

### 1. **Fix Entity Materials** (Critical)
```cpp
// In MaterialSystem::ApplyMaterialToModel()
// Ensure colDiffuse uniform is set for solid colors
SetShaderValue(shader, GetShaderLocation(shader, "colDiffuse"), &color, SHADER_UNIFORM_VEC4);
```

### 2. **Connect BSP to Shaders** (Critical)  
```cpp
// In Renderer::RenderBSPGeometry()
void Renderer::SetCurrentShader(const Shader& shader) {
    currentShader_ = &shader;
    // Apply to all subsequent BSP face rendering
}
```

### 3. **Debug Light Uniforms** (Critical)
```cpp
// In LightSystem::UpdateShaderLights()
int loc = GetShaderLocation(shader, "lights[0].position");
if (loc != -1) {
    LOG_INFO("✅ Found light uniform at location: " + std::to_string(loc));
    SetShaderValue(shader, loc, &position, SHADER_UNIFORM_VEC3);
} else {
    LOG_ERROR("❌ Light uniform not found in shader");
}
```

---

## Testing Strategy

### Test 1: **Basic Lighting**
- Single directional light
- Simple diffuse calculation
- Verify on both entities and BSP

### Test 2: **Multiple Lights**
- 2-3 point lights
- Check performance impact
- Verify light falloff

### Test 3: **Material Integration**
- Solid colors on meshes
- Gradient textures
- Normal BSP textures with lighting

---

## Performance Considerations

| Approach | Lights | Draw Calls | Memory | Complexity |
|----------|--------|------------|---------|------------|
| Forward | 4-8 | Same | Low | Simple |
| Deferred | 100+ | +G-Buffer | High | Complex |
| Lightmaps | Unlimited | Same | High | Medium |
| Clustered | 100+ | Same | Medium | Very Complex |

**Recommendation:** Start with Forward lighting (4-8 lights), then add Lightmaps for BSP geometry.

---

## Conclusion

**Immediate Fix:** Forward lighting with proper shader integration
**Long-term Goal:** Hybrid Forward + Lightmaps system
**Best Performance:** Lightmaps for static world + Forward for dynamic entities

The key is getting the basic forward lighting working first, then building on that foundation.
