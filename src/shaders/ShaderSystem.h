#pragma once

#include "../ecs/System.h"
#include "raylib.h"
#include <unordered_map>
#include <string>
#include <memory>

/*
ShaderSystem - Manages shader loading, compilation, and caching for Paint Strike

This system provides:
- Default shaders for basic texture rendering
- Shader hot-reloading for development
- Uniform management
- Shader type classification
- Integration with MaterialSystem

Key for Paint Strike gameplay:
- Basic texture rendering (immediate need)
- Paint overlay shaders (future)
- Territory control visualization (future)
*/

enum class ShaderType {
    BASIC,          // Simple diffuse texture + basic lighting
    LIGHTING,       // Dynamic lighting with multiple lights
    DEPTH,          // Depth-only rendering for shadow maps
    PBR,            // Physically-based rendering
    PAINT,          // Paint overlay system
    EMISSIVE,       // Glowing effects
    UI,             // HUD elements
    SKYBOX          // Skybox rendering
};

class ShaderSystem : public System {
public:
    ShaderSystem();
    ~ShaderSystem();

    // System lifecycle
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Shader management
    uint32_t LoadShader(const std::string& vsPath, const std::string& fsPath, ShaderType type);
    uint32_t GetOrCreateDefaultShader(ShaderType type);
    uint32_t GetDepthShader();
    Shader* GetShader(uint32_t shaderId);
    
    // Default shaders (automatically created)
    uint32_t GetBasicShaderId() const { return basicShaderId_; }
    uint32_t GetLightingShaderId() const { return lightingShaderId_; }
    uint32_t GetPBRShaderId() const { return pbrShaderId_; }
    
    // Shader application
    void ApplyShaderToModel(uint32_t shaderId, Model& model, int meshIndex = -1);
    void SetShaderUniforms(uint32_t shaderId, const std::unordered_map<std::string, float>& uniforms);
    
    // Hot-reloading for development
    void ReloadShader(uint32_t shaderId);
    void ReloadAllShaders();
    
    // Debug and statistics
    size_t GetLoadedShaderCount() const { return shaders_.size(); }
    void LogShaderStatus() const;

private:
    struct ShaderData {
        Shader shader;
        std::string vertexPath;
        std::string fragmentPath;
        ShaderType type;
        bool isDefault;
        std::unordered_map<std::string, int> uniformLocations; // Cached uniform locations
    };

    std::unordered_map<uint32_t, std::unique_ptr<ShaderData>> shaders_;
    uint32_t nextShaderId_;
    
    // Default shader IDs
    uint32_t basicShaderId_;
    uint32_t lightingShaderId_;
    uint32_t pbrShaderId_;
    uint32_t depthShaderId_;

    // Cached system references for performance
    class LightSystem* lightSystem_;

    // Shader source paths
    std::string shaderDirectory_;

    // Helper methods
    uint32_t CreateDefaultBasicShader();
    uint32_t CreateDefaultLightingShader();
    uint32_t CreateDefaultPBRShader();
    std::string GetShaderPath(const std::string& filename) const;
    bool LoadShaderFromFiles(const std::string& vsPath, const std::string& fsPath, Shader& outShader);
    void SetupDefaultUniforms(ShaderData& shaderData);
    int GetUniformLocation(uint32_t shaderId, const std::string& uniformName);
};
