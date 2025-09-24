# 🚀 Shader Caching & Instancing Integration Plan

**Date**: September 23, 2025  
**Focus**: Unified Caching Architecture + High-Performance Rendering

---

## 🔍 **Current ShaderSystem Analysis**

### ✅ **What's Already Good**
```cpp
// Current shader caching (basic but functional)
std::unordered_map<uint32_t, std::unique_ptr<ShaderData>> shaders_;

struct ShaderData {
    Shader shader;                  // Raylib shader
    std::string vertexPath;         // Source paths for hot-reload
    std::string fragmentPath;
    ShaderType type;                // BASIC, PBR, PAINT, etc.
    bool isDefault;                 // Default vs custom shaders
    std::unordered_map<std::string, int> uniformLocations; // ✅ Cached uniform locations!
};
```

### ❌ **What Needs CacheSystem Integration**
1. **No Deduplication** - Same shader files loaded multiple times = multiple IDs
2. **No Reference Counting** - Shaders never cleaned up automatically
3. **No Memory Management** - Could accumulate unused shaders
4. **No LRU Eviction** - No strategy for shader memory limits

---

## 🎮 **Instancing Examples Insights**

### **Key Pattern: Single Shader, Many Objects**
```cpp
// FROM RAYLIB INSTANCING EXAMPLE:
Material matInstances = LoadMaterialDefault();
matInstances.shader = shader;  // ONE shader
matInstances.maps[MATERIAL_MAP_DIFFUSE].color = RED;

// Render 10,000 cubes with ONE draw call!
DrawMeshInstanced(cube, matInstances, transforms, MAX_INSTANCES);
```

### **Critical Performance Benefits**:
- ✅ **10,000 objects** with **1 draw call** vs 10,000 draw calls
- ✅ **GPU transform arrays** - transforms uploaded once per frame
- ✅ **Shared material state** - one shader/texture for all instances
- ✅ **Per-frame uniform updates** - camera position, lighting

### **Specialized Instancing Shaders**:
```cpp
// Uses special vertex shader: lighting_instancing.vs
Shader shader = LoadShader("lighting_instancing.vs", "lighting.fs");
```

**Instancing vertex shader handles transform arrays in GPU memory!**

---

## 🏗️ **ShaderCache Integration Plan**

### **Phase 1: Migrate ShaderSystem to CacheSystem**

#### **New Shader Caching Structures**:
```cpp
// ShaderCacheKey - for deduplication
struct ShaderCacheKey {
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
    
    bool operator==(const ShaderCacheKey& other) const {
        return vertexPath == other.vertexPath && 
               fragmentPath == other.fragmentPath && 
               type == other.type;
    }
};

// CachedShaderData - intrinsic shader state
struct CachedShaderData {
    Shader shader;                    // Raylib shader object
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
    bool isDefault;
    
    // Cached uniform locations for performance
    std::unordered_map<std::string, int> uniformLocations;
    
    // Instancing support
    bool supportsInstancing = false;
    int maxInstances = 1;
    
    // Performance metrics
    size_t memoryUsage = 0;
    uint32_t renderCalls = 0;
};

// ShaderProperties - input for shader creation
struct ShaderProperties {
    std::string vertexPath;
    std::string fragmentPath;
    ShaderType type;
    bool enableInstancing = false;
    int maxInstances = 1000;
};

class ShaderCacheFactory {
public:
    static ShaderCacheKey GenerateKey(const ShaderProperties& props);
    static std::unique_ptr<CachedShaderData> CreateShaderData(const ShaderProperties& props);
};

// Type alias for CacheSystem specialization
using ShaderCache = CacheSystem<ShaderCacheKey, CachedShaderData, ShaderProperties>;
```

#### **Updated ShaderSystem Interface**:
```cpp
class ShaderSystem : public System {
public:
    // Shader management with caching
    uint32_t GetOrCreateShader(const ShaderProperties& properties);
    const CachedShaderData* GetShader(uint32_t shaderId) const;
    CachedShaderData* GetMutableShader(uint32_t shaderId);
    
    // Reference counting (delegated to CacheSystem)
    void AddShaderReference(uint32_t shaderId);
    void RemoveShaderReference(uint32_t shaderId);
    
    // Direct cache access (like MaterialSystem)
    ShaderCache* GetShaderCache() { return shaderCache_.get(); }
    
    // Instancing support
    uint32_t GetOrCreateInstancingShader(ShaderType baseType, int maxInstances);
    bool SupportsInstancing(uint32_t shaderId) const;
    
private:
    std::unique_ptr<ShaderCache> shaderCache_;
    
    // Default shader IDs (cached)
    uint32_t basicShaderId_;
    uint32_t basicInstancingShaderId_;
    uint32_t pbrShaderId_;
    uint32_t pbrInstancingShaderId_;
};
```

---

## ⚡ **Instancing System Architecture**

### **New InstancedRenderer Component**:
```cpp
class InstancedRenderer {
public:
    // Instance management
    void AddInstance(uint32_t meshId, uint32_t materialId, const Matrix& transform);
    void RemoveInstance(uint64_t instanceId);
    void UpdateInstance(uint64_t instanceId, const Matrix& transform);
    
    // Batch rendering
    void FlushInstanceBatches();
    void ClearInstances();
    
    // Performance queries
    size_t GetInstanceCount(uint32_t meshId) const;
    size_t GetTotalInstances() const;
    size_t GetBatchCount() const;

private:
    struct InstanceBatch {
        uint32_t meshId;
        uint32_t materialId;
        uint32_t shaderId;              // Instancing shader
        std::vector<Matrix> transforms; // GPU upload buffer
        size_t maxInstances;
    };
    
    std::unordered_map<uint64_t, InstanceBatch> instanceBatches_;
    uint64_t nextBatchId_;
};
```

### **Integration with Existing Systems**:
```cpp
// Renderer integration
class Renderer {
public:
    void SetInstancedRenderingEnabled(bool enabled);
    void SetInstanceThreshold(size_t threshold = 10); // Auto-batch when 10+ same objects
    
private:
    std::unique_ptr<InstancedRenderer> instancedRenderer_;
    bool instancingEnabled_ = true;
    size_t instanceThreshold_ = 10;
};

// MaterialSystem integration  
class MaterialSystem {
public:
    // Create instancing-compatible materials
    uint32_t CreateInstancingMaterial(const MaterialProperties& baseProps, int maxInstances);
    bool SupportsInstancing(uint32_t materialId) const;
};
```

---

## 🎯 **Immediate Action Plan**

### **Step 1: Fix Current Shader Assignment (CRITICAL)**
```cpp
// MaterialSystem::ApplyMaterialToModel() - FIXED ORDER
void MaterialSystem::ApplyMaterialToModel(uint32_t materialId, Model& model, int meshIndex) {
    Material* raylibMaterial = GetCachedRaylibMaterial(materialId);
    if (!raylibMaterial) return;

    // Store original shader before material assignment
    std::vector<Shader> originalShaders;
    if (meshIndex == -1) {
        for (int i = 0; i < model.materialCount; i++) {
            originalShaders.push_back(model.materials[i].shader);
        }
    } else {
        originalShaders.push_back(model.materials[meshIndex].shader);
    }

    // Apply material (this might overwrite shader)
    if (meshIndex == -1) {
        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i] = *raylibMaterial;
        }
    } else {
        model.materials[meshIndex] = *raylibMaterial;
    }

    // NOW apply proper shader AFTER material assignment
    if (shaderSystem_) {
        uint32_t shaderId = shaderSystem_->GetBasicShaderId();
        shaderSystem_->ApplyShaderToModel(shaderId, model, meshIndex);
    }
}
```

### **Step 2: Migrate ShaderSystem to CacheSystem**
1. Create `ShaderCacheKey`, `CachedShaderData`, `ShaderProperties` structs
2. Implement `ShaderCacheFactory` class
3. Replace `std::unordered_map<uint32_t, ShaderData>` with `std::unique_ptr<ShaderCache>`
4. Update all methods to delegate to cache system

### **Step 3: Add Instancing Foundation**
1. Create basic `InstancedRenderer` class
2. Add instancing shader variants (basic_instancing.vs, pbr_instancing.vs)
3. Implement automatic batching threshold in Renderer
4. Test with simple cubes/spheres

### **Step 4: Light System Integration**
```cpp
// Future LightSystem with CacheSystem pattern
struct LightCacheKey {
    LightType type;           // DIRECTIONAL, POINT, SPOT
    Vector3 position;
    Vector3 direction;
    Color color;
    float intensity;
    float range;
    float angle;              // For spot lights
};

struct CachedLightData {
    LightType type;
    Vector3 position;
    Vector3 direction;
    Color color;
    float intensity;
    float range;
    float angle;
    
    // Shader uniform locations (cached per shader)
    std::unordered_map<uint32_t, int> positionLocs;
    std::unordered_map<uint32_t, int> colorLocs;
    std::unordered_map<uint32_t, int> intensityLocs;
    
    // Shadow mapping support
    bool castsShadows = false;
    uint32_t shadowMapId = 0;
};

using LightCache = CacheSystem<LightCacheKey, CachedLightData, LightProperties>;
```

---

## 📊 **Expected Performance Improvements**

### **Before (Current)**:
- ❌ **1,000 cubes = 1,000 draw calls**
- ❌ **Shader recompilation/binding per object**
- ❌ **CPU transform calculations every frame**
- ❌ **No shader deduplication**

### **After (With Instancing + Caching)**:
- ✅ **1,000 cubes = 1 draw call** (990 draw calls saved!)
- ✅ **Shader binding once per batch**
- ✅ **GPU transform arrays uploaded once**
- ✅ **Automatic shader deduplication and cleanup**
- ✅ **Reference counting prevents memory leaks**

### **Paint Strike Game Benefits**:
- **Paint splatter particles**: Thousands of instances efficiently
- **Territory markers**: Batched rendering of control points
- **Weapon effects**: Instanced muzzle flashes, bullet tracers
- **Environment props**: Trees, rocks, buildings batched by type

---

## 🎮 **Game-Specific Shader Types**

### **Immediate Shaders Needed**:
```cpp
enum class ShaderType {
    BASIC,              // ✅ Current - simple diffuse + lighting
    BASIC_INSTANCING,   // 🆕 Instanced version of basic
    PBR,                // ✅ Current - physically-based rendering  
    PBR_INSTANCING,     // 🆕 Instanced PBR
    PAINT_SPLATTER,     // 🆕 Paint Strike - dynamic paint overlays
    TERRITORY_MARKER,   // 🆕 Paint Strike - team control visualization
    EMISSIVE,           // ✅ Current - glowing effects
    SKYBOX,             // ✅ Current - environment rendering
    UI,                 // 🆕 HUD elements
    POST_PROCESS        // 🆕 Screen-space effects
};
```

### **Paint Strike Specific Effects**:
1. **Paint Splatter Shader**: Dynamic texture blending for paint impacts
2. **Territory Control**: Color-coded area highlighting based on team ownership
3. **Paint Trail**: Projectile paint trails with fade-out effects
4. **Team Identification**: Player/object highlighting with team colors

---

## 🛠️ **Implementation Priority**

### **Priority 1 (This Session)**:
1. ✅ **Fix shader assignment order** in MaterialSystem
2. ✅ **Add debug logging** for texture/shader verification
3. ✅ **Test basic textured model** rendering

### **Priority 2 (Next Session)**:
1. **Migrate ShaderSystem** to CacheSystem pattern
2. **Add basic instancing** support (cubes/spheres)
3. **Performance testing** with many objects

### **Priority 3 (Future)**:
1. **Light System** with CacheSystem integration
2. **Paint Strike shaders** for game mechanics
3. **Post-processing pipeline** for visual effects

---

## 🎯 **Success Metrics**

1. **Functional**: Textured models render correctly
2. **Performance**: 1,000+ objects at 60 FPS with instancing
3. **Architecture**: All rendering systems use unified CacheSystem
4. **Game-Ready**: Foundation for paint strike mechanics

**The instancing examples show us the path to high-performance rendering - let's implement it systematically!**
