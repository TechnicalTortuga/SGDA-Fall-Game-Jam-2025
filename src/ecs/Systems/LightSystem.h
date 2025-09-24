#pragma once

#include "../System.h"
#include "../Components/LightComponent.h"
#include "../Components/TransformComponent.h"
#include "CacheSystem.h"
#include "raylib.h"
#include <vector>
#include <memory>

// Forward declarations
class Entity;
class ShaderSystem;
class RenderSystem;

// Light cache type alias (defined in CacheSystem.h)
using LightCache = CacheSystem<LightCacheKey, CachedLightData, LightComponent>;

/**
 * @brief LightSystem - Manages dynamic lighting and shader communication
 * 
 * Handles light calculations, shader uniform updates, and integrates with
 * the CacheSystem for performance. Supports point, spot, and directional lights
 * with proper shadow mapping and PBR lighting.
 */
class LightSystem : public System {
public:
    static constexpr int MAX_LIGHTS = 16;  // Maximum lights supported by shader
    
    LightSystem();
    ~LightSystem() override = default;

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Render() override;
    void Shutdown() override;
    const char* GetName() const { return "LightSystem"; }

    // Light management
    void RegisterLight(Entity* entity);
    void UnregisterLight(Entity* entity);
    
    // Shader integration
    void UpdateShaderLights(Shader& shader);
    void SetShaderAmbientLight(Shader& shader, Color ambientColor, float ambientIntensity);
    void UpdateViewPosUniform(Shader& shader);

    // Performance optimization methods
    void MarkLightsDirty() { lightsDirty_ = true; lastLightUpdateTime_ = GetTime(); }
    bool ShouldUpdateLights() const { return lightsDirty_ || (GetTime() - lastLightUpdateTime_) > 1.0; } // Update at least once per second
    
    // Global lighting settings
    void SetAmbientLight(Color color, float intensity);
    void SetGlobalLightIntensity(float multiplier) { globalLightIntensity_ = multiplier; }
    
    // Shadow mapping
    void EnableShadowMapping(bool enable) { shadowMappingEnabled_ = enable; }
    bool IsShadowMappingEnabled() const { return shadowMappingEnabled_; }
    RenderTexture2D GetShadowMap() const { return shadowMap_; }
    Camera3D GetLightCamera() const { return lightCamera_; }
    Matrix GetLightViewProjectionMatrix() const { return lightViewProj_; }
    void RenderShadowMap(Shader& depthShader, const std::vector<Entity*>& entities);
    void SetupLightCamera(Entity* directionalLightEntity, const Vector3& mainCameraPos);
    void SetLightViewProjectionMatrix(const Matrix& matrix) { lightViewProj_ = matrix; }
    void EnableGlobalLighting(bool enabled) { globalLightingEnabled_ = enabled; }
    
    // Light queries
    std::vector<Entity*> GetActiveLights() const;
    int GetActiveLightCount() const { return static_cast<int>(activeLights_.size()); }
    
    // Statistics and debugging
    size_t GetCacheSize() const { return lightCache_->Size(); }
    void LogLightStats() const;

private:
    // Light processing
    void UpdateLight(Entity* entity, float deltaTime);
    RaylibLight CreateRaylibLight(const LightComponent* lightComp, const TransformComponent* transform) const;
    void UpdateShaderLight(Shader& shader, const RaylibLight& light, int lightIndex);
    
    // Cache management
    std::unique_ptr<LightCache> lightCache_;
    
    // Active lights tracking
    std::vector<Entity*> activeLights_;
    std::vector<RaylibLight> shaderLights_;  // Lights sent to shader
    
    // Global lighting state
    Color ambientColor_ = {26, 32, 135, 255};  // Default ambient color
    float ambientIntensity_ = 0.02f;
    float globalLightIntensity_ = 1.0f;
    bool globalLightingEnabled_ = true;
    bool ambientLightDirty_ = true;
    
    // System references
    ShaderSystem* shaderSystem_ = nullptr;
    
    // Shader uniform locations (cached for performance)
    struct ShaderLocations {
        int lightCountLoc = -1;
        int ambientColorLoc = -1;
        int ambientIntensityLoc = -1;
        int viewPosLoc = -1;
        // Shadow mapping uniforms
        int lightVPLoc = -1;
        int shadowMapLoc = -1;
        int shadowMapResolutionLoc = -1;
        int shadowsEnabledLoc = -1;
    } shaderLocs_;
    
    // Shadow mapping
    static constexpr int SHADOW_MAP_RESOLUTION = 1024;
    bool shadowMappingEnabled_ = false;
    RenderTexture2D shadowMap_ = {0};
    Camera3D lightCamera_ = {0};
    Matrix lightViewProj_ = {0};
    int shadowMapTextureSlot_ = 10; // Texture slot for shadow map
    
    // Performance optimization - cache shader updates
    bool lightsDirty_ = true; // Flag to track if lights have changed
    double lastLightUpdateTime_ = 0.0; // Timestamp of last light change

    // Performance tracking
    mutable int lightUpdateCount_ = 0;
    mutable int shaderUpdateCount_ = 0;
};
