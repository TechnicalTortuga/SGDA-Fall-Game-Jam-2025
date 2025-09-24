#pragma once

#include "../Component.h"
#include "../../shaders/ShaderSystem.h"
#include <unordered_map>
#include <string>

/*
ShaderComponent - Lightweight shader reference for entities

This component links entities to specific shaders and stores
per-entity shader parameters for dynamic effects.

Essential for Paint Strike's dynamic painting system:
- Basic texture rendering (immediate)
- Paint overlay effects (phase 2)
- Team territory visualization (phase 3)
*/

struct ShaderComponent : public Component {
    // Component identification
    const char* GetTypeName() const override { return "ShaderComponent"; }
    
    // Shader reference
    uint32_t shaderId = 0;              // Reference to ShaderSystem
    ShaderType shaderType = ShaderType::BASIC;  // Shader type for easy identification
    
    // Per-entity uniform parameters
    std::unordered_map<std::string, float> uniforms;
    
    // Paint Strike specific parameters (future use)
    struct PaintParameters {
        float paintCoverage = 0.0f;     // 0.0 = no paint, 1.0 = fully painted
        float teamColorBlend = 0.0f;    // Blend factor for team colors
        float paintFreshness = 1.0f;    // Paint aging factor (1.0 = fresh, 0.0 = old)
        bool needsUpdate = false;       // Flag for shader parameter updates
    } paintParams;
    
    // Component state
    bool isActive = true;
    bool needsShaderUpdate = false;     // Flag when uniforms need uploading
    
    // Constructors
    ShaderComponent() = default;
    ShaderComponent(uint32_t shaderID, ShaderType type = ShaderType::BASIC) 
        : shaderId(shaderID), shaderType(type) {}
    
    // Uniform management helpers
    void SetUniform(const std::string& name, float value) {
        uniforms[name] = value;
        needsShaderUpdate = true;
    }
    
    float GetUniform(const std::string& name, float defaultValue = 0.0f) const {
        auto it = uniforms.find(name);
        return (it != uniforms.end()) ? it->second : defaultValue;
    }
    
    void ClearUniforms() {
        uniforms.clear();
        needsShaderUpdate = true;
    }
    
    // Paint Strike specific helpers
    void SetPaintCoverage(float coverage) {
        paintParams.paintCoverage = coverage;
        paintParams.needsUpdate = true;
        needsShaderUpdate = true;
    }
    
    void SetTeamColor(float blendFactor) {
        paintParams.teamColorBlend = blendFactor;
        paintParams.needsUpdate = true;
        needsShaderUpdate = true;
    }
    
    bool HasValidShader() const {
        return shaderId > 0;
    }
};
