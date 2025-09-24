#include "LightSystem.h"
#include "../Components/GameObject.h"
#include "../Entity.h"
#include "GameObjectSystem.h"
#include "RenderSystem.h"
#include "../../shaders/ShaderSystem.h"
#include "../../core/Engine.h"
#include "../../utils/Logger.h"
#include "raymath.h"

// Light cache implementations moved to CacheSystem.cpp

// LightSystem implementation
LightSystem::LightSystem() {
    // Initialize light cache with factory functions
    lightCache_ = std::make_unique<LightCache>(
        LightCacheFactory::GenerateKey,
        LightCacheFactory::CreateLightData,
        "LightCache"
    );
    
    shaderLights_.reserve(MAX_LIGHTS);
    activeLights_.reserve(MAX_LIGHTS);
    
    LOG_INFO("LightSystem created with MAX_LIGHTS=" + std::to_string(MAX_LIGHTS));
}

void LightSystem::Initialize() {
    LOG_INFO("LightSystem initializing...");

    // Get shader system reference
    shaderSystem_ = Engine::GetInstance().GetSystem<ShaderSystem>();
    if (!shaderSystem_) {
        LOG_WARNING("ShaderSystem not found - lighting may not work properly");
    }

    // Register with GameObjectSystem to receive light entities
    auto gameObjectSystem = Engine::GetInstance().GetSystem<GameObjectSystem>();
    if (gameObjectSystem) {
        // Get existing lights from GameObjectSystem
        auto existingLights = gameObjectSystem->GetActiveLights();
        for (Entity* lightEntity : existingLights) {
            RegisterLight(lightEntity);
        }
        LOG_INFO("Registered " + std::to_string(existingLights.size()) + " existing lights");
    }

    // Create shadow map texture (depth-only, like Raylib shadow example)
    shadowMap_.id = rlLoadFramebuffer(); // Load an empty framebuffer
    shadowMap_.texture.width = SHADOW_MAP_RESOLUTION;
    shadowMap_.texture.height = SHADOW_MAP_RESOLUTION;

    if (shadowMap_.id > 0) {
        rlEnableFramebuffer(shadowMap_.id);

        // Create depth texture - no color attachment needed for shadowmap
        shadowMap_.depth.id = rlLoadTextureDepth(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, false);
        shadowMap_.depth.width = SHADOW_MAP_RESOLUTION;
        shadowMap_.depth.height = SHADOW_MAP_RESOLUTION;
        shadowMap_.depth.format = 19; // DEPTH_COMPONENT_24BIT
        shadowMap_.depth.mipmaps = 1;

        // Attach depth texture to FBO
        rlFramebufferAttach(shadowMap_.id, shadowMap_.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(shadowMap_.id)) {
            LOG_INFO("✅ Shadow map FBO created successfully (ID: " + std::to_string(shadowMap_.id) + ")");
        } else {
            LOG_ERROR("❌ Shadow map FBO is not complete!");
        }

        rlDisableFramebuffer();
    } else {
        LOG_ERROR("❌ Failed to create shadow map framebuffer!");
    }

    // Enable shadow mapping
    EnableShadowMapping(true);

    LOG_INFO("LightSystem initialized with " + std::to_string(activeLights_.size()) + " active lights");
}

void LightSystem::Update(float deltaTime) {
    if (!globalLightingEnabled_) {
        return;
    }
    
    // Check for new lights from GameObjectSystem periodically
    static int checkCounter = 0;
    checkCounter++;
    if (checkCounter % 60 == 0) {  // Check every second
        auto gameObjectSystem = Engine::GetInstance().GetSystem<GameObjectSystem>();
        if (gameObjectSystem) {
            auto allLights = gameObjectSystem->GetActiveLights();
            for (Entity* lightEntity : allLights) {
                // Check if this light is not already registered
                auto it = std::find(activeLights_.begin(), activeLights_.end(), lightEntity);
                if (it == activeLights_.end()) {
                    RegisterLight(lightEntity);
                    LOG_INFO("🔆 Discovered new light entity " + std::to_string(lightEntity->GetId()) + " from GameObjectSystem");
                }
            }
        }
    }
    
    // Update all active lights
    for (Entity* entity : activeLights_) {
        if (entity && entity->IsActive()) {
            UpdateLight(entity, deltaTime);
        }
    }
    
    lightUpdateCount_++;
    
    // Log statistics every 5 seconds
    static int logCounter = 0;
    logCounter++;
    if (logCounter % 300 == 0) {  // 300 frames at 60fps = 5 seconds
        LogLightStats();
    }
}

void LightSystem::Render() {
    // LightSystem doesn't do direct rendering - it updates shader uniforms
    // The actual rendering is handled by RenderSystem
}

void LightSystem::Shutdown() {
    LOG_INFO("LightSystem shutting down...");
    
    activeLights_.clear();
    shaderLights_.clear();
    lightCache_.reset();
    
    LOG_INFO("LightSystem shutdown complete");
}

void LightSystem::RegisterLight(Entity* entity) {
    if (!entity) {
        LOG_WARNING("Cannot register null light entity");
        return;
    }
    
    auto lightComp = entity->GetComponent<LightComponent>();
    if (!lightComp) {
        LOG_WARNING("Entity " + std::to_string(entity->GetId()) + " has no LightComponent");
        return;
    }
    
    // Check if already registered
    auto it = std::find(activeLights_.begin(), activeLights_.end(), entity);
    if (it != activeLights_.end()) {
        LOG_DEBUG("Light entity " + std::to_string(entity->GetId()) + " already registered");
        return;
    }
    
    // Check light limit
    if (activeLights_.size() >= MAX_LIGHTS) {
        LOG_WARNING("Maximum light count (" + std::to_string(MAX_LIGHTS) + ") reached, ignoring new light");
        return;
    }
    
    activeLights_.push_back(entity);
    LOG_INFO("🔆 Registered light entity " + std::to_string(entity->GetId()) + 
             " (total: " + std::to_string(activeLights_.size()) + ")");
}

void LightSystem::UnregisterLight(Entity* entity) {
    auto it = std::find(activeLights_.begin(), activeLights_.end(), entity);
    if (it != activeLights_.end()) {
        activeLights_.erase(it);
        LOG_INFO("Unregistered light entity " + std::to_string(entity->GetId()));
    }
}

void LightSystem::UpdateShaderLights(Shader& shader) {
    if (!globalLightingEnabled_) {
        return;
    }

    // Performance optimization: only update lights if they've changed
    if (!ShouldUpdateLights()) {
        return;
    }
    
    // Clear shader lights
    shaderLights_.clear();
    
    // Build list of lights to send to shader
    for (Entity* entity : activeLights_) {
        if (!entity || !entity->IsActive()) continue;
        
        auto lightComp = entity->GetComponent<LightComponent>();
        auto transform = entity->GetComponent<TransformComponent>();
        
        if (!lightComp || !lightComp->enabled || !transform) continue;
        
        RaylibLight raylibLight = CreateRaylibLight(lightComp, transform);
        shaderLights_.push_back(raylibLight);
        
        if (shaderLights_.size() >= MAX_LIGHTS) {
            LOG_WARNING("⚠️ Light limit reached (" + std::to_string(MAX_LIGHTS) + "), skipping additional lights");
            break;  // Shader light limit reached
        }
    }
    
    // Update shader uniforms
    int lightCount = static_cast<int>(shaderLights_.size());
    
    // NOTE: Raylib doesn't use numOfLights uniform - lights array is fixed size
    // The shader loops through MAX_LIGHTS and checks enabled flag for each light
    
    // Update individual lights
    for (int i = 0; i < lightCount; ++i) {
        UpdateShaderLight(shader, shaderLights_[i], i);
    }
    
    // Update activeLightCount uniform so shader only processes active lights
    if (shaderLocs_.lightCountLoc == -1) {
        shaderLocs_.lightCountLoc = GetShaderLocation(shader, "activeLightCount");
    }
    if (shaderLocs_.lightCountLoc != -1) {
        SetShaderValue(shader, shaderLocs_.lightCountLoc, &lightCount, SHADER_UNIFORM_INT);
        LOG_DEBUG("💡 Updated activeLightCount uniform: " + std::to_string(lightCount));
    } else {
        LOG_WARNING("❌ activeLightCount uniform location not found in shader!");
    }
    
    // Update ambient lighting if dirty
    if (ambientLightDirty_) {
        SetShaderAmbientLight(shader, ambientColor_, ambientIntensity_);
        ambientLightDirty_ = false;
    }
    
    // Update camera position for specular lighting calculations
    UpdateViewPosUniform(shader);
    
    shaderUpdateCount_++;
}

void LightSystem::SetShaderAmbientLight(Shader& shader, Color ambientColor, float ambientIntensity) {
    // Cache shader location (Raylib uses vec4 ambient, not separate color/intensity)
    if (shaderLocs_.ambientColorLoc == -1) {
        shaderLocs_.ambientColorLoc = GetShaderLocation(shader, "ambient");
    }
    
    // Set ambient as vec4 (color + intensity combined, Raylib style)
    if (shaderLocs_.ambientColorLoc != -1) {
        Vector4 ambientVec4 = {
            (ambientColor.r / 255.0f) * ambientIntensity,
            (ambientColor.g / 255.0f) * ambientIntensity,
            (ambientColor.b / 255.0f) * ambientIntensity,
            ambientColor.a / 255.0f
        };
        SetShaderValue(shader, shaderLocs_.ambientColorLoc, &ambientVec4, SHADER_UNIFORM_VEC4);
        LOG_DEBUG("🔆 Set ambient light: (" + std::to_string(ambientVec4.x) + ", " + 
                  std::to_string(ambientVec4.y) + ", " + std::to_string(ambientVec4.z) + 
                  ", " + std::to_string(ambientVec4.w) + ")");
    }
}

void LightSystem::SetAmbientLight(Color color, float intensity) {
    if (ambientColor_.r != color.r || ambientColor_.g != color.g || 
        ambientColor_.b != color.b || ambientColor_.a != color.a ||
        fabsf(ambientIntensity_ - intensity) > 0.001f) {
        
        ambientColor_ = color;
        ambientIntensity_ = intensity;
        ambientLightDirty_ = true;
        
        LOG_INFO("🌅 Updated ambient light: color=(" + std::to_string(color.r) + "," + 
                std::to_string(color.g) + "," + std::to_string(color.b) + "), intensity=" + 
                std::to_string(intensity));
    }
}

std::vector<Entity*> LightSystem::GetActiveLights() const {
    return activeLights_;
}

void LightSystem::LogLightStats() const {
    LOG_INFO("🔆 LightSystem Stats:");
    LOG_INFO("  - Active lights: " + std::to_string(activeLights_.size()) + "/" + std::to_string(MAX_LIGHTS));
    LOG_INFO("  - Cache size: " + std::to_string(lightCache_->Size()));
    LOG_INFO("  - Light updates: " + std::to_string(lightUpdateCount_));
    LOG_INFO("  - Shader updates: " + std::to_string(shaderUpdateCount_));
    LOG_INFO("  - Global lighting: " + std::string(globalLightingEnabled_ ? "enabled" : "disabled"));
    LOG_INFO("  - Ambient: (" + std::to_string(ambientColor_.r) + "," + 
             std::to_string(ambientColor_.g) + "," + std::to_string(ambientColor_.b) + 
             ") intensity=" + std::to_string(ambientIntensity_));
}

void LightSystem::UpdateLight(Entity* entity, float deltaTime) {
    auto lightComp = entity->GetComponent<LightComponent>();
    if (!lightComp || !lightComp->enabled) {
        return;
    }
    
    // Get or create cached light data
    uint32_t lightId = lightCache_->GetOrCreate(*lightComp);
    auto cachedLight = lightCache_->GetMutable(lightId);
    
    if (cachedLight && cachedLight->isDirty) {
        // Update cached light properties if needed
        cachedLight->isDirty = false;
    }
}

RaylibLight LightSystem::CreateRaylibLight(const LightComponent* lightComp, const TransformComponent* transform) const {
    RaylibLight light = {};
    
    // Convert light type
    switch (lightComp->type) {
        case LightType::DIRECTIONAL:
            light.type = 0;
            light.target = Vector3Add(transform->position, {0, -1, 0});  // Point downward
            break;
        case LightType::POINT:
            light.type = 1;
            light.target = {0, 0, 0};  // Not used for point lights
            break;
        case LightType::SPOT:
            light.type = 2;
            light.target = Vector3Add(transform->position, {0, -1, 0});  // Point downward
            break;
    }
    
    light.enabled = lightComp->enabled ? 1 : 0;
    light.position = transform->position;
    
    // Normalize color and apply toned-down intensity for brightness
    float intensity = (lightComp->intensity * globalLightIntensity_) * 0.001f; // Much more reasonable scaling
    light.color[0] = (lightComp->color.r / 255.0f) * intensity;
    light.color[1] = (lightComp->color.g / 255.0f) * intensity;
    light.color[2] = (lightComp->color.b / 255.0f) * intensity;
    light.color[3] = (lightComp->color.a / 255.0f);

    // Use a small constant for attenuation (distance falloff)
    light.attenuation = 0.1f;
    
    return light;
}

void LightSystem::UpdateViewPosUniform(Shader& shader) {
    // Get camera position from the Engine's renderer system
    auto renderSystem = Engine::GetInstance().GetSystem<RenderSystem>();
    if (renderSystem) {
        Vector3 cameraPos = renderSystem->GetRenderer()->GetCameraPosition();
        
        // Cache shader location for viewPos uniform
        if (shaderLocs_.viewPosLoc == -1) {
            shaderLocs_.viewPosLoc = GetShaderLocation(shader, "viewPos");
        }
        
        // Update viewPos uniform for specular lighting calculations
        if (shaderLocs_.viewPosLoc != -1) {
            float viewPos[3] = {cameraPos.x, cameraPos.y, cameraPos.z};
            SetShaderValue(shader, shaderLocs_.viewPosLoc, viewPos, SHADER_UNIFORM_VEC3);
            LOG_DEBUG("🎥 Updated viewPos uniform: (" + std::to_string(cameraPos.x) + ", " + 
                     std::to_string(cameraPos.y) + ", " + std::to_string(cameraPos.z) + ")");
        } else {
            LOG_WARNING("❌ viewPos uniform location not found in shader!");
        }
    } else {
        LOG_WARNING("❌ Could not get RenderSystem to update viewPos uniform");
    }
}

void LightSystem::RenderShadowMap(Shader& depthShader, const std::vector<Entity*>& entities) {
    // Delegate shadow rendering to RenderSystem
    auto renderSystem = Engine::GetInstance().GetSystem<RenderSystem>();
    if (renderSystem) {
        renderSystem->RenderShadowsToTexture(entities);
    }
}

void LightSystem::SetupLightCamera(Entity* directionalLightEntity, const Vector3& mainCameraPos) {
    if (!directionalLightEntity) {
        return;
    }

    auto lightComp = directionalLightEntity->GetComponent<LightComponent>();
    auto lightTransform = directionalLightEntity->GetComponent<TransformComponent>();

    if (!lightComp || !lightTransform) {
        return;
    }

    // For directional lights, position camera relative to main camera (Substack approach)
    // This ensures the shadow map covers the area visible to the player
    Vector3 lightOffset = {400.0f, 400.0f, 400.0f}; // Offset from main camera
    lightCamera_.position = Vector3Add(mainCameraPos, lightOffset);
    lightCamera_.target = mainCameraPos; // Look at main camera position
    lightCamera_.up = {0.0f, 1.0f, 0.0f}; // Standard up vector
    lightCamera_.projection = CAMERA_ORTHOGRAPHIC;
    lightCamera_.fovy = 20.0f; // Not used for orthographic, but set for consistency

    LOG_DEBUG("🔆 Light camera setup - Main camera pos: (" +
              std::to_string(mainCameraPos.x) + ", " + std::to_string(mainCameraPos.y) + ", " + std::to_string(mainCameraPos.z) +
              "), Light camera pos: (" + std::to_string(lightCamera_.position.x) + ", " +
              std::to_string(lightCamera_.position.y) + ", " + std::to_string(lightCamera_.position.z) + ")");
}

void LightSystem::UpdateShaderLight(Shader& shader, const RaylibLight& light, int lightIndex) {
    // Cache shader locations if needed
    if (light.typeLoc == -1) {
        const_cast<RaylibLight&>(light).typeLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].type", lightIndex));
        const_cast<RaylibLight&>(light).enabledLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].enabled", lightIndex));
        const_cast<RaylibLight&>(light).positionLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].position", lightIndex));
        const_cast<RaylibLight&>(light).targetLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].target", lightIndex));
        const_cast<RaylibLight&>(light).colorLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].color", lightIndex));
        const_cast<RaylibLight&>(light).attenuationLoc = GetShaderLocation(shader, 
            TextFormat("lights[%i].attenuation", lightIndex));
            
        // DEBUG: Log shader uniform locations
        LOG_DEBUG("🔍 Light[" + std::to_string(lightIndex) + "] uniform locations:");
        LOG_DEBUG("  - type: " + std::to_string(light.typeLoc));
        LOG_DEBUG("  - enabled: " + std::to_string(light.enabledLoc));
        LOG_DEBUG("  - position: " + std::to_string(light.positionLoc));
        LOG_DEBUG("  - target: " + std::to_string(light.targetLoc));
        LOG_DEBUG("  - color: " + std::to_string(light.colorLoc));
        LOG_DEBUG("  - attenuation: " + std::to_string(light.attenuationLoc));
    }
    
    // Update shader uniforms with debug logging
    if (light.typeLoc != -1) {
        SetShaderValue(shader, light.typeLoc, &light.type, SHADER_UNIFORM_INT);
        LOG_DEBUG("🔆 Set light[" + std::to_string(lightIndex) + "].type = " + std::to_string(light.type));
    } else {
        LOG_WARNING("❌ Light[" + std::to_string(lightIndex) + "].type uniform location not found!");
    }
    
    if (light.enabledLoc != -1) {
        SetShaderValue(shader, light.enabledLoc, &light.enabled, SHADER_UNIFORM_INT);
        LOG_DEBUG("🔆 Set light[" + std::to_string(lightIndex) + "].enabled = " + std::to_string(light.enabled));
    } else {
        LOG_WARNING("❌ Light[" + std::to_string(lightIndex) + "].enabled uniform location not found!");
    }
    
    if (light.positionLoc != -1) {
        float position[3] = { light.position.x, light.position.y, light.position.z };
        SetShaderValue(shader, light.positionLoc, position, SHADER_UNIFORM_VEC3);
        LOG_DEBUG("🔆 Set light[" + std::to_string(lightIndex) + "].position = (" + 
                 std::to_string(position[0]) + ", " + std::to_string(position[1]) + ", " + std::to_string(position[2]) + ")");
    } else {
        LOG_WARNING("❌ Light[" + std::to_string(lightIndex) + "].position uniform location not found!");
    }
    
    if (light.targetLoc != -1) {
        float target[3] = { light.target.x, light.target.y, light.target.z };
        SetShaderValue(shader, light.targetLoc, target, SHADER_UNIFORM_VEC3);
    }
    
    if (light.colorLoc != -1) {
        SetShaderValue(shader, light.colorLoc, light.color, SHADER_UNIFORM_VEC4);
        LOG_DEBUG("🔆 Set light[" + std::to_string(lightIndex) + "].color = (" + 
                 std::to_string(light.color[0]) + ", " + std::to_string(light.color[1]) + ", " + 
                 std::to_string(light.color[2]) + ", " + std::to_string(light.color[3]) + ")");
    } else {
        LOG_WARNING("❌ Light[" + std::to_string(lightIndex) + "].color uniform location not found!");
    }
    
    if (light.attenuationLoc != -1) {
        SetShaderValue(shader, light.attenuationLoc, &light.attenuation, SHADER_UNIFORM_FLOAT);
        LOG_DEBUG("🔆 Set light[" + std::to_string(lightIndex) + "].attenuation = " + std::to_string(light.attenuation));
    } else {
        LOG_WARNING("❌ Light[" + std::to_string(lightIndex) + "].attenuation uniform location not found!");
    }
}
