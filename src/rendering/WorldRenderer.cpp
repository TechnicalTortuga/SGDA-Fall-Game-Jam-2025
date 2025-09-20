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

void WorldRenderer::RenderFace(const Face& face) {

    // Get material for this face
    const WorldMaterial* material = worldGeometry_->GetMaterial(face.materialId);
    if (!material) {
        LOG_WARNING("No material found for materialId: " + std::to_string(face.materialId));
        material = worldGeometry_->GetMaterial(0); // Fallback to first material
        if (!material) return; // Still no material, can't render
    }

    // Setup material (binds texture if available)
    SetupMaterial(*material);
    
    // Apply tint logic: for textured surfaces, show texture without tinting unless specifically requested
    if (material->hasTexture) {
        // For textured surfaces, default to showing texture as-is (white)
        // Only apply tint if explicitly needed for special effects
        rlColor4ub(255, 255, 255, 255); // Show texture as-is
    } else {
        // For non-textured surfaces, use the tint as the base color
        rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
    }

    // Calculate face dimensions for texture scaling
    Vector3 edge1 = Vector3Subtract(face.vertices[1], face.vertices[0]);
    Vector3 edge2 = Vector3Subtract(face.vertices[2], face.vertices[1]);
    Vector3 faceNormal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
    
    // Determine if this is a floor/ceiling (Y-normal) or wall (XZ-normal)
    bool isHorizontal = fabsf(faceNormal.y) > 0.9f;
    
    // Get face bounds for texture scaling
    Vector3 minVert = face.vertices[0];
    Vector3 maxVert = face.vertices[0];
    for (const auto& v : face.vertices) {
        minVert = Vector3Min(minVert, v);
        maxVert = Vector3Max(maxVert, v);
    }
    
    // Calculate texture scale based on face size
    float texScale = 1.0f; // Default scale
    if (isHorizontal) {
        // For floors/ceilings, scale based on X/Z dimensions
        float sizeX = maxVert.x - minVert.x;
        float sizeZ = maxVert.z - minVert.z;
        texScale = 1.0f / fmaxf(1.0f, fmaxf(sizeX, sizeZ) / 4.0f);
    } else {
        // For walls, scale based on height
        float height = maxVert.y - minVert.y;
        texScale = 1.0f / fmaxf(1.0f, height / 4.0f);
    }

    // Decide primitive: quads or triangles
    size_t vcount = face.vertices.size();
    if (vcount == 4) {
        // Optimized quad rendering with proper UVs
        rlBegin(RL_QUADS);
            // Calculate UVs based on vertex positions
            for (size_t i = 0; i < 4; i++) {
                float u, v;
                if (isHorizontal) {
                    // For floors/ceilings, use X/Z coordinates for UVs
                    u = (face.vertices[i].x - minVert.x) * texScale;
                    v = (face.vertices[i].z - minVert.z) * texScale;
                } else {
                    // For walls, use X or Z (whichever changes most) and Y for UVs
                    if (fabsf(faceNormal.x) > fabsf(faceNormal.z)) {
                        // Wall facing mostly X direction, use Z and Y for UVs
                        u = (face.vertices[i].z - minVert.z) * texScale;
                        v = (face.vertices[i].y - minVert.y) * texScale;
                    } else {
                        // Wall facing mostly Z direction, use X and Y for UVs
                        u = (face.vertices[i].x - minVert.x) * texScale;
                        v = (face.vertices[i].y - minVert.y) * texScale;
                    }
                }
                rlTexCoord2f(u, v);
                rlVertex3f(face.vertices[i].x, face.vertices[i].y, face.vertices[i].z);
            }
        rlEnd();
        trianglesRendered_ += 2;
    } else if (vcount >= 3) {
        // Triangle fan for non-quad faces
        rlBegin(RL_TRIANGLES);
            for (size_t i = 1; i + 1 < vcount; ++i) {
                // For each triangle in the fan (0, i, i+1)
                for (size_t j = 0; j < 3; j++) {
                    size_t idx = (j == 0) ? 0 : (j == 1 ? i : i + 1);
                    float u, v;
                    if (isHorizontal) {
                        u = (face.vertices[idx].x - minVert.x) * texScale;
                        v = (face.vertices[idx].z - minVert.z) * texScale;
                    } else {
                        if (fabsf(faceNormal.x) > fabsf(faceNormal.z)) {
                            u = (face.vertices[idx].z - minVert.z) * texScale;
                            v = (face.vertices[idx].y - minVert.y) * texScale;
                        } else {
                            u = (face.vertices[idx].x - minVert.x) * texScale;
                            v = (face.vertices[idx].y - minVert.y) * texScale;
                        }
                    }
                    rlTexCoord2f(u, v);
                    rlVertex3f(face.vertices[idx].x, face.vertices[idx].y, face.vertices[idx].z);
                }
                trianglesRendered_++;
            }
        rlEnd();
    }
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
        LOG_INFO("BSP: collecting visible faces");
    }

    // If we have prebuilt batches, render them directly
    const auto& batches = worldGeometry_->GetBatches();
    if (!batches.empty()) {
        if (bspRenderCount % 60 == 0) {
            LOG_INFO("BSP: rendering batched geometry (" + std::to_string(batches.size()) + " batches)");
        }
        if (wireframeMode_) rlEnableWireMode();
        RenderBatches();
        if (wireframeMode_) rlDisableWireMode();
        rlSetTexture(0);
        
        return;
    }

    // Otherwise fall back to face traversal and immediate drawing
    std::vector<const Face*> visibleFaces = worldGeometry_->GetVisibleFaces(camera);
    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: found " + std::to_string(visibleFaces.size()) + " visible faces");
    }

    // Set wireframe mode if enabled
    if (wireframeMode_) {
        rlEnableWireMode();
    }

    // Render faces
    if (bspRenderCount % 60 == 0) {
        LOG_INFO("BSP: rendering " + std::to_string(visibleFaces.size()) + " faces");
    }
    for (const Face* face : visibleFaces) {
        if (face) {
            RenderFace(*face);
            surfacesRendered_++;
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

// Removed RenderSurface: migrating to face-only pipeline

void WorldRenderer::SetupMaterial(const WorldMaterial& material) {
    static int lastBoundTexture = -1;
    
    // Don't set color here - let the face tint be applied by the caller
    
    // Bind texture if available
    if (material.hasTexture && material.texture.id != 0) {
        // Only rebind if it's a different texture to reduce state changes
        if (lastBoundTexture != material.texture.id) {
            rlSetTexture(material.texture.id);
            lastBoundTexture = material.texture.id;

            // Set texture parameters for better quality
            SetTextureFilter(material.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(material.texture, TEXTURE_WRAP_REPEAT);
        }
    } else {
        // No texture, use default white texture
        if (lastBoundTexture != 0) {
            rlSetTexture(0);
            lastBoundTexture = 0;

            static bool loggedWarning = false;
            if (!loggedWarning) {
                LOG_WARNING("Rendering with no texture (material.hasTexture=" +
                           std::to_string(material.hasTexture) + ")");
                loggedWarning = true; // Only log once to avoid spam
            }
        }
    }
    
    // Set material properties for lighting (if using shaders)
    // This is a placeholder for when we implement proper lighting
    static bool shaderWarningLogged = false;
    if (!shaderWarningLogged) {
        LOG_DEBUG("Material shininess: " + std::to_string(material.shininess));
        shaderWarningLogged = true;
    }
}

// Removed IsSurfaceVisible: migrating to face-only pipeline

void WorldRenderer::RenderBatches() {
    const auto& batches = worldGeometry_->GetBatches();
    static int dbgCount = 0;
    for (const auto& batch : batches) {
        const WorldMaterial* mat = worldGeometry_->GetMaterial(batch.materialId);
        if (mat) {
            SetupMaterial(*mat);
            if (dbgCount < 10 || batch.materialId == 0) { // Always log first 10 batches and material 0
                LOG_INFO("RenderBatches: materialId=" + std::to_string(batch.materialId) +
                         " texId=" + std::to_string(mat->texture.id) +
                         " tris=" + std::to_string(batch.indices.size()/3) +
                         " hasTex=" + std::to_string(mat->hasTexture) +
                         " texWidth=" + std::to_string(mat->texture.width) +
                         " texHeight=" + std::to_string(mat->texture.height));
            }
        } else {
            rlSetTexture(0);
            if ((dbgCount % 120) == 0) {
                LOG_WARNING("RenderBatches: no material for id " + std::to_string(batch.materialId));
            }
        }

        // Render using quads and triangles (original approach)
        size_t vertexIndex = 0;
        while (vertexIndex < batch.positions.size()) {
            // Check if we have at least 3 vertices for a triangle
            if (vertexIndex + 2 >= batch.positions.size()) break;

            // For quads (4 vertices in sequence)
            if (vertexIndex + 3 < batch.positions.size()) {
                // Render as quad
                rlBegin(RL_QUADS);
                    rlColor4ub(batch.colors[vertexIndex].r, batch.colors[vertexIndex].g, batch.colors[vertexIndex].b, batch.colors[vertexIndex].a);
                    rlTexCoord2f(batch.uvs[vertexIndex].x, batch.uvs[vertexIndex].y);
                    rlVertex3f(batch.positions[vertexIndex].x, batch.positions[vertexIndex].y, batch.positions[vertexIndex].z);

                    rlColor4ub(batch.colors[vertexIndex+1].r, batch.colors[vertexIndex+1].g, batch.colors[vertexIndex+1].b, batch.colors[vertexIndex+1].a);
                    rlTexCoord2f(batch.uvs[vertexIndex+1].x, batch.uvs[vertexIndex+1].y);
                    rlVertex3f(batch.positions[vertexIndex+1].x, batch.positions[vertexIndex+1].y, batch.positions[vertexIndex+1].z);

                    rlColor4ub(batch.colors[vertexIndex+2].r, batch.colors[vertexIndex+2].g, batch.colors[vertexIndex+2].b, batch.colors[vertexIndex+2].a);
                    rlTexCoord2f(batch.uvs[vertexIndex+2].x, batch.uvs[vertexIndex+2].y);
                    rlVertex3f(batch.positions[vertexIndex+2].x, batch.positions[vertexIndex+2].y, batch.positions[vertexIndex+2].z);

                    rlColor4ub(batch.colors[vertexIndex+3].r, batch.colors[vertexIndex+3].g, batch.colors[vertexIndex+3].b, batch.colors[vertexIndex+3].a);
                    rlTexCoord2f(batch.uvs[vertexIndex+3].x, batch.uvs[vertexIndex+3].y);
                    rlVertex3f(batch.positions[vertexIndex+3].x, batch.positions[vertexIndex+3].y, batch.positions[vertexIndex+3].z);
                rlEnd();
                trianglesRendered_ += 2;
                vertexIndex += 4;
                continue;
            }

            // For triangles (3 vertices) - render as triangle
            if (vertexIndex + 2 < batch.positions.size()) {
                rlBegin(RL_TRIANGLES);
                    rlColor4ub(batch.colors[vertexIndex].r, batch.colors[vertexIndex].g, batch.colors[vertexIndex].b, batch.colors[vertexIndex].a);
                    rlTexCoord2f(batch.uvs[vertexIndex].x, batch.uvs[vertexIndex].y);
                    rlVertex3f(batch.positions[vertexIndex].x, batch.positions[vertexIndex].y, batch.positions[vertexIndex].z);

                    rlColor4ub(batch.colors[vertexIndex+1].r, batch.colors[vertexIndex+1].g, batch.colors[vertexIndex+1].b, batch.colors[vertexIndex+1].a);
                    rlTexCoord2f(batch.uvs[vertexIndex+1].x, batch.uvs[vertexIndex+1].y);
                    rlVertex3f(batch.positions[vertexIndex+1].x, batch.positions[vertexIndex+1].y, batch.positions[vertexIndex+1].z);

                    rlColor4ub(batch.colors[vertexIndex+2].r, batch.colors[vertexIndex+2].g, batch.colors[vertexIndex+2].b, batch.colors[vertexIndex+2].a);
                    rlTexCoord2f(batch.uvs[vertexIndex+2].x, batch.uvs[vertexIndex+2].y);
                    rlVertex3f(batch.positions[vertexIndex+2].x, batch.positions[vertexIndex+2].y, batch.positions[vertexIndex+2].z);
                rlEnd();
                trianglesRendered_ += 1;
                vertexIndex += 3;
            } else {
                break; // Not enough vertices for a triangle
            }
        }
    }
    dbgCount++;
}
