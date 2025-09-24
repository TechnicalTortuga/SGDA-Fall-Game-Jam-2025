#pragma once

#include "raylib.h"
#include "rlgl.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include "../world/WorldGeometry.h" // For World struct
#include "../world/BSPTreeSystem.h"
#include "../math/AABB.h"
#include "../ecs/Components/Collidable.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Components/TransformComponent.h"
#include "../ecs/Components/MaterialComponent.h"
#include "../ecs/Systems/AssetSystem.h"
#include "../core/Engine.h"
#include "Skybox.h"
#include "../ecs/Systems/CacheSystem.h"

class Entity;
class Position;
class Sprite;

// Different types of renderable objects
enum class RenderType {
    SPRITE_2D,      // 2D sprite as billboard in 3D space
    PRIMITIVE_3D,   // 3D primitive (cube, sphere, etc.)
    MESH_3D,        // 3D mesh/model
    WORLD_GEOMETRY, // Static world geometry (BSP, static meshes)
    LIGHT_GIZMO,    // Light source visualization
    DEBUG           // Debug visualization
};

// Note: Primitive types removed - all mesh creation handled by MeshSystem

struct RenderCommand {
    Entity* entity;
    TransformComponent* transform;
    Sprite* sprite;
    MeshComponent* mesh;
    MaterialComponent* material;
    RenderType type;
    float depth; // For sorting (higher = rendered later)

    // Note: Primitive properties removed - all handled by MeshComponent

    RenderCommand(Entity* e, TransformComponent* t, Sprite* s, MeshComponent* m = nullptr, MaterialComponent* mat = nullptr, RenderType rt = RenderType::SPRITE_2D)
        : entity(e), transform(t), sprite(s), mesh(m), material(mat), type(rt), depth(0.0f) {
    }
};

class MeshSystem;

// Forward declarations
class MeshSystem;
class AssetSystem;
class InputSystem;
class Engine;

class Renderer {
public:
    // Instanced rendering data structure
    struct InstanceData {
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
        Color tint;
    };

    Renderer();
    ~Renderer();

    // System access for resource operations
    void SetMeshSystem(MeshSystem* meshSystem) { meshSystem_ = meshSystem; }
    void SetAssetSystem(AssetSystem* assetSystem) { assetSystem_ = assetSystem; }
    // Engine access (singleton)
    Engine& GetEngine() const { return Engine::GetInstance(); }

    // Main rendering methods
    void BeginFrame();
    void EndFrame();
    void Clear(Color color = BLACK);

    // Legacy sprite rendering (now uses dispatcher)
    void DrawSprite(const RenderCommand& command);

    // 2D Sprite rendering (billboards in 3D space)
    void DrawSprite2D(const RenderCommand& command);

    // 3D Primitive rendering
    void DrawPrimitive3D(const RenderCommand& command);

    // 3D Mesh/Model rendering (optimized for ECS)
    void DrawMesh3D(const RenderCommand& command);
    void FlushMeshBatch();

    // Shadow rendering mode
    void BeginShadowMode(Shader& depthShader);
    void EndShadowMode();
    
    // Light gizmo rendering
    void DrawLightGizmo(const RenderCommand& command);

    // Instanced rendering support
    void EnableInstancing(bool enabled) { instancingEnabled_ = enabled; }
    bool IsInstancingEnabled() const { return instancingEnabled_; }
    void AddInstance(const std::string& meshName, const InstanceData& instance);
    void FlushInstances();
    void ClearInstances();

    // Helper methods
    void CreateSimpleCubeMesh(MeshComponent& mesh, const Vector3& position, float size, const Color& color);
    void RenderCompositeMesh(const RenderCommand& command, const MeshComponent& mesh, const Vector3& worldPos, const Vector3& scale);

    // Main render command dispatcher
    void DrawRenderCommand(const RenderCommand& command);

    // Debug rendering
    void DrawDebugInfo();
    void DrawGrid(float spacing = 50.0f, Color color = LIGHTGRAY);

    // Raycasting for hit detection
    bool CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const;
    bool CastRayWorld(const Vector3& origin, const Vector3& direction, float maxDistance,
                     Vector3& hitPoint, Vector3& hitNormal) const;
    bool CastRayEntities(const Vector3& origin, const Vector3& direction, float maxDistance,
                        Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const;

    // Camera/viewport management
    void SetCameraPosition(float x, float y, float z);
    void SetCameraTarget(float x, float y, float z);
    void SetCameraRotation(float rotation);
    void SetCameraZoom(float zoom);
    void UpdateCameraToFollowPlayer(float playerX, float playerY, float playerZ);
    void UpdateCameraRotation(float mouseDeltaX, float mouseDeltaY, float deltaTime);
    void SetCameraAngles(float yaw, float pitch) { yaw_ = yaw; pitch_ = pitch; }
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    Vector3 GetCameraPosition() const { return camera_.position; }
    Vector3 GetCameraTarget() const { return camera_.target; }
    float GetCameraZoom() const { return camera_.fovy; }

    // Screen utilities
    int GetScreenWidth() const { return screenWidth_; }
    int GetScreenHeight() const { return screenHeight_; }
    Vector2 ScreenToWorld(Vector2 screenPos) const;
    Vector2 WorldToScreen(Vector2 worldPos) const;

    // Statistics access
    int GetSpritesRendered() const { return spritesRendered_; }
    int GetFramesRendered() const { return framesRendered_; }
    
    // Model cache management  
    ModelCache* GetModelCache() { return modelCache_.get(); }
    void InvalidateMeshCache(uint64_t meshId); // Implementation will use CacheSystem cleanup

    // World integration
    void SetBSPTree(BSPTree* bspTree) { bspTree_ = bspTree; }
    void SetRenderableEntities(const std::vector<Entity*>& entities) { renderableEntities_ = entities; }
    void SetWorldGeometry(WorldGeometry* worldGeometry) { worldGeometry_ = worldGeometry; }
    void SetWorldRenderingEnabled(bool enabled) { worldRenderingEnabled_ = enabled; }
    BSPTreeSystem* GetBSPTreeSystem() { return bspTreeSystem_.get(); }
    
    // PVS Debug controls
    // Note: Debug controls use the following key mappings (must be set up in InputSystem):
    // - F3: Toggle PVS debug visualization
    // - F4: Toggle showing all clusters
    // - F5: Toggle visibility lines between clusters
    // - F6: Select previous cluster
    // - F7: Select next cluster
    void SetPVSDebugEnabled(bool enabled) { showPVSDebug_ = enabled; }
    bool IsPVSDebugEnabled() const { return showPVSDebug_; }
    void TogglePVSDebug() { showPVSDebug_ = !showPVSDebug_; }
    void SetShowAllClusters(bool show) { showAllClusters_ = show; }
    bool IsShowingAllClusters() const { return showAllClusters_; }
    void SetShowVisibilityLines(bool show) { showVisibilityLines_ = show; }
    bool IsShowingVisibilityLines() const { return showVisibilityLines_; }
    void SetSelectedCluster(int clusterId) { selectedCluster_ = clusterId; }
    int GetSelectedCluster() const { return selectedCluster_; }
    
    // Set the input system for debug controls
    // Call this during initialization to enable debug input handling
    void SetInputSystem(InputSystem* inputSystem) { inputSystem_ = inputSystem; }
    
    // Handle debug input for PVS visualization
    void HandleDebugInput();

    // PVS Debug visualization methods
    void DebugDrawClusters(bool showAllClusters, bool showVisibilityLines) const;
    void DebugDrawClusterPVS(int32_t clusterId) const;
    void DebugDrawAllClusterBounds() const;

    // World geometry rendering
    void RenderWorldGeometry();
    WorldGeometry* GetWorldGeometry() const { return worldGeometry_; }
    bool IsWorldRenderingEnabled() const { return worldRenderingEnabled_; }
    const Camera3D& GetCamera() const { return camera_; }
    
    // Shader management for BSP geometry
    void SetCurrentShader(const Shader& shader);
    void ClearCurrentShader();

    // Culling methods
    bool IsEntityVisible(const Vector3& position, float boundingRadius = 1.0f) const;
    void SetFrustumCullingEnabled(bool enabled) { enableFrustumCulling_ = enabled; }
    bool IsFrustumCullingEnabled() const { return enableFrustumCulling_; }
    void SetFarClipDistance(float distance) { farClipDistance_ = distance; }
    float GetFarClipDistance() const { return farClipDistance_; }
    
    // Culling statistics
    struct CullingStats {
        int totalEntitiesChecked = 0;
        int entitiesCulledByDistance = 0;
        int entitiesCulledByFrustum = 0;
        int entitiesVisible = 0;
        
        void Reset() {
            totalEntitiesChecked = entitiesCulledByDistance = entitiesCulledByFrustum = entitiesVisible = 0;
        }
        
        float GetCullRate() const {
            return totalEntitiesChecked > 0 ? 
                (float)(entitiesCulledByDistance + entitiesCulledByFrustum) / totalEntitiesChecked : 0.0f;
        }
    };
    
    const CullingStats& GetCullingStats() const { return cullingStats_; }
    void ResetCullingStats() { cullingStats_.Reset(); }

private:
    // World rendering helper methods
    void RenderBSPGeometry();
    void RenderSkybox();
    void SetupMaterial(const MaterialComponent& material);
    void RenderFace(const Face& face);
    bool IsFaceVisibleForRendering(const Face& face, const Camera3D& camera) const;
    bool IsPointInViewFrustum(const Vector3& point, const Camera3D& camera) const;
    bool IsAABBInViewFrustum(const AABB& box, const Camera3D& camera) const;
    
    // PVS Debug rendering
    void RenderPVSDebug();
    Camera3D camera_;
    int screenWidth_;
    int screenHeight_;

    // System references
    MeshSystem* meshSystem_;
    AssetSystem* assetSystem_;
    InputSystem* inputSystem_;
    
    // Asset caching
    // Model caching using CacheSystem (types defined in CacheSystem.h)
    std::unique_ptr<ModelCache> modelCache_;
    
    // Additional missing member variables (some may be duplicates)
    WorldGeometry* worldGeometry_;
    bool worldRenderingEnabled_;
    
    // World rendering state (moved from WorldRenderer)
    bool bspRenderingEnabled_;
    bool skyboxRenderingEnabled_;
    bool wireframeMode_;
    int surfacesRendered_;
    int trianglesRendered_;

    // Shadow rendering state
    Shader* shadowShader_;
    bool inShadowMode_;

    // Pre-allocated containers to avoid per-frame allocations (major performance optimization)
    std::vector<const Face*> visibleFaces_;
    std::unordered_map<unsigned int, std::vector<const Face*>> facesByMaterial_;

    // Optimized mesh rendering buffers (ECS-friendly)
    std::vector<float> vertexBuffer_;
    std::vector<unsigned int> indexBuffer_;
    int currentTextureId_;
    bool batchInProgress_;

    // Instanced rendering support
    std::vector<InstanceData> instanceBuffer_;
    std::unordered_map<std::string, std::vector<InstanceData>> instanceGroups_; // mesh name -> instances
    bool instancingEnabled_;

    // Camera rotation angles (standard FPS terminology)
    float yaw_;              // Horizontal rotation (left/right around Y axis)
    float pitch_;            // Vertical rotation (up/down from horizontal plane)
    float mouseSensitivity_;

    // World raycasting
    BSPTree* bspTree_;
    std::unique_ptr<BSPTreeSystem> bspTreeSystem_;
    std::vector<Entity*> renderableEntities_;


    // ECS access

    // Helper functions for camera calculations
    Vector3 SphericalToCartesian(float yaw, float pitch, float radius) const;
    void UpdateCameraFromAngles();

    // Render statistics
    int spritesRendered_;
    int framesRendered_;

    // UV mapping mode
    bool useStretchUV_; // true = stretch (0-1), false = wrap (calculated from geometry)

    // Texture binding state
    int lastBoundTexture_;

    // Culling settings
    bool enableFrustumCulling_;
    float farClipDistance_;
    mutable CullingStats cullingStats_;
    
    // PVS Debug visualization
    bool showPVSDebug_ = false;
    bool showAllClusters_ = false;
    bool showVisibilityLines_ = false;
    int selectedCluster_ = -1;

    // World geometry material caching to avoid expensive entity lookups
    mutable std::unordered_map<uint64_t, MaterialComponent*> materialCache_;
    
    // Current shader for BSP geometry lighting
    Shader* currentShader_;
    bool hasCurrentShader_;
    
    void UpdateScreenSize();
    void SetUVStretchMode(bool stretch) { useStretchUV_ = stretch; }
    bool IsUVStretchMode() const { return useStretchUV_; }

};
