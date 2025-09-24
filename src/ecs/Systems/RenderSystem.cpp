#include "RenderSystem.h"
#include "../Entity.h"
#include "../../ecs/Components/Position.h"
#include "../../ecs/Components/Sprite.h"
#include "../../ecs/Components/MeshComponent.h"
#include "../../ecs/Systems/MeshSystem.h"  // Include before Engine.h for template instantiation
#include "../../ecs/Systems/WorldSystem.h"
#include "../../ecs/Systems/LightSystem.h"
#include "../../ecs/Systems/GameObjectSystem.h"
#include "../../shaders/ShaderSystem.h"
#include "../../core/Engine.h"
#include "../../utils/Logger.h"

RenderSystem::RenderSystem()
    : meshSystem_(nullptr), assetSystem_(nullptr), bspTree_(nullptr),
      debugRendering_(true), gridEnabled_(true), visibilityCullingEnabled_(true)
{
    // Initialize the culling camera once
    cullingCamera_.position = {0.0f, 0.0f, 0.0f};
    cullingCamera_.target = {0.0f, 0.0f, -1.0f};
    cullingCamera_.up = {0.0f, 1.0f, 0.0f};
    cullingCamera_.fovy = 45.0f;
    cullingCamera_.projection = CAMERA_PERSPECTIVE;

    LOG_INFO("RenderSystem constructed; signature set during Initialize()");
}

RenderSystem::~RenderSystem()
{
    LOG_INFO("RenderSystem destroyed");
}

void RenderSystem::Initialize()
{
    LOG_INFO("RenderSystem::Initialize - setting up TransformComponent+(Sprite|Mesh) signature");

    // Set signature to get entities with TransformComponent AND (Sprite OR Mesh)
    // We'll handle the OR logic in CollectRenderCommands
    SetSignature<TransformComponent>();

    // Get system references
    meshSystem_ = engine_.GetSystem<MeshSystem>();
    assetSystem_ = engine_.GetSystem<AssetSystem>();
    auto worldSystem = engine_.GetSystem<WorldSystem>();

        if (meshSystem_) {
            LOG_INFO("RenderSystem acquired MeshSystem reference");
        } else {
            LOG_WARNING("RenderSystem could not acquire MeshSystem reference");
        }

        if (assetSystem_) {
            LOG_INFO("RenderSystem acquired AssetSystem reference");
        } else {
            LOG_WARNING("RenderSystem could not acquire AssetSystem reference");
        }

        if (worldSystem && worldSystem->GetWorldGeometry()) {
            renderer_.SetWorldGeometry(worldSystem->GetWorldGeometry());
            LOG_INFO("RenderSystem connected to WorldSystem geometry");
        } else {
            LOG_WARNING("RenderSystem could not acquire WorldSystem or WorldGeometry reference");
        }

        // Set engine reference on renderer for ECS access
        // Renderer now uses singleton Engine

        // Pass system references to Renderer
        if (meshSystem_) {
            renderer_.SetMeshSystem(meshSystem_);
        }
        if (assetSystem_) {
            renderer_.SetAssetSystem(assetSystem_);
        }

    LOG_INFO("RenderSystem signature set (requires TransformComponent, optional Sprite/Mesh)");
    LOG_INFO("RenderSystem initialized and ready for entity registration");
}

void RenderSystem::Update(float deltaTime)
{
    // Systems with signatures might not get Update() called by the engine
    // Collect render commands here as a fallback
    CollectRenderCommands();
    SortRenderCommands();
}

void RenderSystem::Render()
{
    LOG_DEBUG("RenderSystem::Render called");

    // Ensure render commands are collected (in case Update() wasn't called)
    CollectRenderCommands();
    SortRenderCommands();

    // Render phase - execute the actual rendering
    ExecuteRenderCommands();
    LOG_DEBUG("RenderSystem::Render completed");
}

// Render world geometry directly (not through command system to avoid OpenGL conflicts)
void RenderSystem::RenderWorldGeometryDirect()
{
    if (!renderer_.GetWorldGeometry() || !renderer_.IsWorldRenderingEnabled()) {
        LOG_DEBUG("RenderWorldGeometryDirect: Skipping world geometry rendering");
        return;
    }

    LOG_DEBUG("RenderWorldGeometryDirect: Starting world geometry rendering");

    // Begin 3D mode for world geometry
    BeginMode3D(renderer_.GetCamera());

    // Render the world geometry using the renderer's method
    renderer_.RenderWorldGeometry();

    // End 3D mode
    EndMode3D();

    LOG_DEBUG("RenderWorldGeometryDirect: World geometry rendering completed");
}

// Collect world geometry as render commands (goes through unified command system)
void RenderSystem::CollectWorldGeometryCommands()
{
    if (!renderer_.GetWorldGeometry() || !renderer_.IsWorldRenderingEnabled()) {
        return;
    }

    // For now, we'll add a single world geometry command
    // In a more advanced implementation, we could batch faces or create multiple commands
    RenderCommand worldCommand(nullptr, nullptr, nullptr, nullptr, nullptr, RenderType::WORLD_GEOMETRY);
    worldCommand.entity = nullptr;
    worldCommand.transform = nullptr;
    worldCommand.sprite = nullptr;
    worldCommand.mesh = nullptr;
    worldCommand.type = RenderType::WORLD_GEOMETRY;
    worldCommand.depth = 0.0f; // Render world geometry first (behind everything else)

    renderCommands_.push_back(worldCommand);

    LOG_DEBUG("Added world geometry command to render pipeline");
}

void RenderSystem::CollectRenderCommands()
{
    renderCommands_.clear();

    // Update culling camera position based on the actual render camera
    cullingCamera_.position = renderer_.GetCameraPosition();
    cullingCamera_.target = renderer_.GetCameraTarget();
    cullingCamera_.up = {0.0f, 1.0f, 0.0f};
    cullingCamera_.fovy = renderer_.GetCameraZoom();
    cullingCamera_.projection = CAMERA_PERSPECTIVE;

    // Collect world geometry commands first (static geometry)
    CollectWorldGeometryCommands();

    // Get entities that match our TransformComponent signature
    const auto& entities = GetEntities();

    LOG_INFO("RenderSystem: processing " + std::to_string(entities.size()) + " entities with TransformComponent signature");
    for (const auto& entity : entities) {
        LOG_INFO("RenderSystem: Found entity " + std::to_string(entity->GetId()) + " with TransformComponent");
        auto gameObj = entity->GetComponent<GameObject>();
        if (gameObj) {
            LOG_INFO("RenderSystem: Entity " + std::to_string(entity->GetId()) + " is GameObject '" + gameObj->name + "' of class '" + gameObj->className + "'");
        }
    }
    if (entities.empty()) {
        LOG_DEBUG("No entities match RenderSystem signature.");
        return;
    }

    int processedCount = 0;
    int skippedCount = 0;
    int culledCount = 0;

    for (Entity* entity : entities) {
        if (!entity) {
            LOG_WARNING("Null entity in RenderSystem");
            skippedCount++;
            continue;
        }

        if (!entity->IsActive()) {
            LOG_DEBUG("Skipping inactive entity " + std::to_string(entity->GetId()));
            skippedCount++;
            continue;
        }

        auto transform = entity->GetComponent<TransformComponent>();
        auto sprite = entity->GetComponent<Sprite>();
        auto mesh = entity->GetComponent<MeshComponent>();

        // Perform visibility culling if enabled and BSP tree is available
        if (visibilityCullingEnabled_ && bspTree_) {
            Vector3 entityPos = transform->position;

            // TODO: Use BSPTreeSystem for point containment check
            // For now, skip this check as proper culling is done in Renderer

            // For mesh entities, we could do more advanced frustum culling here
            // For now, we'll rely on the BSP bounds check
        }

        LOG_DEBUG("Processing entity " + std::to_string(entity->GetId()) +
                " - Position: (" + std::to_string(transform->position.x) + ", " +
                std::to_string(transform->position.y) + ", " + std::to_string(transform->position.z) + ")");

        // Check for light components to render light gizmos
        auto light = entity->GetComponent<LightComponent>();
        
        // Only process entities that have visual components (Sprite, Mesh) or are lights
        if (!sprite && !mesh && !light) {
            LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " has TransformComponent but no visual components - skipping");
            skippedCount++;
            continue;
        }

        // Handle Sprite entities
        if (sprite) {
            // Determine render type based on sprite properties
            RenderType renderType = RenderType::SPRITE_2D;
            if (!sprite->IsTextureLoaded()) {
                // No texture means it's a primitive (like a cube)
                renderType = RenderType::PRIMITIVE_3D;
            }

            RenderCommand command(entity, transform, sprite, nullptr, nullptr, renderType);
            // Calculate depth as distance from camera (better than just Z for rotated cameras)
            Vector3 cameraPos = renderer_.GetCameraPosition();
            command.depth = Vector3Distance(cameraPos, transform->position);
            renderCommands_.push_back(command);

            LOG_DEBUG("Added Sprite entity " + std::to_string(entity->GetId()) +
                     " to render commands - Type: " + (renderType == RenderType::SPRITE_2D ? "2D Sprite" : "3D Primitive"));
        }
        // Handle Mesh entities
        else if (mesh) {
            // Check frustum culling first
            if (!renderer_.IsEntityVisible(transform->position, 2.0f)) { // Assume 2 unit bounding radius
                skippedCount++;
                continue;
            }

            // Check if entity has a MaterialComponent
            MaterialComponent* material = nullptr;
            if (entity->HasComponent<MaterialComponent>()) {
                material = entity->GetComponent<MaterialComponent>();
                LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " has MaterialComponent");
            } else {
                LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " does NOT have MaterialComponent");
            }
            RenderCommand command(entity, transform, nullptr, mesh, material, RenderType::MESH_3D);
            // Calculate depth as distance from camera (better than just Z for rotated cameras)
            Vector3 cameraPos = renderer_.GetCameraPosition();
            command.depth = Vector3Distance(cameraPos, transform->position);
            renderCommands_.push_back(command);

            LOG_DEBUG("Added Mesh entity " + std::to_string(entity->GetId()) +
                     " to render commands - Type: 3D Mesh");
        }
        // Handle Light entities (render as visible gizmos)
        else if (light) {
            RenderCommand command(entity, transform, nullptr, nullptr, nullptr, RenderType::LIGHT_GIZMO);
            // Calculate depth as distance from camera
            Vector3 cameraPos = renderer_.GetCameraPosition();
            command.depth = Vector3Distance(cameraPos, transform->position);
            renderCommands_.push_back(command);

            LOG_DEBUG("Added Light entity " + std::to_string(entity->GetId()) +
                     " to render commands - Type: Light Gizmo");
        }

        processedCount++;
    }

    LOG_DEBUG("RenderSystem summary:");
    LOG_DEBUG("  - Total entities: " + std::to_string(entities.size()));
    LOG_DEBUG("  - Processed: " + std::to_string(processedCount));
    LOG_DEBUG("  - Skipped: " + std::to_string(skippedCount));
    LOG_DEBUG("  - Culled by BSP: " + std::to_string(culledCount));
    LOG_DEBUG("  - Final render commands: " + std::to_string(renderCommands_.size()));
}

void RenderSystem::SortRenderCommands()
{
    // Enhanced sorting for material batching:
    // 1. Sort by material ID first (to minimize state changes)
    // 2. Then by render type (to group similar operations)  
    // 3. Finally by depth (for proper transparency)
    std::sort(renderCommands_.begin(), renderCommands_.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            // First priority: Material ID (for batching)
            uint32_t materialA = 0, materialB = 0;
            if (a.material) materialA = a.material->materialId;
            if (b.material) materialB = b.material->materialId;
            
            if (materialA != materialB) {
                return materialA < materialB;
            }
            
            // Second priority: Render type (group similar operations)
            if (a.type != b.type) {
                return static_cast<int>(a.type) < static_cast<int>(b.type);
            }
            
            // Third priority: Depth (front to back for opaque, back to front for transparent)
            // For now, we'll use front-to-back for all objects to help with early Z rejection
            return a.depth < b.depth;
        });
        
    LOG_DEBUG("Sorted " + std::to_string(renderCommands_.size()) + " render commands by material batching");
}

void RenderSystem::ExecuteRenderCommands()
{
    // Reset and track batching statistics
    batchingStats_.Reset();
    batchingStats_.totalCommands = renderCommands_.size();
    
    LOG_DEBUG("RenderSystem: executing " + std::to_string(renderCommands_.size()) + " render commands");
    if (renderCommands_.empty()) {
        LOG_WARNING("No render commands to execute. Check entity registration.");
    }

    // UNIFIED SHADER PIPELINE - Phase 2.1
    // Setup lighting system and get shader BEFORE any rendering
    auto lightSystem = engine_.GetSystem<LightSystem>();
    auto shaderSystem = engine_.GetSystem<ShaderSystem>();
    Shader* lightingShader = nullptr;
    
    if (lightSystem && shaderSystem) {
        // Get or create the lighting shader
        uint32_t lightingShaderId = shaderSystem->GetOrCreateDefaultShader(ShaderType::LIGHTING);
        LOG_DEBUG("🔍 Lighting shader ID: " + std::to_string(lightingShaderId));
        if (lightingShaderId != 0) {
            lightingShader = shaderSystem->GetShader(lightingShaderId);
            if (lightingShader) {
                // Update lighting uniforms ONCE per frame
                lightSystem->UpdateShaderLights(*lightingShader);
                LOG_DEBUG("🔆 Updated lighting for unified shader pipeline");
            } else {
                LOG_WARNING("⚠️ Failed to get lighting shader object for ID " + std::to_string(lightingShaderId));
            }
        } else {
            LOG_WARNING("⚠️ Lighting shader ID is 0 - shader not created");
        }
    } else {
        LOG_WARNING("⚠️ Missing LightSystem or ShaderSystem");
    }

    // Render shadows first (before main rendering)
    if (lightSystem && lightSystem->IsShadowMappingEnabled()) {
        auto shaderSystem = engine_.GetSystem<ShaderSystem>();
        if (shaderSystem) {
            uint32_t depthShaderId = shaderSystem->GetDepthShader();
            if (depthShaderId != 0) {
                Shader* depthShader = shaderSystem->GetShader(depthShaderId);
                if (depthShader) {
                    // Collect all entities for shadow rendering
                    std::vector<Entity*> allEntities = GetAllEntitiesForRendering();
                    lightSystem->RenderShadowMap(*depthShader, allEntities);
                    LOG_DEBUG("🌑 Rendered shadow map");
                }
            }
        }
    }

    // Apply lighting shader to ALL rendering (BSP + entities)
    if (lightingShader) {
        BeginShaderMode(*lightingShader);
        renderer_.SetCurrentShader(*lightingShader);  // Track current shader for gizmo restoration

        // Set shadow mapping uniforms
        if (lightSystem && lightSystem->IsShadowMappingEnabled()) {
            RenderTexture2D shadowMap = lightSystem->GetShadowMap();
            Matrix lightVPMatrix = lightSystem->GetLightViewProjectionMatrix();

            // Bind shadow map texture (following Substack approach)
            rlActiveTextureSlot(10);
            // Try using the texture attachment instead of depth (like Substack)
            rlEnableTexture(shadowMap.texture.id > 0 ? shadowMap.texture.id : shadowMap.depth.id);

            int shadowMapLoc = GetShaderLocation(*lightingShader, "shadowMap");
            LOG_DEBUG("🔆 shadowMap uniform location: " + std::to_string(shadowMapLoc));
            if (shadowMapLoc != -1) {
                int textureSlot = 10;
                // Try using rlSetUniform like the Raylib example
                rlSetUniform(shadowMapLoc, &textureSlot, SHADER_UNIFORM_INT, 1);
                LOG_DEBUG("🔆 Set shadowMap texture slot to: " + std::to_string(textureSlot) + " using rlSetUniform");
            }

            int lightVPLoc = GetShaderLocation(*lightingShader, "lightVP");
            LOG_DEBUG("🔆 lightVP uniform location: " + std::to_string(lightVPLoc));
            if (lightVPLoc != -1) {
                SetShaderValueMatrix(*lightingShader, lightVPLoc, lightVPMatrix);
                LOG_DEBUG("🔆 Set lightVP matrix");
            }

            int shadowsEnabledLoc = GetShaderLocation(*lightingShader, "shadowsEnabled");
            LOG_DEBUG("🔆 shadowsEnabled uniform location: " + std::to_string(shadowsEnabledLoc));
            if (shadowsEnabledLoc != -1) {
                int enabled = 1;
                SetShaderValue(*lightingShader, shadowsEnabledLoc, &enabled, SHADER_UNIFORM_INT);
                LOG_DEBUG("🔆 Set shadowsEnabled to: " + std::to_string(enabled));
            }

            int shadowMapResolutionLoc = GetShaderLocation(*lightingShader, "shadowMapResolution");
            LOG_DEBUG("🔆 shadowMapResolution uniform location: " + std::to_string(shadowMapResolutionLoc));
            if (shadowMapResolutionLoc != -1) {
                int resolution = 1024; // Match SHADOW_MAP_RESOLUTION constant
                SetShaderValue(*lightingShader, shadowMapResolutionLoc, &resolution, SHADER_UNIFORM_INT);
                LOG_DEBUG("🔆 Set shadowMapResolution to: " + std::to_string(resolution));
            }

            // Reset to texture slot 0
            rlActiveTextureSlot(0);
        }

        LOG_DEBUG("🎨 Applied lighting shader to unified pipeline");
    }

    // Render world geometry WITH lighting shader
    RenderWorldGeometryDirect();

    renderer_.BeginFrame();

    // Draw grid if enabled
    if (gridEnabled_) {
        renderer_.DrawGrid(50.0f, Fade(LIGHTGRAY, 0.3f));
    }

    // Draw all render commands using the dispatcher (dynamic entities only)
    uint32_t lastMaterialId = UINT32_MAX; // Track material changes for batching stats
    int currentBatch = 0;
    int commandsInCurrentBatch = 0;
    
    for (const auto& command : renderCommands_) {
        // Track material state changes
        uint32_t currentMaterialId = 0;
        if (command.material) {
            currentMaterialId = command.material->materialId;
        }
        
        if (currentMaterialId != lastMaterialId) {
            // If we had a previous batch, record its size
            if (currentBatch > 0) {
                batchingStats_.averageBatchSize = 
                    (batchingStats_.averageBatchSize * (currentBatch - 1) + commandsInCurrentBatch) / currentBatch;
            }
            
            batchingStats_.stateChanges++;
            currentBatch++;
            commandsInCurrentBatch = 1;
            lastMaterialId = currentMaterialId;
        } else {
            commandsInCurrentBatch++;
        }
        std::string typeStr;
        switch (command.type) {
            case RenderType::SPRITE_2D: typeStr = "2D Sprite"; break;
            case RenderType::PRIMITIVE_3D: typeStr = "3D Primitive"; break;
            case RenderType::MESH_3D: typeStr = "3D Mesh"; break;
            // WORLD_GEOMETRY commands are handled separately
            case RenderType::WORLD_GEOMETRY: typeStr = "World Geometry"; break;
            case RenderType::LIGHT_GIZMO: typeStr = "Light Gizmo"; break;
            case RenderType::DEBUG: typeStr = "Debug"; break;
            default: typeStr = "Unknown"; break;
        }

        // Handle commands with null entity pointers (like WORLD_GEOMETRY)
        if (command.entity) {
            LOG_DEBUG("Rendering entity " + std::to_string(command.entity->GetId()) +
                     " at (" + std::to_string(command.transform->position.x) + ", " +
                     std::to_string(command.transform->position.y) + ", " + std::to_string(command.transform->position.z) +
                     ") Type: " + typeStr);
        } else {
            LOG_DEBUG("Rendering special command - Type: " + typeStr);
        }

        // Skip WORLD_GEOMETRY commands as they're handled separately
        if (command.type == RenderType::WORLD_GEOMETRY) {
            LOG_DEBUG("Skipping WORLD_GEOMETRY command (handled separately)");
            continue;
        }

        renderer_.DrawRenderCommand(command);
    }

    // Complete final batch statistics
    if (currentBatch > 0 && commandsInCurrentBatch > 0) {
        batchingStats_.averageBatchSize = 
            (batchingStats_.averageBatchSize * (currentBatch - 1) + commandsInCurrentBatch) / currentBatch;
    }
    batchingStats_.totalBatches = currentBatch;

    renderer_.EndFrame();

    // End unified shader mode AFTER all 3D rendering but BEFORE debug UI
    if (lightingShader) {
        EndShaderMode();
        renderer_.ClearCurrentShader();  // Clear current shader tracking
        LOG_DEBUG("🎨 Ended unified shader mode");
    }

    // Draw minimal debug info (optional overlay) - AFTER EndShaderMode
    if (debugRendering_) {
        DrawText(TextFormat("Meshes: %d", renderer_.GetSpritesRendered()), 10, 10, 16, YELLOW);
        auto camPos = renderer_.GetCameraPosition();
        DrawText(TextFormat("Cam: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z), 10, 30, 16, WHITE);
        
        // Display culling statistics
        auto& cullingStats = renderer_.GetCullingStats();
        DrawText(TextFormat("Culling: %d checked, %d visible (%.1f%% culled)", 
                           cullingStats.totalEntitiesChecked, 
                           cullingStats.entitiesVisible,
                           cullingStats.GetCullRate() * 100.0f), 10, 50, 16, GREEN);
        DrawText(TextFormat("Distance: %d, Frustum: %d", 
                           cullingStats.entitiesCulledByDistance,
                           cullingStats.entitiesCulledByFrustum), 10, 70, 16, ORANGE);
        
        // Display batching statistics
        DrawText(TextFormat("Batching: %d cmds, %d batches, %.1f avg", 
                           batchingStats_.totalCommands,
                           batchingStats_.totalBatches,
                           batchingStats_.averageBatchSize), 10, 90, 16, SKYBLUE);
        DrawText(TextFormat("State changes: %d (%.1f%% efficiency)", 
                           batchingStats_.stateChanges,
                           batchingStats_.GetBatchingEfficiency() * 100.0f), 10, 110, 16, PURPLE);
    }
}

std::vector<Entity*> RenderSystem::GetAllEntitiesForRendering() {
    std::vector<Entity*> allEntities;

    // Get all game objects from GameObjectSystem
    auto* goSystem = engine_.GetSystem<GameObjectSystem>();
    if (goSystem) {
        allEntities = goSystem->GetActiveGameObjects();
    }

    return allEntities;
}

// Shadow rendering support
void RenderSystem::BeginShadowMode(Shader& depthShader) {
    renderer_.BeginShadowMode(depthShader);
}

void RenderSystem::EndShadowMode() {
    renderer_.EndShadowMode();
}

void RenderSystem::RenderShadowsToTexture(const std::vector<Entity*>& shadowCastingEntities) {
    auto lightSystem = Engine::GetInstance().GetSystem<LightSystem>();
    auto shaderSystem = Engine::GetInstance().GetSystem<ShaderSystem>();

    if (!lightSystem || !shaderSystem) {
        return;
    }

    // Get the depth shader for shadow rendering
    uint32_t depthShaderId = shaderSystem->GetDepthShader();
    Shader* depthShader = shaderSystem->GetShader(depthShaderId);
    if (!depthShader) {
        LOG_ERROR("❌ Depth shader not available for shadow rendering");
        return;
    }

    // Find the directional light entity for shadows
    Entity* directionalLightEntity = nullptr;
    auto activeLights = lightSystem->GetActiveLights();
    for (Entity* lightEntity : activeLights) {
        auto lightComp = lightEntity->GetComponent<LightComponent>();
        if (lightComp && lightComp->type == LightType::DIRECTIONAL && lightComp->enabled) {
            directionalLightEntity = lightEntity;
            break;
        }
    }

    if (!directionalLightEntity) {
        LOG_DEBUG("🔆 No directional light entity found for shadow rendering");
        return;
    }

    // Get main camera position from renderer
    Vector3 mainCameraPos = renderer_.GetCameraPosition();

    // Setup light camera for shadow mapping (Substack approach: relative to main camera)
    lightSystem->SetupLightCamera(directionalLightEntity, mainCameraPos);

    // Begin shadow rendering with depth shader
    BeginShaderMode(*depthShader);

    BeginTextureMode(lightSystem->GetShadowMap());
    ClearBackground(WHITE); // Clear to white (far depth) like Raylib example

    // Set up light camera and capture matrices (Substack approach)
    BeginMode3D(lightSystem->GetLightCamera());

    // Capture the matrices that Raylib set up (AFTER BeginMode3D)
    Matrix lightView = rlGetMatrixModelview();
    Matrix lightProj = rlGetMatrixProjection();
    Matrix lightViewProj = MatrixMultiply(lightView, lightProj);

    // Store the matrix for use in final rendering
    lightSystem->SetLightViewProjectionMatrix(lightViewProj);
    LOG_DEBUG("🔆 Captured light VP matrix for shadows");

    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlEnableDepthMask();

    // Render all shadow-casting entities
    for (Entity* entity : shadowCastingEntities) {
        if (!entity || !entity->IsActive()) continue;

        auto transform = entity->GetComponent<TransformComponent>();
        auto mesh = entity->GetComponent<MeshComponent>();
        auto material = entity->GetComponent<MaterialComponent>();

        if (!transform || !mesh || !material) continue;

        // Skip lights themselves from casting shadows
        if (entity->GetComponent<LightComponent>()) continue;

        // Create render command and render it directly
        RenderCommand shadowCommand(entity, transform, nullptr, mesh, nullptr, RenderType::MESH_3D);
        renderer_.DrawMesh3D(shadowCommand);
        LOG_DEBUG("Rendered entity " + std::to_string(entity->GetId()) + " to shadow map");
    }

    EndMode3D();
    EndTextureMode();
    rlEnableColorBlend();

    // End shadow rendering
    EndShaderMode();

    LOG_DEBUG("🌑 Rendered shadow map with captured matrices");
}

