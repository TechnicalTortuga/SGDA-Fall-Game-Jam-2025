# Paint Strike - Shader System Architecture Roadmap

## 🎨 **Game Vision: Paint Strike Force**
Counter-Strike style tactical shooter with paintball mechanics and territory control through surface painting.

## 🏗️ **Current Architecture Analysis**

### **Existing Systems:**
- ✅ ECS Architecture (Entity-Component-System)
- ✅ MaterialSystem (flyweight pattern for materials)
- ✅ AssetSystem (texture loading and caching)
- ✅ RenderAssetCache (Model caching for primitives)
- ✅ MeshSystem (geometry creation)
- ⚠️ **MISSING**: Comprehensive Shader System

### **Current Issues:**
1. **No Default Shaders**: Models render white without proper shaders
2. **Material-Shader Disconnect**: Materials don't automatically get appropriate shaders
3. **No Paint System**: Core gameplay mechanic missing
4. **No Instancing Support**: Performance bottleneck for painted surfaces

## 🚀 **Phase 1: Foundation Shader System (Tonight)**

### **1.1 Create ShaderSystem (ECS System)**
```cpp
class ShaderSystem : public System {
    // Shader loading, compilation, caching
    // Uniform management
    // Shader hot-reloading for development
};
```

### **1.2 Create ShaderComponent**
```cpp
struct ShaderComponent : public Component {
    uint32_t shaderId;           // Reference to ShaderSystem
    std::map<string, float> uniforms;  // Per-entity shader parameters
    ShaderType type;             // BASIC, PBR, PAINT, EMISSIVE, etc.
};
```

### **1.3 Integrate with MaterialSystem**
- MaterialSystem automatically assigns appropriate shaders
- Default shader for basic texture rendering
- PBR shader for advanced materials

### **1.4 Fix Current Rendering**
- Ensure all cached models get default shaders
- Fix material-shader binding in RenderAssetCache

## 🎨 **Phase 2: Paint System Foundation (Week 1)**

### **2.1 Paint Surface Tracking**
```cpp
struct PaintComponent : public Component {
    std::vector<PaintSplat> paintSplats;  // Paint impact points
    uint32_t teamOwnership;               // Which team controls this surface
    float paintCoverage;                  // 0.0 - 1.0 coverage percentage
};
```

### **2.2 Dynamic Paint Shaders**
- Surface shader that blends base texture with paint
- Real-time paint splattering effects
- Paint aging/fading over time
- Team color blending

### **2.3 Territory Control System**
```cpp
class TerritorySystem : public System {
    // Calculate team control percentages
    // Handle territory capture events
    // Paint coverage validation
};
```

## ⚡ **Phase 3: Performance & Instancing (Week 2)**

### **3.1 Instanced Rendering**
- Group similar painted surfaces for instanced rendering
- Batch draw calls by shader type
- GPU-based paint calculations

### **3.2 LOD System Integration**
- Multiple paint detail levels based on distance
- Simplified paint shaders for distant objects
- Dynamic quality adjustment

### **3.3 Occlusion Culling**
- Only render painted surfaces in view
- Paint system integration with BSP visibility

## 🎮 **Phase 4: Gameplay Integration (Week 3)**

### **4.1 Paintball Weapon System**
```cpp
struct PaintballWeapon : public Component {
    PaintType paintType;      // Different paint effects
    float paintRadius;        // Splat size
    Color teamColor;         // Team identification
};
```

### **4.2 Surface Impact System**
- Ray casting for paint impacts
- Surface normal-based paint orientation
- Paint physics (dripping, spreading)

### **4.3 Game Mode Integration**
- Territory control win conditions
- Paint coverage scoring
- Real-time team territory display

## 🔧 **Technical Architecture**

### **Shader Types Hierarchy:**
```
BaseShader
├── BasicShader (diffuse texture + lighting)
├── PBRShader (physically-based rendering)
├── PaintShader (base + dynamic paint overlay)
│   ├── WallPaintShader
│   ├── FloorPaintShader
│   └── PropPaintShader
├── EmissiveShader (glowing effects)
└── UIShader (HUD elements)
```

### **Component Integration:**
```
Entity
├── TransformComponent
├── MeshComponent
├── MaterialComponent
├── ShaderComponent     ← New
├── PaintComponent      ← New
└── TerritoryComponent  ← New
```

### **System Dependencies:**
```
RenderSystem
├── ShaderSystem ← New, manages all shaders
├── MaterialSystem (enhanced with shader support)
├── TerritorySystem ← New, paint tracking
└── RenderAssetCache (enhanced with shader caching)
```

## 🎯 **Tonight's Implementation Plan**

### **Immediate Fixes (1-2 hours):**
1. Create basic ShaderSystem
2. Add default shader to all models
3. Fix white texture issue
4. Restore composite capsule rendering

### **Foundation Setup (2-3 hours):**
1. Implement ShaderComponent
2. Create basic.vs/basic.fs shaders
3. Integrate ShaderSystem with MaterialSystem
4. Test textured rendering

### **Advanced (3-4 hours):**
1. Create paint.vs/paint.fs shader templates
2. Basic PaintComponent structure
3. Simple paint impact system
4. Territory ownership tracking

## 📂 **File Structure**
```
src/
├── shaders/
│   ├── ShaderSystem.h/cpp
│   ├── basic/
│   │   ├── basic.vs
│   │   └── basic.fs
│   ├── paint/
│   │   ├── paint.vs
│   │   └── paint.fs
│   └── pbr/
│       ├── pbr.vs
│       └── pbr.fs
├── ecs/
│   ├── Components/
│   │   ├── ShaderComponent.h
│   │   └── PaintComponent.h
│   └── Systems/
│       ├── ShaderSystem.h/cpp
│       └── TerritorySystem.h/cpp
└── gameplay/
    ├── PaintballWeapon.h/cpp
    └── TerritoryControl.h/cpp
```

## 🧪 **Testing Strategy**
1. **Immediate**: Fix white objects, confirm texture rendering
2. **Short-term**: Basic paint shader with simple color overlay
3. **Medium-term**: Dynamic paint splattering
4. **Long-term**: Full territory control gameplay

## 🎨 **Visual Goals**
- Dynamic paint splattering with team colors
- Real-time territory control visualization
- High-performance instanced rendering
- Professional shader effects (dripping, fading, blending)
- Strategic gameplay through visual territory control

---

**Success Metrics:**
- [ ] All objects render with proper textures (tonight)
- [ ] Basic paint system functional (week 1)
- [ ] Territory control gameplay (week 3)
- [ ] 60fps with 100+ painted surfaces (optimization target)
