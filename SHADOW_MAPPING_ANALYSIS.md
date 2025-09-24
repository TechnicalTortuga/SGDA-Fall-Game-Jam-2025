# Shadow Mapping Implementation Analysis

## Current State Analysis

### ✅ Working Components
- Shadow map FBO creation (ID: 1) ✓
- Shadow rendering pipeline ✓
- Entity rendering to shadow map ✓
- Uniform locations found (lightVP: 3, shadowMap: 8, shadowsEnabled: 14, shadowMapResolution: 15) ✓
- Basic lighting system ✓

### ❌ Issues Identified

#### 1. **Incorrect Shadow Application Logic**
**Problem**: Our shader applies shadows per-light instead of globally.

**Current (Wrong) Approach**:
```glsl
// Apply shadows only to first directional light
float lightShadowFactor = (i == 0 && lights[i].type == LIGHT_DIRECTIONAL) ? shadowFactor : 1.0;
// Apply to individual light contribution
lightDot += lights[i].color.rgb * NdotL * attenuation * lightShadowFactor * 1.5;
```

**Correct Approach (from Raylib example)**:
```glsl
// Calculate all lighting first
finalColor = (texelColor*((colDiffuse + vec4(specular, 1.0))*vec4(lightDot, 1.0)));
// Then apply shadows globally
finalColor = mix(finalColor, vec4(0, 0, 0, 1), shadowFactor);
```

#### 2. **Over-Complex Multi-Light Shadow Logic**
**Problem**: Our shader tries to handle shadows for multiple lights, but shadow mapping typically works best with directional lights.

**Raylib Approach**: Simple single directional light with shadows.

#### 3. **Shadow Factor Calculation**
**Problem**: Our shadow factor goes from 1.0 (no shadow) to 0.0 (full shadow), but we invert it incorrectly.

**Current**: `shadowFactor = 1.0 - (float(shadowCounter) / float(numSamples));`
**Raylib**: Uses `float(shadowCounter)/float(numSamples)` directly in mix().

## Raylib Shadow Mapping Reference

### Shader Structure (shadowmap.fs)
```glsl
// 1. Calculate lighting normally
vec3 lightDot = lightColor.rgb * NdotL;
finalColor = (texelColor * ((colDiffuse + vec4(specular, 1.0)) * vec4(lightDot, 1.0)));

// 2. Calculate shadow factor (0.0 = no shadow, 1.0 = full shadow)
float shadowFactor = float(shadowCounter) / float(numSamples);

// 3. Apply shadows globally
finalColor = mix(finalColor, vec4(0, 0, 0, 1), shadowFactor);

// 4. Add ambient
finalColor += texelColor * (ambient/10.0) * colDiffuse;
```

### Key Implementation Details

#### Shadow Map Creation
```c
target.depth.id = rlLoadTextureDepth(width, height, false);
rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
```

#### Rendering Pipeline
1. **Pass 1**: Render depth from light's perspective to shadow map
2. **Pass 2**: Render scene normally, sample shadow map to determine shadowing

#### Shadow Calculation
```glsl
// Transform to light space
vec4 fragPosLightSpace = lightVP * vec4(fragPosition, 1);
fragPosLightSpace.xyz /= fragPosLightSpace.w;
fragPosLightSpace.xyz = (fragPosLightSpace.xyz + 1.0)/2.0;

// PCF sampling
vec2 texelSize = vec2(1.0/float(shadowMapResolution));
for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
        float sampleDepth = texture(shadowMap, sampleCoords + texelSize*vec2(x, y)).r;
        if (curDepth - bias > sampleDepth) shadowCounter++;
    }
}
```

## Required Fixes

### 1. **Simplify Shadow Shader Logic**
- Remove per-light shadow application
- Apply shadows globally after all lighting calculations
- Focus on directional light shadows only

### 2. **Fix Shadow Factor Calculation**
- Use 0.0-1.0 range correctly (0 = no shadow, 1 = full shadow)
- Apply via `mix(finalColor, black, shadowFactor)`

### 3. **Ensure Proper Texture Binding**
- Shadow map bound to texture slot 10
- Correct sampler2D uniform setup

### 4. **Debug Shadow Map Rendering**
- Verify entities are rendered to shadow map correctly
- Check shadow map coordinates are valid

## Implementation Plan

1. **Update lighting_shadows.fs** to match Raylib approach
2. **Test shadow rendering** with debug visualization
3. **Tune shadow parameters** (bias, PCF kernel size)
4. **Add shadow map debugging** (render shadow map to screen)

## Testing Strategy

1. **Visual Confirmation**: Shadows should appear as dark areas where geometry occludes light
2. **Edge Cases**: Test with different light directions, camera positions
3. **Performance**: Ensure no significant frame rate impact
4. **Quality**: Verify soft shadow edges (PCF working)

## Success Criteria

- ✅ Shadows appear on geometry behind occluding objects
- ✅ Shadow edges are soft (PCF filtering)
- ✅ No shadow acne artifacts
- ✅ Performance acceptable (< 5ms shadow rendering)
- ✅ Works with existing lighting system</content>
</xai:function_call">SHADOW_MAPPING_ANALYSIS.md
