#pragma once

#include "raylib.h"
#include <memory>
#include "../world/WorldGeometry.h"

// Forward declaration
class Camera3D;

// WorldRenderer - Dedicated renderer for static world geometry
// This class handles rendering of all static world elements (BSP geometry,
// static meshes, skybox) that are stored in WorldGeometry.
// It is completely separate from dynamic entity rendering.
class WorldRenderer {
public:
    WorldRenderer();
    ~WorldRenderer() = default;

    // Initialize with WorldGeometry reference
    void SetWorldGeometry(WorldGeometry* worldGeometry);

    // Main rendering method
    void Render(const Camera3D& camera);

    // Enable/disable rendering features
    void SetBSPRenderingEnabled(bool enabled) { bspRenderingEnabled_ = enabled; }
    void SetSkyboxRenderingEnabled(bool enabled) { skyboxRenderingEnabled_ = enabled; }
    void SetWireframeMode(bool enabled) { wireframeMode_ = enabled; }

    // Debug information
    int GetSurfacesRendered() const { return surfacesRendered_; }
    int GetTrianglesRendered() const { return trianglesRendered_; }

private:
    WorldGeometry* worldGeometry_;  // Reference to static world data

    // Rendering options
    bool bspRenderingEnabled_;
    bool skyboxRenderingEnabled_;
    bool wireframeMode_;

    // Statistics
    int surfacesRendered_;
    int trianglesRendered_;

    // Internal rendering methods
    void RenderBSPGeometry(const Camera3D& camera);
    void RenderSkybox(const Camera3D& camera);
    void RenderFace(const Face& face);
    void RenderBatches();
    void SetupMaterial(const WorldMaterial& material);

    // Visibility culling
    bool IsFaceVisible(const Face& face, const Camera3D& camera) const;
};
