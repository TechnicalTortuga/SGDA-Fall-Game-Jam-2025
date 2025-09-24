# 🎯 Actionable Phase Tracker: Shader & Texture Fixes

**Date**: September 23, 2025  
**Goal**: Fix texture rendering → Implement shader caching → Add instancing support

---

## 📋 **Phase Overview**

| Phase | Priority | Duration | Risk Level | Dependencies |
|-------|----------|----------|------------|--------------|
| **Phase 1** | 🔴 CRITICAL | 30 min | LOW | None |
| **Phase 2** | 🟡 HIGH | 45 min | LOW | Phase 1 |
| **Phase 3** | 🟢 MEDIUM | 2-3 hours | MEDIUM | Phase 1,2 |
| **Phase 4** | 🔵 LOW | 3-4 hours | HIGH | Phase 1,2,3 |

---

## 🚨 **PHASE 1: Fix Shader Assignment Order (CRITICAL)**

### **🎯 Goal**: Fix textures not rendering on models
### **⏱️ Estimated Time**: 30 minutes
### **🎲 Risk Level**: LOW (targeted fix, easy rollback)

### **📁 Files to Modify**:
1. `src/ecs/Systems/MaterialSystem.cpp` - **PRIMARY TARGET**
2. `src/ecs/Systems/MaterialSystem.h` - **MINOR CHANGES**

### **🔧 Specific Changes**:

#### **MaterialSystem.cpp - `ApplyMaterialToModel()` method**:
```cpp
// CURRENT BROKEN CODE (lines ~265-275):
model.materials[meshIndex] = *raylibMaterial;  // ❌ Overwrites shader
if (shaderId > 0 && shaderSystem_) {
    shaderSystem_->ApplyShaderToModel(shaderId, model, meshIndex);  // Too late!
}

// NEW FIXED CODE:
if (meshIndex == -1) {
    // Apply to all materials
    for (int i = 0; i < model.materialCount; i++) {
        // Copy texture maps and properties BUT preserve material structure
        Material& targetMaterial = model.materials[i];
        
        // Copy texture maps
        targetMaterial.maps[MATERIAL_MAP_DIFFUSE] = raylibMaterial->maps[MATERIAL_MAP_DIFFUSE];
        targetMaterial.maps[MATERIAL_MAP_NORMAL] = raylibMaterial->maps[MATERIAL_MAP_NORMAL];
        targetMaterial.maps[MATERIAL_MAP_SPECULAR] = raylibMaterial->maps[MATERIAL_MAP_SPECULAR];
        // ... copy other relevant maps
        
        // Copy material properties
        targetMaterial.params[0] = raylibMaterial->params[0];  // If used
        
        // Apply shader AFTER texture assignment
        if (shaderId > 0 && shaderSystem_) {
            shaderSystem_->ApplyShaderToModel(shaderId, model, i);
        }
    }
} else if (meshIndex >= 0 && meshIndex < model.materialCount) {
    // Apply to specific material (same pattern)
    Material& targetMaterial = model.materials[meshIndex];
    // ... same texture copying logic ...
    
    // Apply shader AFTER texture assignment
    if (shaderId > 0 && shaderSystem_) {
        shaderSystem_->ApplyShaderToModel(shaderId, model, meshIndex);
    }
}
```

### **🔍 Expected Results**:
- ✅ **Textures should appear** on cubes, spheres, cylinders
- ✅ **Basic lighting** should work (our basic.fs has lighting)
- ✅ **No build errors** or crashes
- ✅ **Materials maintain** texture assignments through shader changes

### **⚠️ Potential Issues**:
- **Texture map copying** might miss some maps → **Mitigation**: Copy all MATERIAL_MAP_* entries
- **Shader uniform locations** might not be set → **Mitigation**: ShaderSystem handles this
- **Material params** might be lost → **Mitigation**: Copy params array

### **🧪 Validation Steps**:
1. **Build succeeds** without warnings
2. **Run game** and load test map
3. **Verify textures** appear on test objects
4. **Check console logs** for shader loading messages
5. **No crashes** on map load/unload

### **🔄 Rollback Plan**:
- **Simple**: Revert `MaterialSystem.cpp` to original code
- **Backup**: Git commit before changes
- **Fast Recovery**: < 5 minutes to restore

---

## 📊 **PHASE 2: Debug and Verify Pipeline**

### **🎯 Goal**: Add comprehensive logging to understand rendering pipeline
### **⏱️ Estimated Time**: 45 minutes  
### **🎲 Risk Level**: LOW (only adding logs, no logic changes)

### **📁 Files to Modify**:
1. `src/ecs/Systems/MaterialSystem.cpp` - **Add debug logging**
2. `src/shaders/ShaderSystem.cpp` - **Add shader loading logs**
3. `src/assets/AssetSystem.cpp` - **Add texture loading logs** (if exists)

### **🔧 Specific Changes**:

#### **MaterialSystem.cpp - Enhanced logging**:
```cpp
void MaterialSystem::ApplyDiffuseTexture(Material& material, const std::string& texturePath) const {
    LOG_INFO("🎨 ATTEMPTING to load texture: " + texturePath);
    
    if (!assetSystem_) {
        LOG_ERROR("❌ AssetSystem not available for texture: " + texturePath);
        return;
    }

    Texture2D* texture = assetSystem_->GetOrLoadTexture(texturePath);
    if (texture && texture->id != 0) {
        LOG_INFO("✅ TEXTURE LOADED SUCCESSFULLY:");
        LOG_INFO("  Path: " + texturePath);
        LOG_INFO("  ID: " + std::to_string(texture->id));
        LOG_INFO("  Size: " + std::to_string(texture->width) + "x" + std::to_string(texture->height));
        LOG_INFO("  Format: " + std::to_string(texture->format));
        
        material.maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
        material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        
        LOG_INFO("✅ TEXTURE ASSIGNED to material slot");
    } else {
        LOG_ERROR("❌ TEXTURE LOADING FAILED:");
        LOG_ERROR("  Path: " + texturePath);
        LOG_ERROR("  Texture pointer: " + std::string(texture ? "valid" : "null"));
        if (texture) {
            LOG_ERROR("  Texture ID: " + std::to_string(texture->id));
        }
        ApplySolidColor(material, WHITE);
    }
}
```

#### **ShaderSystem.cpp - Enhanced shader logging**:
```cpp
uint32_t ShaderSystem::CreateDefaultBasicShader() {
    LOG_INFO("🔧 CREATING default basic shader");
    
    std::string vsPath = GetShaderPath("basic/basic.vs");
    std::string fsPath = GetShaderPath("basic/basic.fs");
    
    LOG_INFO("🔍 SHADER PATHS:");
    LOG_INFO("  Vertex: " + vsPath);
    LOG_INFO("  Fragment: " + fsPath);
    LOG_INFO("  VS exists: " + std::string(fs::exists(vsPath) ? "YES" : "NO"));
    LOG_INFO("  FS exists: " + std::string(fs::exists(fsPath) ? "YES" : "NO"));
    
    // ... rest of creation logic with enhanced logging ...
}
```

### **🔍 Expected Results**:
- ✅ **Clear visibility** into texture loading success/failure
- ✅ **Shader file existence** verification  
- ✅ **Texture dimensions and IDs** in logs
- ✅ **Material assignment** confirmation

### **🧪 Validation Steps**:
1. **Search logs** for "🎨 ATTEMPTING" to verify texture loading attempts
2. **Search logs** for "✅ TEXTURE LOADED" to confirm successful loads  
3. **Search logs** for "❌ TEXTURE LOADING FAILED" to identify failures
4. **Search logs** for "🔧 CREATING" to verify shader creation
5. **Count texture vs material assignments** to ensure 1:1 ratio

---

## 🏗️ **PHASE 3: ShaderSystem CacheSystem Migration**

### **🎯 Goal**: Migrate ShaderSystem to use unified CacheSystem pattern
### **⏱️ Estimated Time**: 2-3 hours
### **🎲 Risk Level**: MEDIUM (major architectural change)

### **📁 Files to Modify**:
1. `src/ecs/Systems/CacheSystem.h` - **Add shader specializations**
2. `src/ecs/Systems/CacheSystem.cpp` - **Add ShaderCacheFactory**  
3. `src/shaders/ShaderSystem.h` - **Major refactor**
4. `src/shaders/ShaderSystem.cpp` - **Major refactor**
5. `src/ecs/Systems/MaterialSystem.cpp` - **Update shader integration**
6. `src/core/Engine.cpp` - **Verify initialization order**

### **🔧 Specific Changes**:

#### **CacheSystem.h - Add shader specializations**:
```cpp
// Shader caching structures
struct ShaderCacheKey {
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
    
    bool operator==(const ShaderCacheKey& other) const;
};

struct CachedShaderData {
    Shader shader;                    // Raylib shader object
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
    bool isDefault;
    
    // Cached uniform locations for performance
    std::unordered_map<std::string, int> uniformLocations;
    
    // Performance metrics
    size_t memoryUsage = 0;
    uint32_t renderCalls = 0;
};

struct ShaderProperties {
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
};

class ShaderCacheFactory {
public:
    static ShaderCacheKey GenerateKey(const ShaderProperties& props);
    static std::unique_ptr<CachedShaderData> CreateShaderData(const ShaderProperties& props);
};

// Type alias for CacheSystem specialization  
using ShaderCache = CacheSystem<ShaderCacheKey, CachedShaderData, ShaderProperties>;

// Hash specialization
namespace std {
    template<>
    struct hash<ShaderCacheKey> {
        size_t operator()(const ShaderCacheKey& key) const;
    };
}
```

#### **ShaderSystem.h - Major refactor**:
```cpp
class ShaderSystem : public System {
public:
    // New CacheSystem-based interface
    uint32_t GetOrCreateShader(const ShaderProperties& properties);
    const CachedShaderData* GetShader(uint32_t shaderId) const;
    CachedShaderData* GetMutableShader(uint32_t shaderId);
    
    // Reference counting (delegated to CacheSystem)
    void AddShaderReference(uint32_t shaderId);
    void RemoveShaderReference(uint32_t shaderId);
    
    // Direct cache access (like MaterialSystem pattern)
    ShaderCache* GetShaderCache() { return shaderCache_.get(); }
    
    // Legacy compatibility methods
    uint32_t GetBasicShaderId() const { return basicShaderId_; }
    uint32_t GetPBRShaderId() const { return pbrShaderId_; }
    
    // Shader application (updated to use CachedShaderData)
    void ApplyShaderToModel(uint32_t shaderId, Model& model, int meshIndex = -1);

private:
    std::unique_ptr<ShaderCache> shaderCache_;
    
    // Default shader IDs (cached)
    uint32_t basicShaderId_;
    uint32_t pbrShaderId_;
    
    // Remove old data structures:
    // ❌ std::unordered_map<uint32_t, std::unique_ptr<ShaderData>> shaders_;
    // ❌ uint32_t nextShaderId_;
    
    // Helper methods (updated)
    uint32_t CreateDefaultBasicShader();
    uint32_t CreateDefaultPBRShader();
};
```

### **🔗 Systems to Rehook/Relink**:
1. **MaterialSystem** → Update `GetBasicShaderId()` calls
2. **Renderer** → Update any direct shader access  
3. **WorldSystem** → Verify shader system integration
4. **Engine initialization** → Ensure ShaderSystem initializes before MaterialSystem

### **🔍 Expected Results**:
- ✅ **Same functionality** as before but with caching benefits
- ✅ **Automatic shader deduplication** (same files = same cache entry)
- ✅ **Reference counting** for shader cleanup
- ✅ **Memory management** with LRU eviction
- ✅ **Performance improvement** from cached uniform locations

### **⚠️ Potential Issues**:
- **ShaderSystem initialization** order with other systems
- **Legacy shader ID references** might break
- **Uniform location caching** might need update
- **Default shader creation** timing

### **🧪 Validation Steps**:
1. **Build succeeds** with no compilation errors
2. **Same textures render** as in Phase 1
3. **Shader cache statistics** show proper hit/miss ratios
4. **Memory usage** doesn't increase significantly  
5. **No performance regression** in frame rate
6. **Default shaders** still load correctly
7. **Map unload** clears shader cache appropriately

### **🔄 Rollback Plan**:
- **Staged commits**: Commit each file change separately
- **Feature flag**: Keep old ShaderSystem methods as fallback
- **Quick restore**: Each file can be reverted individually
- **Test after each file**: Ensure incremental functionality

---

## ⚡ **PHASE 4: Instancing Foundation**

### **🎯 Goal**: Add high-performance instanced rendering foundation
### **⏱️ Estimated Time**: 3-4 hours
### **🎲 Risk Level**: HIGH (new rendering path, complex integration)

### **📁 Files to Create/Modify**:
1. **NEW**: `src/rendering/InstancedRenderer.h`
2. **NEW**: `src/rendering/InstancedRenderer.cpp`
3. **NEW**: `build/bin/shaders/basic/basic_instancing.vs`
4. **NEW**: `build/bin/shaders/basic/lighting_instancing.vs`
5. `src/rendering/Renderer.h` - **Add instancing integration**
6. `src/rendering/Renderer.cpp` - **Add instancing rendering path**
7. `src/shaders/ShaderSystem.cpp` - **Add instancing shader support**
8. `src/ecs/Systems/RenderSystem.cpp` - **Add instancing logic**

### **🔧 Specific Changes**:

#### **InstancedRenderer.h - New component**:
```cpp
class InstancedRenderer {
public:
    // Instance management
    uint64_t AddInstance(uint32_t meshId, uint32_t materialId, const Matrix& transform);
    void RemoveInstance(uint64_t instanceId);
    void UpdateInstance(uint64_t instanceId, const Matrix& transform);
    
    // Batch rendering
    void FlushInstanceBatches();
    void ClearInstances();
    
    // Performance queries
    size_t GetInstanceCount(uint32_t meshId) const;
    size_t GetTotalInstances() const;
    size_t GetBatchCount() const;
    
    // Configuration
    void SetMaxInstancesPerBatch(size_t maxInstances);
    void SetAutoFlushThreshold(size_t threshold);

private:
    struct InstanceBatch {
        uint32_t meshId;
        uint32_t materialId;
        uint32_t shaderId;              // Instancing shader
        std::vector<Matrix> transforms; // GPU upload buffer
        size_t maxInstances;
        bool needsGPUUpdate;
    };
    
    std::unordered_map<uint64_t, InstanceBatch> instanceBatches_;
    uint64_t nextBatchId_;
    size_t maxInstancesPerBatch_;
    size_t autoFlushThreshold_;
};
```

#### **Renderer.h - Instancing integration**:
```cpp
class Renderer {
public:
    // Instancing configuration
    void SetInstancedRenderingEnabled(bool enabled);
    void SetInstanceThreshold(size_t threshold = 10);
    
    // Instance management
    uint64_t AddRenderInstance(uint32_t meshId, uint32_t materialId, const Matrix& transform);
    void RemoveRenderInstance(uint64_t instanceId);
    
private:
    std::unique_ptr<InstancedRenderer> instancedRenderer_;
    bool instancingEnabled_;
    size_t instanceThreshold_;
    
    // Auto-batching detection
    std::unordered_map<std::pair<uint32_t, uint32_t>, size_t> renderCounts_;  // (meshId, materialId) -> count
    
    void DetectInstanceOpportunities();
    void FlushInstanceBatches();
};
```

### **🔗 Systems to Rehook/Relink**:
1. **RenderSystem** → Detect when multiple entities use same mesh+material
2. **MeshComponent** → Add instancing hints/flags
3. **MaterialSystem** → Ensure instancing shader compatibility
4. **ShaderSystem** → Load instancing shader variants
5. **WorldSystem** → Mark static vs dynamic objects for instancing
6. **Engine** → Initialize InstancedRenderer in correct order

### **🔍 Expected Results**:
- ✅ **Automatic batching** when 10+ identical objects detected
- ✅ **Performance improvement** for repeated objects (cubes, spheres)
- ✅ **Single draw call** per batch instead of individual calls
- ✅ **GPU transform arrays** for efficient processing
- ✅ **Fallback rendering** for non-instanced objects

### **⚠️ Potential Issues**:
- **Complex shader management** between regular and instancing shaders
- **Transform matrix uploads** to GPU every frame
- **Memory usage** for instance arrays
- **Compatibility** with existing material system
- **Detection logic** for auto-batching might be expensive

### **🧪 Validation Steps**:
1. **Build succeeds** with new instancing components
2. **Backward compatibility** - single objects still render normally
3. **Performance test** - 100+ cubes should auto-batch
4. **Frame rate improvement** measurable with many objects
5. **Instance statistics** show correct batching
6. **No visual differences** between instanced and regular rendering
7. **Memory usage** reasonable for instance arrays

### **🔄 Rollback Plan**:
- **Feature flag**: `ENABLE_INSTANCING` to disable instancing path
- **Graceful degradation**: Fall back to regular rendering if instancing fails
- **Modular design**: InstancedRenderer can be completely disabled
- **Separate shaders**: Instancing shaders are additive, don't replace existing

---

## 📊 **Integration & Testing Strategy**

### **After Each Phase**:
1. **🧪 Automated Tests** (if available):
   - Build success
   - Basic rendering functionality
   - Memory leak detection
   - Performance benchmarks

2. **🎮 Manual Testing**:
   - Load test map with various objects
   - Verify visual results match expectations
   - Test map load/unload cycles
   - Monitor console logs for errors

3. **📈 Performance Monitoring**:
   - Frame rate measurements
   - Memory usage tracking  
   - Draw call counts
   - Cache hit/miss ratios

### **Final Integration Test**:
```bash
# Test sequence after all phases
1. Build project successfully
2. Load test map with mixed objects (cubes, spheres, cylinders, capsules)
3. Verify textures appear on all objects
4. Monitor frame rate with 100+ objects
5. Test instancing auto-detection
6. Verify cache statistics
7. Test map unload/reload cycles
8. Memory leak detection over time
```

---

## 🎯 **Success Criteria**

### **Phase 1 Success**:
- [ ] Textures visible on test objects
- [ ] No build errors or crashes
- [ ] Basic lighting functional

### **Phase 2 Success**:
- [ ] Comprehensive logging working
- [ ] Clear debugging information
- [ ] Texture loading pipeline visible

### **Phase 3 Success**:
- [ ] ShaderSystem using CacheSystem
- [ ] No performance regression
- [ ] Shader deduplication working
- [ ] Reference counting functional

### **Phase 4 Success**:
- [ ] Instancing automatically activates for repeated objects
- [ ] Measurable performance improvement
- [ ] Backward compatibility maintained
- [ ] Foundation ready for game-specific effects

---

## 🚨 **Risk Mitigation**

### **High-Risk Areas**:
1. **Shader-Material Integration** → Test after each change
2. **Memory Management** → Monitor for leaks
3. **Performance Regression** → Benchmark before/after
4. **System Initialization Order** → Verify dependencies

### **Rollback Triggers**:
- **Build failures** that can't be fixed in 15 minutes
- **Visual regression** (objects disappear/become white)
- **Performance drops** > 20% from baseline
- **Memory leaks** detected during testing

### **Safety Measures**:
- **Git commits** after each major change
- **Feature flags** for new functionality
- **Fallback paths** for critical rendering
- **Modular implementation** - each phase standalone

---

## ✅ **Ready for Implementation**

**This tracker provides**:
- ✅ **Exact files to modify** for each phase
- ✅ **Specific code changes** with examples
- ✅ **Clear success criteria** and validation steps  
- ✅ **Risk assessment** and mitigation strategies
- ✅ **Rollback plans** for each phase
- ✅ **Integration testing** strategy

**Please review and confirm this plan before I proceed with implementation!**
