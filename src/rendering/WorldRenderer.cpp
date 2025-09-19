#include "WorldRenderer.h"
#include <algorithm>
#include "../utils/Logger.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

WorldRenderer::WorldRenderer()
    : worldGeometry_(nullptr)
    , bspRenderingEnabled_(true)
    , skyboxRenderingEnabled_(true)
    , wireframeMode_(false)
    , surfacesRendered_(0)
    , trianglesRendered_(0)
{
}

void WorldRenderer::SetWorldGeometry(WorldGeometry* worldGeometry) {
    worldGeometry_ = worldGeometry;
    LOG_INFO("WorldRenderer initialized with WorldGeometry");
}

void WorldRenderer::Render(const Camera3D& camera) {
    static int renderCount = 0;
    renderCount++;

    if (renderCount % 60 == 0) {
        LOG_INFO("WorldRenderer::Render (frame " + std::to_string(renderCount) + ")");
    }

    if (!worldGeometry_) {
        LOG_ERROR("WorldGeometry is null!");
        return;
    }
    if (!worldGeometry_->IsValid()) {
        if (renderCount % 60 == 0) {
            LOG_WARNING("WorldGeometry is not valid (no BSP tree)");
        }
        // Continue anyway to render skybox if possible
    }

    // Reset statistics
    surfacesRendered_ = 0;
    trianglesRendered_ = 0;

    // Ensure all static world rendering occurs within a valid 3D context
    BeginMode3D(camera);

    // Render skybox first (background)
    if (skyboxRenderingEnabled_) {
        RenderSkybox(camera);
    }

    // Render BSP geometry
    if (bspRenderingEnabled_) {
        RenderBSPGeometry(camera);
    }

    EndMode3D();
}

void WorldRenderer::RenderBSPGeometry(const Camera3D& camera) {
    static int bspRenderCount = 0;
    bspRenderCount++;

    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: checking tree state");
    }

    if (!worldGeometry_) {
        LOG_ERROR("WorldGeometry is null");
        return;
    }
    if (!worldGeometry_->bspTree) {
        LOG_ERROR("BSP tree is null");
        return;
    }

    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: collecting visible surfaces");
    }

    // Get visible surfaces from BSP tree
    std::vector<const Surface*> visibleSurfaces = worldGeometry_->GetVisibleSurfaces(camera.position);

    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: found " + std::to_string(visibleSurfaces.size()) + " visible surfaces");
    }

    // Set wireframe mode if enabled
    if (wireframeMode_) {
        rlEnableWireMode();
    }

    // Render each visible surface
    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: rendering " + std::to_string(visibleSurfaces.size()) + " surfaces");
    }

    for (const Surface* surface : visibleSurfaces) {
        if (surface) {
            if (IsSurfaceVisible(*surface, camera)) {
                RenderSurface(*surface);
                surfacesRendered_++;
            }
        } else {
            if (bspRenderCount % 60 == 0) {
                LOG_WARNING("Null surface pointer");
            }
        }
    }

    // Unbind texture to leave clean state
    rlSetTexture(0);

    // Reset wireframe mode
    if (wireframeMode_) {
        rlDisableWireMode();
    }
}

void WorldRenderer::RenderSkybox(const Camera3D& camera) {
    static int skyboxRenderCount = 0;
    skyboxRenderCount++;

    if (skyboxRenderCount % 60 == 0) {
        LOG_INFO("Skybox: rendering");
    }

    if (worldGeometry_->skybox) {
        if (worldGeometry_->skybox->IsLoaded()) {
            worldGeometry_->skybox->Render(camera);
            if (skyboxRenderCount % 60 == 0) {
                LOG_INFO("Skybox: rendered via Skybox class");
            }
        } else {
            if (skyboxRenderCount % 60 == 0) {
                LOG_WARNING("Skybox exists but is not loaded");
            }
            ClearBackground(worldGeometry_->GetSkyColor());
        }
    } else {
        if (skyboxRenderCount % 60 == 0) {
            LOG_WARNING("Skybox object is null");
        }
        ClearBackground(worldGeometry_->GetSkyColor());
    }
}

void WorldRenderer::RenderSurface(const Surface& surface) {
    // Setup material for this surface
    const WorldMaterial* material = worldGeometry_->GetMaterial(surface.textureIndex);
    if (material) {
        SetupMaterial(*material);
    }

    // Draw the surface based on its type (with optional texturing)
    if (surface.isWall) {
        // Draw wall as a quad
        Vector3 normal = surface.GetNormal();

        // Calculate wall corners
        // Convention: start = bottom-left (y = floor), end = top-right (y = ceiling)
        // Mix x/z from start/end with y from start/end to get all 4 corners
        Vector3 bottomLeft = {surface.start.x,  surface.start.y, surface.start.z};
        Vector3 bottomRight= {surface.end.x,    surface.start.y, surface.end.z};
        Vector3 topLeft    = {surface.start.x,  surface.end.y,   surface.start.z};
        Vector3 topRight   = {surface.end.x,    surface.end.y,   surface.end.z};

        // UVs: u along wall length, v along height
        float length = Vector3Length(Vector3Subtract(bottomRight, bottomLeft));
        const float uvScale = 0.25f; // tiles per world unit (adjust as desired)
        float uMax = length * uvScale;
        float wallHeight = fabsf(surface.end.y - surface.start.y);
        float vMax = wallHeight * uvScale;

        rlBegin(RL_QUADS);
            rlColor4ub(surface.color.r, surface.color.g, surface.color.b, surface.color.a);

            // Bottom-left (u=0, v=vMax)
            rlTexCoord2f(0.0f, vMax);
            rlVertex3f(bottomLeft.x, bottomLeft.y, bottomLeft.z);
            // Bottom-right (u=uMax, v=vMax)
            rlTexCoord2f(uMax, vMax);
            rlVertex3f(bottomRight.x, bottomRight.y, bottomRight.z);
            // Top-right (u=uMax, v=0)
            rlTexCoord2f(uMax, 0.0f);
            rlVertex3f(topRight.x, topRight.y, topRight.z);
            // Top-left (u=0, v=0)
            rlTexCoord2f(0.0f, 0.0f);
            rlVertex3f(topLeft.x, topLeft.y, topLeft.z);
        rlEnd();

        trianglesRendered_ += 2; // Quad = 2 triangles

    } else if (surface.isFloor) {
        // Draw floor as a quad
        Vector3 p1 = surface.start;
        Vector3 p2 = {surface.end.x, surface.floorHeight, surface.start.z};
        Vector3 p3 = {surface.end.x, surface.floorHeight, surface.end.z};
        Vector3 p4 = {surface.start.x, surface.floorHeight, surface.end.z};

        // UVs from X/Z extents
        float lenX = fabsf(p3.x - p4.x);
        float lenZ = fabsf(p3.z - p2.z);
        const float uvScale = 0.25f;
        float uMax = lenX * uvScale;
        float vMax = lenZ * uvScale;

        rlBegin(RL_QUADS);
            rlColor4ub(surface.color.r, surface.color.g, surface.color.b, surface.color.a);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(p1.x, p1.y, p1.z);
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(p2.x, p2.y, p2.z);
            rlTexCoord2f(uMax, vMax); rlVertex3f(p3.x, p3.y, p3.z);
            rlTexCoord2f(0.0f, vMax); rlVertex3f(p4.x, p4.y, p4.z);
        rlEnd();

        trianglesRendered_ += 2;

    } else if (surface.isCeiling) {
        // Draw ceiling as a quad
        Vector3 p1 = {surface.start.x, surface.ceilingHeight, surface.start.z};
        Vector3 p2 = {surface.end.x, surface.ceilingHeight, surface.start.z};
        Vector3 p3 = {surface.end.x, surface.ceilingHeight, surface.end.z};
        Vector3 p4 = {surface.start.x, surface.ceilingHeight, surface.end.z};

        float lenX = fabsf(p3.x - p4.x);
        float lenZ = fabsf(p3.z - p2.z);
        const float uvScale = 0.25f;
        float uMax = lenX * uvScale;
        float vMax = lenZ * uvScale;

        rlBegin(RL_QUADS);
            rlColor4ub(surface.color.r, surface.color.g, surface.color.b, surface.color.a);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(p1.x, p1.y, p1.z);
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(p2.x, p2.y, p2.z);
            rlTexCoord2f(uMax, vMax); rlVertex3f(p3.x, p3.y, p3.z);
            rlTexCoord2f(0.0f, vMax); rlVertex3f(p4.x, p4.y, p4.z);
        rlEnd();

        trianglesRendered_ += 2;
    }
}

void WorldRenderer::SetupMaterial(const WorldMaterial& material) {
    // Set diffuse color
    rlColor4ub(material.diffuseColor.r, material.diffuseColor.g,
               material.diffuseColor.b, material.diffuseColor.a);

    // Bind texture if available
    if (material.hasTexture && material.texture.id != 0) {
        rlSetTexture(material.texture.id);
    } else {
        rlSetTexture(0);
    }
}

bool WorldRenderer::IsSurfaceVisible(const Surface& surface, const Camera3D& camera) const {
    // Simple frustum culling - check if surface is in front of camera
    Vector3 toSurface = Vector3Subtract(surface.start, camera.position);
    float distance = Vector3Length(toSurface);

    // Basic distance culling (surfaces too far away)
    if (distance > 1000.0f) {
        return false;
    }

    // Check if surface normal is facing camera (backface culling)
    Vector3 normal = surface.GetNormal();
    Vector3 viewDir = Vector3Normalize(toSurface);
    float dotProduct = Vector3DotProduct(normal, viewDir);

    // If surface is facing away from camera, don't render it
    return dotProduct < 0.0f;
}
