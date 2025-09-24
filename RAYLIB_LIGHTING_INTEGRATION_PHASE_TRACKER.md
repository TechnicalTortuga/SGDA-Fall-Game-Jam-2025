# Raylib Lighting Integration Phase Tracker
## Comprehensive Implementation Plan for PaintStrike Engine

---

## 🔍 **CURRENT ARCHITECTURE ANALYSIS**

### **UPDATED Current Render Pipeline Flow** *(Analysis Completed: 2025-01-24)*
```
RenderSystem::ExecuteRenderCommands()
├── UNIFIED SHADER PIPELINE (Phase 2.1) - ✅ PARTIALLY IMPLEMENTED
│   ├── Get LightingShader from ShaderSystem ✅ WORKING
│   ├── lightSystem->UpdateShaderLights()   ✅ WORKING
│   └── BeginShaderMode(lightingShader)     ✅ APPLIED TO ALL
├── RenderWorldGeometryDirect()            ✅ NOW WITH SHADER
│   └── BSP geometry gets lighting         ✅ UNIFIED PIPELINE
├── renderer_.BeginFrame()                 ✅ 
├── Entity Rendering                       ✅ SAME SHADER
│   ├── DrawRenderCommand()               
│   └── materialSystem->ApplyMaterial()   
└── ❌ MISSING EndShaderMode()             ❌ CRITICAL BUG
```

### **Current Light System Architecture**
```cpp
LightComponent (ECS Component)
├── LightType: POINT, SPOT, DIRECTIONAL
├── Color, Intensity, Range, Angles
├── Shadow properties
└── System integration IDs

LightSystem (ECS System)  
├── LightCache (flyweight pattern)
├── RaylibLight conversion
├── Shader uniform updates ❌ WRONG FORMAT
└── activeLights_ tracking

CacheSystem<LightCacheKey, CachedLightData>
├── Key generation
├── Data factory
└── Reference counting
```

### **UPDATED Issues Identified** *(Research Completed: 2025-01-24)*
**✅ FIXED ISSUES:**
1. **✅ BSP Integration**: World geometry now gets lighting via unified shader pipeline
2. **✅ Timing**: Lighting updates happen BEFORE rendering in correct sequence
3. **✅ BeginShaderMode**: Now properly applied to unified pipeline
4. **✅ Unified Pipeline**: BSP and entities use the same lighting shader

**🎉 MAJOR PROGRESS UPDATE - September 24, 2025:**

**✅ COMPLETED MAJOR MILESTONES:**
1. **✅ LIGHTING SYSTEM WORKING** - 5 lights rendering successfully (3 point, 1 directional, 1 spot)
2. **✅ SHADER PIPELINE UNIFIED** - Both BSP world geometry and entities use same lighting shader
3. **✅ MAX_LIGHTS ISSUE RESOLVED** - Reduced from 1000 to 16, eliminated GPU uniform array size issues
4. **✅ UNIFORM LOCATIONS WORKING** - All light uniforms found and applied correctly
5. **✅ DISTANCE ATTENUATION** - Point lights properly fall off with distance
6. **✅ IMPROVED BLENDING** - Added tone mapping, reduced oversaturation, softer transitions
7. **✅ REDUCED REFLECTIVITY** - Materials now appear matte instead of metallic
8. **✅ BETTER BRIGHTNESS** - Lighting levels more realistic and comfortable

**🚀 CURRENT ACTIVE DEVELOPMENT:**

**🔧 HIGH PRIORITY TASKS (In Progress):**
1. **🚧 SHADOW MAPPING** - Researched Raylib implementation, adding depth buffer support
2. **✅ VISIBLE LIGHT SOURCES** - ✅ COMPLETED - Implemented light gizmo rendering system:
   - Point lights: Sphere with wireframe range indicator scaled to radius field
   - Directional lights: Arrow with sun-like rays showing typical downward direction  
   - Spot lights: Cone wireframe using outerAngle and range fields
   - Integration: LIGHT_GIZMO render type, RenderSystem auto-detects light entities
3. **🔧 SPOT LIGHT CONES** - Currently treating spots like points, need cone calculations  
4. **🔧 LIGHT CULLING** - Restore MAX_LIGHTS to 1000 with distance-based culling

**✅ RECENTLY FIXED ISSUES:**
1. **✅ Missing EndShaderMode()**: Pipeline properly closes shader mode  
2. **✅ Data Structure Fixed**: RaylibLight now uses correct `bool enabled` and `float attenuation`
3. **✅ Attenuation Working**: Proper distance-based light falloff implemented
4. **❌ No Light Culling**: Sending all 1000 lights to shader every frame (massive GPU waste)
5. **❌ Missing ViewPos Uniform**: Required for specular lighting calculations
6. **❌ Missing activeLightCount**: Shader loops through all 1000 lights even when only 2-3 active

**🔍 NEW FINDINGS FROM RAYLIB RESEARCH:**
- **Official rlights.h**: Uses 4 lights max because it sends ALL to shader every frame
- **Smart Solution**: Keep 1000 world lights, cull to 4-8 closest/brightest for GPU
- **PBR Support**: Victor Fisac's rPBR provides advanced PBR + shadow mapping  
- **Standard Format**: Must use `bool enabled` and `float attenuation`
- **Performance Strategy**: Distance culling + priority sorting = Best of both worlds
- **CreateLight()/UpdateLightValues()**: Official helper functions available

---

## 🎯 **PHASE 1: CRITICAL FIXES** *(Updated Priority)*
**Target: Fix immediate issues preventing lighting from working**

### **1.1 Fix Missing EndShaderMode()** ⭐ **HIGHEST PRIORITY**
- **Files to Modify**: 
  - `src/ecs/Systems/RenderSystem.cpp` → ExecuteRenderCommands()
- **Issue**: BeginShaderMode() called but never ended
- **Fix**: Add `EndShaderMode()` after all rendering is complete
- **Test**: Verify no OpenGL state corruption

### **1.2 Fix Data Structure Compatibility** ⭐ **CRITICAL**
- **Files to Modify**:
  - `src/ecs/Systems/CacheSystem.h` → RaylibLight struct
- **Changes**:
  ```cpp
  struct RaylibLight {
      int type;            // Keep as int (matches Raylib)
      bool enabled;        // ❌ CHANGE from int to bool 
      Vector3 position;    // ✅ Correct
      Vector3 target;      // ✅ Correct
      float color[4];      // ✅ Correct
      float attenuation;   // ❌ ADD this field (remove intensity)
      
      // Shader locations (keep existing)
      int typeLoc, enabledLoc, positionLoc, targetLoc, colorLoc;
      int attenuationLoc = -1;  // ❌ ADD this
  };
  ```
- **Expected Result**: Perfect compatibility with official Raylib rlights.h

### **1.3 Add ViewPos Uniform Update** ⭐ **CRITICAL**
- **Files to Modify**:
  - `src/ecs/Systems/LightSystem.cpp` → UpdateShaderLights()
- **Changes**: Add camera position uniform for specular calculations
  ```cpp
  // Add to UpdateShaderLights()
  Vector3 cameraPos = GetCameraPosition();
  int viewPosLoc = GetShaderLocation(shader, "viewPos");
  if (viewPosLoc != -1) {
      float viewPos[3] = {cameraPos.x, cameraPos.y, cameraPos.z};
      SetShaderValue(shader, viewPosLoc, viewPos, SHADER_UNIFORM_VEC3);
  }
  ```

### **1.4 Add activeLightCount Uniform** ⭐ **BASIC FUNCTIONALITY**
- **Files to Modify**:
  - `src/ecs/Systems/LightSystem.cpp` → UpdateShaderLights()
  - `build/bin/shaders/lighting/lighting.fs` → Loop optimization
- **Strategy**: Keep MAX_LIGHTS = 1000, but tell shader how many are actually active
- **Changes**:
  ```cpp
  // In UpdateShaderLights() - after building shaderLights_:
  int activeLightCount = static_cast<int>(shaderLights_.size());
  int lightCountLoc = GetShaderLocation(shader, "activeLightCount");
  if (lightCountLoc != -1) {
      SetShaderValue(shader, lightCountLoc, &activeLightCount, SHADER_UNIFORM_INT);
  }
  ```
- **Shader Update**:
  ```glsl
  uniform int activeLightCount;  // How many lights to actually process
  
  // In main(), change loop from:
  for (int i = 0; i < min(MAX_LIGHTS, 1000); i++)
  // To:
  for (int i = 0; i < min(activeLightCount, MAX_LIGHTS); i++)
  ```
- **Expected Result**: Shader only processes actual active lights, not all 1000

### **1.5 Integrate Official rlights.h Helper Functions** ⭐ **OPTIONAL**
- **Files to Create**: `src/lighting/RaylibLights.h` (copy from samples)
- **Files to Modify**: `src/ecs/Systems/LightSystem.h/.cpp`
- **Changes**: Use `CreateLight()` and `UpdateLightValues()` functions
- **Expected Result**: Standard Raylib light management

---

## 🎯 **PHASE 2: UNIFIED SHADER PIPELINE**
**Target: Apply lighting shaders to both BSP and entity rendering**

### **2.1 Restructure Render Pipeline**
- **Files to Modify**: `src/ecs/Systems/RenderSystem.cpp`
- **New Flow**:
  ```cpp
  RenderSystem::ExecuteRenderCommands()
  ├── Setup Lighting (ONCE per frame)
  │   ├── Get lighting shader
  │   ├── Update light uniforms  
  │   └── Update camera position
  ├── BeginShaderMode(lightingShader)     🆕 UNIFIED
  │   ├── RenderWorldGeometryDirect()     ✅ NOW WITH LIGHTING
  │   └── Entity Rendering                ✅ SAME SHADER
  └── EndShaderMode()                     🆕 UNIFIED
  ```

### **2.2 Modify BSP Rendering**
- **Files to Modify**: `src/rendering/Renderer.cpp` → RenderFace()
- **Changes**: Remove manual shader application, let BeginShaderMode handle it
- **Expected Result**: BSP faces inherit lighting from BeginShaderMode

### **2.3 Unified Material System**
- **Files to Modify**: `src/ecs/Systems/MaterialSystem.cpp`
- **Changes**: Ensure materials work with Raylib lighting shader
- **Expected Result**: Both BSP and entities use same material pipeline

---

## 🎯 **PHASE 3: LIGHT CACHE OPTIMIZATION** 
**Target: Optimize light data and implement instancing**

### **3.1 Update Light Cache Format**
- **Files to Modify**: `src/ecs/Systems/CacheSystem.h`
- **Changes**:
  ```cpp
  struct RaylibLight {
      // Match rlights.h EXACTLY
      int type;
      bool enabled;
      Vector3 position;
      Vector3 target;
      Color color;        // Keep as Color, convert to float[4] only for shader
      float attenuation;  // Add missing field
      
      // Shader location cache (for performance)
      int enabledLoc, typeLoc, positionLoc, targetLoc, colorLoc, attenuationLoc;
  };
  ```

### **3.2 Implement Light Instancing**
- **Files to Create**: `src/lighting/LightInstancer.h/.cpp`
- **Purpose**: Batch identical lights for performance
- **Features**:
  ```cpp
  class LightInstancer {
      std::vector<InstancedLight> instances_;
      void BatchIdenticalLights(const std::vector<RaylibLight>& lights);
      void UpdateShaderInstances(Shader& shader);
  };
  ```

### **3.3 Cache Performance Optimization**
- **Files to Modify**: `src/ecs/Systems/LightSystem.cpp`
- **Optimizations**:
  - Cache shader locations between frames
  - Only update dirty lights
  - Frustum cull lights outside view
  - Sort lights by distance for falloff

---

## 🎯 **PHASE 4: ADVANCED LIGHTING FEATURES**
**Target: Add shadows, PBR, and post-processing support**

### **4.1 Shadow Mapping Integration**
- **Files to Copy**: `shadowmap.fs/vs` from Raylib samples
- **Files to Modify**: `src/ecs/Systems/ShadowSystem.h/.cpp` (new)
- **Features**: Point light and directional light shadows

### **4.2 PBR Shader Support**
- **Files to Copy**: `pbr.fs/vs` from Raylib samples  
- **Files to Modify**: `src/ecs/Systems/ShaderSystem.cpp`
- **Purpose**: Advanced materials for entities

### **4.3 Post-Processing Pipeline**
- **Files to Create**: `src/rendering/PostProcessor.h/.cpp`
- **Features**: Bloom, tone mapping, gamma correction

---

## 🎯 **PHASE 5: WEBGL DEPLOYMENT PREP**
**Target: Ensure Emscripten compatibility**

### **5.1 WebGL Shader Validation**
- **Test**: All #version 330 shaders compile under Emscripten
- **Files to Check**: All shader files in `build/bin/shaders/`

### **5.2 Performance Profiling**  
- **Tools**: Browser dev tools, Raylib FPS counter
- **Targets**: 60fps at 1080p, <16ms frame time

### **5.3 Asset Optimization**
- **Textures**: Convert to WebP where supported
- **Audio**: Convert to OGG for web compatibility
- **Models**: Optimize polygon counts

---

## 📊 **SUCCESS METRICS**

### **Phase 1 Success Criteria**
- [ ] Raylib lighting shaders load without errors
- [ ] Purple debug light visible in scene  
- [ ] Console shows lighting uniform updates
- [ ] No shader compilation errors

### **Phase 2 Success Criteria**
- [ ] BSP geometry shows lighting effects
- [ ] Entity meshes show same lighting
- [ ] Single shader pipeline for both
- [ ] No visual regressions

### **Phase 3 Success Criteria**
- [ ] Light cache hit rate >90%
- [ ] 4+ lights render at 60fps
- [ ] Frustum culling working
- [ ] Clean debug logs

### **Phase 4 Success Criteria**
- [ ] Shadow mapping functional
- [ ] PBR materials working
- [ ] Post-processing pipeline

### **Phase 5 Success Criteria**
- [ ] Game runs in browser at 60fps
- [ ] WebGL deployment successful
- [ ] All assets load correctly

---

## 🔧 **UPDATED IMPLEMENTATION ORDER** *(Research-Based Priority)*

### **IMMEDIATE FIXES TO MAKE LIGHTS VISIBLE** ⭐ **START HERE - DAY 1**
1. **Phase 1.1**: Fix Missing EndShaderMode() ⭐ **15 minutes**
2. **Phase 1.3**: Add ViewPos uniform update ⭐ **30 minutes**
3. **Phase 1.4**: Add activeLightCount uniform ⭐ **20 minutes**
4. **Phase 1.2**: Fix RaylibLight data structure ⭐ **45 minutes**
5. **Test**: Verify lights are visible and affecting geometry ⭐ **PRIORITY**

**TOTAL TIME: ~2 hours to get basic lighting working**

### **ADVANCED INTEGRATION** - **DAY 2-3** 
6. **Phase 1.5**: Integrate official rlights.h (optional)
7. **Phase 3.1**: Optimize cache format for new structure
8. **Test**: Performance benchmarks with 4 lights

### **FUTURE ENHANCEMENTS** - **WEEK 2+**
9. **Phase 4.1**: Shadow mapping (Victor Fisac's rPBR integration)
10. **Phase 4.2**: Full PBR support pipeline  
11. **Phase 5.1**: WebGL deployment validation
12. **Test**: Full-featured lighting system

### **RESEARCH-BASED RECOMMENDATIONS:**
- **Get Lights Working First**: Focus on visibility before optimization
- **Keep MAX_LIGHTS = 1000**: Full world lighting capacity maintained
- **Smart Optimization Later**: Distance culling and prioritization after basic functionality
- **Use Proven Solutions**: Integrate Victor Fisac's rPBR for advanced features when ready
- **WebGL Ready**: All changes maintain Emscripten compatibility

---

## 🚨 **CRITICAL DEPENDENCIES**

1. **Raylib Compatibility**: Must use exact rlights.h uniform naming
2. **Pipeline Unification**: BeginShaderMode must wrap ALL rendering
3. **Cache Integration**: Light changes must invalidate cache properly
4. **Performance**: 4 lights at 60fps is minimum requirement
5. **WebGL**: All shaders must be #version 330 compatible

---

## 📝 **IMPLEMENTATION NOTES**

### **Immediate Actions Required**
1. Copy Raylib's `lighting.fs/vs` to replace our custom shaders
2. Update `LightSystem::UpdateShaderLights()` uniform names
3. Wrap BSP rendering in `BeginShaderMode()`
4. Test with existing purple debug light

### **Architecture Decisions**
- **Single Pipeline**: Use BeginShaderMode for everything
- **Cache First**: Always check light cache before creation
- **Raylib Standard**: Follow rlights.h conventions exactly
- **Performance**: Profile every change, maintain 60fps

### **Risk Mitigation**
- **Backup Current**: Git commit before major changes
- **Incremental**: Test each phase independently
- **Fallback**: Keep basic shader as emergency fallback
- **Monitor**: Add extensive logging during development

---

## 🎯 **READY FOR PHASE 1 IMPLEMENTATION**

The analysis is complete. Our current system has good architecture but needs:
1. **Raylib shader replacement** (immediate)
2. **Pipeline unification** (critical) 
3. **Cache optimization** (performance)
4. **WebGL preparation** (deployment)

**Next Action: Begin Phase 1.1 - Replace lighting shaders with Raylib versions**
