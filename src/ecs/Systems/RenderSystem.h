#pragma once

#include "../System.h"
#include "../../rendering/Renderer.h"
#include "../Components/Velocity.h"
#include "../Systems/MeshSystem.h"
#include "../Systems/AssetSystem.h"
#include "../../world/BSPTree.h"
#include <vector>

// Forward declarations for lighting integration
class LightSystem;
class ShaderSystem;

class RenderSystem : public System {
public:
    RenderSystem();
    ~RenderSystem();

    void Initialize() override;
    void Update(float deltaTime) override;  // Called during update phase - does nothing for rendering
    void Render() override;  // Called during render phase - does the actual rendering

    // Renderer access
    Renderer* GetRenderer() { return &renderer_; }
    const Renderer* GetRenderer() const { return &renderer_; }

    // Rendering options
    void SetDebugRendering(bool enabled) { debugRendering_ = enabled; }
    bool IsDebugRenderingEnabled() const { return debugRendering_; }

    // Debug info - deprecated, now uses camera state from Renderer
    void SetPlayerPosition(float x, float y, float z) {
        // No longer needed - camera state comes from Renderer
        (void)x; (void)y; (void)z; // Suppress unused parameter warnings
    }
    
    // Material batching statistics
    struct BatchingStats {
        int totalCommands = 0;
        int materialBatches = 0;
        int stateChanges = 0;
        int totalBatches = 0;
        float averageBatchSize = 0.0f;
        
        void Reset() {
            totalCommands = materialBatches = stateChanges = totalBatches = 0;
            averageBatchSize = 0.0f;
        }
        
        float GetBatchingEfficiency() const {
            return totalCommands > 0 ? (float)materialBatches / totalCommands : 0.0f;
        }
    };
    
    const BatchingStats& GetBatchingStats() const { return batchingStats_; }

    void SetGridEnabled(bool enabled) { gridEnabled_ = enabled; }
    bool IsGridEnabled() const { return gridEnabled_; }

    // BSP integration for visibility culling
    void SetBSPTree(BSPTree* bspTree) { bspTree_ = bspTree; }
    BSPTree* GetBSPTree() const { return bspTree_; }

    // Visibility culling control
    void SetVisibilityCullingEnabled(bool enabled) { visibilityCullingEnabled_ = enabled; }
    bool IsVisibilityCullingEnabled() const { return visibilityCullingEnabled_; }

    // Get all entities that should be rendered (for shadow mapping)
    std::vector<Entity*> GetAllEntitiesForRendering();

    // Shadow rendering support
    void RenderShadowsToTexture(const std::vector<Entity*>& shadowCastingEntities);

private:
    Renderer renderer_;
    MeshSystem* meshSystem_;
    AssetSystem* assetSystem_;
    BSPTree* bspTree_;
    bool debugRendering_;
    bool gridEnabled_;
    bool visibilityCullingEnabled_;

    // Batching statistics
    BatchingStats batchingStats_;

    // Camera for visibility culling (synchronized with main render camera)
    Camera3D cullingCamera_;

    std::vector<RenderCommand> renderCommands_;

    void CollectRenderCommands();
    void CollectWorldGeometryCommands();
    void RenderWorldGeometryDirect();
    void SortRenderCommands();
    void ExecuteRenderCommands();

    void BeginShadowMode(Shader& depthShader);
    void EndShadowMode();
};
