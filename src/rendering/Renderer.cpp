#include "Renderer.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Position.h"
#include "../ecs/Components/Sprite.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Components/LightComponent.h"
#include "../ecs/Systems/InputSystem.h"
#include "../ecs/Components/TransformComponent.h"
#include "../ecs/Systems/MeshSystem.h"
#include "../ecs/Systems/AssetSystem.h"
#include "utils/Logger.h"
#include <algorithm>
#include <string>

Renderer::Renderer()
    : screenWidth_(800), screenHeight_(600),
      meshSystem_(nullptr), assetSystem_(nullptr), modelCache_(std::make_unique<ModelCache>(
          ModelCacheFactory::GenerateKey, ModelCacheFactory::CreateModelData, "ModelCache")),
      worldGeometry_(nullptr), worldRenderingEnabled_(true),
      bspRenderingEnabled_(true), skyboxRenderingEnabled_(true), wireframeMode_(false),
      surfacesRendered_(0), trianglesRendered_(0),
      shadowShader_(nullptr), inShadowMode_(false),
      vertexBuffer_(), indexBuffer_(), currentTextureId_(0), batchInProgress_(false),
      instanceBuffer_(), instanceGroups_(), instancingEnabled_(true),
      spritesRendered_(0), framesRendered_(0),
      yaw_(0.0f), pitch_(0.0f), mouseSensitivity_(0.15f),
      bspTree_(nullptr), bspTreeSystem_(std::make_unique<BSPTreeSystem>()), useStretchUV_(true), lastBoundTexture_(-1),
      enableFrustumCulling_(true), farClipDistance_(100.0f),
      camera_({0}),
      showPVSDebug_(false),
      showAllClusters_(false),
      showVisibilityLines_(false),
      selectedCluster_(-1),
      inputSystem_(nullptr),
      currentShader_(nullptr),
      hasCurrentShader_(false)
{
    UpdateScreenSize();

    // Initialize Raylib Camera3D with default values
    // Camera will be positioned properly when UpdateCameraToFollowPlayer is called
    camera_.position = {0.0f, 5.0f, 10.0f};   // Initial camera position
    camera_.target = {0.0f, 0.0f, 0.0f};      // Initial target (origin)
    camera_.up = {0.0f, 1.0f, 0.0f};          // Up vector
    camera_.fovy = 45.0f;                      // Field of view
    camera_.projection = CAMERA_PERSPECTIVE;   // Perspective projection

    LOG_INFO("Renderer initialized with Camera3D");
}

Renderer::~Renderer()
{
    // Asset cache will automatically log its final statistics in its destructor
    modelCache_.reset();
    LOG_INFO("Renderer destroyed");
}

void Renderer::BeginFrame()
{
    spritesRendered_ = 0;
    cullingStats_.Reset(); // Reset culling stats for this frame
    UpdateScreenSize();

    // Begin camera mode for 3D rendering
    BeginMode3D(camera_);
}

void Renderer::EndFrame()
{
    // Flush any remaining batched mesh data
    FlushMeshBatch();

    // End camera mode
    EndMode3D();

    framesRendered_++;
}

// PVS Debug rendering
void Renderer::RenderPVSDebug() {
    if (!showPVSDebug_ || !worldGeometry_ || !worldGeometry_->bspTree) return;
    
    // Set up for world rendering
    // Raylib handles the OpenGL state management internally
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    
    // Draw PVS debug visualization
    if (selectedCluster_ >= 0) {
        // Show PVS for selected cluster
        DebugDrawClusterPVS(selectedCluster_);
    } else {
        // Show all clusters with visibility
        DebugDrawClusters(showAllClusters_, showVisibilityLines_);
    }

    // Draw cluster bounds if enabled
    if (showAllClusters_) {
        DebugDrawAllClusterBounds();
    }

    // Restore OpenGL state to match what RenderBSPGeometry expects
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
}

// World geometry rendering (moved from WorldRenderer)
void Renderer::RenderWorldGeometry()
{
    if (!worldGeometry_ || !worldRenderingEnabled_) {
        return;
    }

    surfacesRendered_ = 0;
    trianglesRendered_ = 0;

    LOG_DEBUG("WorldGeometry has " + std::to_string(worldGeometry_->faces.size()) + " faces");

    // Set wireframe mode if enabled
    if (wireframeMode_) {
        rlEnableWireMode();
    }

    // Render skybox first (never culled)
    RenderSkybox();

    // Render BSP geometry if available (using new World system)
    if (worldGeometry_->GetWorld()) {
        RenderBSPGeometry();
    }

    // Render PVS debug visualization if enabled
    if (showPVSDebug_ && worldGeometry_->GetWorld()) {
        RenderPVSDebug();
    }

    // Reset wireframe mode
    if (wireframeMode_) {
        rlDisableWireMode();
    }

    LOG_DEBUG("World geometry rendered - Surfaces: " + std::to_string(surfacesRendered_) +
              ", Triangles: " + std::to_string(trianglesRendered_));
}

// Handle debug input for PVS visualization
void Renderer::HandleDebugInput() {
    if (!inputSystem_) return;
    
    // Toggle PVS debug with F3
    if (inputSystem_->IsActionPressed(InputAction::CUSTOM_START)) {  // F3
        TogglePVSDebug();
        if (showPVSDebug_) {
            LOG_INFO("PVS Debug: " + std::string(showPVSDebug_ ? "ENABLED" : "DISABLED"));
        }
    }
    
    if (showPVSDebug_) {
        // Toggle all clusters with F4
        if (inputSystem_->IsActionPressed(static_cast<InputAction>(
                static_cast<int>(InputAction::CUSTOM_START) + 1))) {  // F4
            showAllClusters_ = !showAllClusters_;
            LOG_INFO("Show All Clusters: " + std::string(showAllClusters_ ? "ON" : "OFF"));
        }
        
        // Toggle visibility lines with F5
        if (inputSystem_->IsActionPressed(static_cast<InputAction>(
                static_cast<int>(InputAction::CUSTOM_START) + 2))) {  // F5
            showVisibilityLines_ = !showVisibilityLines_;
            LOG_INFO("Show Visibility Lines: " + std::string(showVisibilityLines_ ? "ON" : "OFF"));
        }
        
        // Cycle through clusters with F6/F7
        if (bspTree_ && bspTree_->GetClusterCount() > 0) {
            if (inputSystem_->IsActionPressed(static_cast<InputAction>(
                    static_cast<int>(InputAction::CUSTOM_START) + 3))) {  // F6
                selectedCluster_ = (selectedCluster_ - 1 + bspTree_->GetClusterCount()) % 
                                 bspTree_->GetClusterCount();
                LOG_INFO("Selected Cluster: " + std::to_string(selectedCluster_));
            } else if (inputSystem_->IsActionPressed(static_cast<InputAction>(
                    static_cast<int>(InputAction::CUSTOM_START) + 4))) {  // F7
                selectedCluster_ = (selectedCluster_ + 1) % bspTree_->GetClusterCount();
                LOG_INFO("Selected Cluster: " + std::to_string(selectedCluster_));
            }
        }
    }
}

// BSP geometry rendering
void Renderer::RenderBSPGeometry()
{
    if (!worldGeometry_) {
        return;
    }

    // QUAKE-STYLE RENDERING PIPELINE - following the exact order from Quake 3
    visibleFaces_.clear();

    if (worldGeometry_->GetWorld() && bspTreeSystem_) {
        // Phase 1: PVS Visibility Determination (like Quake's R_MarkLeaves)
        // This MUST happen first - it marks entire subtrees as potentially visible
        bspTreeSystem_->MarkLeaves(*worldGeometry_->GetWorld(), camera_.position);

        // Phase 2: Recursive BSP Traversal with Frustum Culling (like R_RecursiveWorldNode)
        // Only traverses nodes marked visible by PVS, applies hierarchical frustum culling
        visibleFaces_.clear();

        bspTreeSystem_->TraverseForRendering(*worldGeometry_->GetWorld(), camera_,
            [&](const Face& face) {
                // Final face-level checks: backface culling only
                // Frustum culling already done at node level for efficiency
                // For now, always render faces in the new system (backface culling removed for debugging)
                // TODO: Add backface culling when ready
                if (true) {
                    visibleFaces_.push_back(&face);
                }
            });

        LOG_DEBUG("Quake-style rendering pipeline results:");
        LOG_DEBUG("  - Total faces in world: " + std::to_string(worldGeometry_->GetWorld()->surfaces.size()));
        LOG_DEBUG("  - Faces passing PVS + frustum culling: " + std::to_string(visibleFaces_.size()));
        if (!worldGeometry_->GetWorld()->surfaces.empty()) {
            float cullRate = 100.0f - ((float)visibleFaces_.size() / worldGeometry_->GetWorld()->surfaces.size() * 100.0f);
            LOG_DEBUG("  - Culling efficiency: " + std::to_string((int)cullRate) + "% culled");
        }
    } else {
        // Fallback: if no BSP tree, use all faces with basic visibility checks
        LOG_WARNING("No BSP tree available, using fallback rendering (significant performance impact)");
        size_t facesProcessed = 0;
        for (const auto& face : worldGeometry_->faces) {
            facesProcessed++;
            // For now, always render faces (backface culling removed for debugging)
            if (true) {
                visibleFaces_.push_back(&face);
            } else if (!bspTreeSystem_ && IsFaceVisibleForRendering(face, camera_)) {
                visibleFaces_.push_back(&face);
            }
        }
        LOG_DEBUG("Fallback processing: checked " + std::to_string(facesProcessed) + " faces, " +
                  std::to_string(visibleFaces_.size()) + " visible");
    }

    // Get MaterialSystem for material data access
    MaterialSystem* materialSystem = GetEngine().GetSystem<MaterialSystem>();

    // Create a default material component (materialId 0 for default)
    static MaterialComponent defaultMaterial(0); // materialId 0 = default material

    // Group faces by material for batching to reduce draw calls
    facesByMaterial_.clear();

    // First pass: group faces by material and count stats
    for (const Face* facePtr : visibleFaces_) {
        const Face& face = *facePtr;

        // Skip faces with NoDraw flag or no vertices
        if (static_cast<unsigned int>(face.flags) & static_cast<unsigned int>(FaceFlags::NoDraw) ||
            face.vertices.empty()) {
            continue;
        }

        // Use materialId as key (0 for default material)
        unsigned int materialKey = face.materialId;
        facesByMaterial_[materialKey].push_back(facePtr);

        surfacesRendered_++;
        trianglesRendered_ += (face.vertices.size() >= 3) ? face.vertices.size() - 2 : 0;
    }

    // Second pass: render each material group in batch (dramatically reduces draw calls)
    for (const auto& materialGroup : facesByMaterial_) {
        unsigned int materialId = materialGroup.first;
        const auto& faces = materialGroup.second;

        // Set up material for this batch (once per material, not per face)
        MaterialComponent faceMaterialComponent;

        // Look up material in WorldSystem's ID mapping
        auto worldSystem = GetEngine().GetSystem<WorldSystem>();
        if (worldSystem) {
            const auto& materialIdMap = worldSystem->GetMaterialIdMap();
            auto matIt = materialIdMap.find(materialId);
            if (matIt != materialIdMap.end()) {
                // Found material mapping - create MaterialComponent with the MaterialSystem ID
                faceMaterialComponent = MaterialComponent(matIt->second);
                LOG_DEBUG("Using materialId " + std::to_string(materialId) +
                         " -> MaterialSystem ID " + std::to_string(matIt->second));
            } else {
                // Material not found - use default material (ID 0)
                faceMaterialComponent = MaterialComponent(0);
                LOG_DEBUG("MaterialId " + std::to_string(materialId) + " not found, using default material");
            }
        } else {
            // No WorldSystem - use default material
            faceMaterialComponent = MaterialComponent(0);
            LOG_WARNING("WorldSystem not available for material lookup");
        }

        MaterialComponent* faceMaterial = &faceMaterialComponent;

        // Set up material once per batch
        if (faceMaterial) {
            SetupMaterial(*faceMaterial);
        }

        // Render all faces in this material batch (single draw call per face, but batched by material)
        for (const Face* facePtr : faces) {
            RenderFace(*facePtr);
        }
    }

    // Reset render state
    rlSetTexture(0);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    // BSP geometry rendering completed
}

// Skybox rendering (following WorldRenderer pattern)
void Renderer::RenderSkybox()
{
    static int skyboxRenderCount = 0;
    skyboxRenderCount++;

    if (skyboxRenderCount % 60 == 0) {
        LOG_INFO("Skybox: rendering");
    }

    if (worldGeometry_->skybox) {
        if (worldGeometry_->skybox->IsLoaded()) {
            worldGeometry_->skybox->Render(camera_);
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

// Setup material properties for rendering
void Renderer::SetupMaterial(const MaterialComponent& material)
{
    static int materialSetupCounter = 0;
    materialSetupCounter++;
    LOG_DEBUG("SetupMaterial called (count: " + std::to_string(materialSetupCounter) + ") for materialId " + std::to_string(material.materialId));

    // Get MaterialSystem for material data access
    MaterialSystem* materialSystem = GetEngine().GetSystem<MaterialSystem>();
    if (!materialSystem) {
        LOG_ERROR("SetupMaterial: MaterialSystem not available");
        return;
    }

    // Get the actual material data from the flyweight
    const MaterialData* materialData = materialSystem->GetMaterial(material.materialId);
    if (!materialData) {
        LOG_WARNING("SetupMaterial: No material data found for materialId " + std::to_string(material.materialId));
        return;
    }

    LOG_DEBUG("SetupMaterial called for materialId " + std::to_string(material.materialId) +
              " ('" + materialData->materialName + "')");

    // Determine color based on gradient mode
    Color diffuse;
    if (material.IsLinearGradient()) {
        // For gradients, use primary color as base (gradient is handled elsewhere)
        diffuse = materialData->primaryColor;
    } else if (material.IsRadialGradient()) {
        // For radial gradients, use primary color as base
        diffuse = materialData->primaryColor;
    } else {
        // Solid color - use primary color
        diffuse = materialData->primaryColor;
    }

    diffuse.a = (unsigned char)(materialData->alpha * 255.0f);

    LOG_DEBUG("SetupMaterial: Using diffuse color (" + std::to_string(diffuse.r) + "," +
              std::to_string(diffuse.g) + "," + std::to_string(diffuse.b) + "," +
              std::to_string(diffuse.a) + ") for material '" + materialData->materialName + "'");

    // Set the base color
    rlColor4ub(diffuse.r, diffuse.g, diffuse.b, diffuse.a);

    // Handle transparency
    if (materialData->alpha < 1.0f) {
        BeginBlendMode(BLEND_ALPHA);
    } else {
        EndBlendMode();
    }

    // Handle face culling (use MaterialComponent flags)
    if (material.IsDoubleSided()) {
        rlDisableBackfaceCulling();
    } else {
        rlEnableBackfaceCulling();
    }

    // Handle depth testing (use MaterialComponent flags)
    if (material.DepthTestEnabled()) {
        rlEnableDepthTest();
    } else {
        rlDisableDepthTest();
    }

    // Handle depth writing (use MaterialComponent flags)
    if (material.DepthWriteEnabled()) {
        rlEnableDepthMask();
    } else {
        rlDisableDepthMask();
    }

    // Bind diffuse texture if available
    if (!materialData->diffuseMap.empty()) {
        LOG_DEBUG("Attempting to get texture from AssetSystem for path: " + materialData->diffuseMap);
        Texture2D* tex = assetSystem_->GetOrLoadTexture(materialData->diffuseMap);
        if (tex && tex->id != 0) {
            LOG_DEBUG("SUCCESS: Got texture with ID " + std::to_string(tex->id) + ", binding it");
            if (lastBoundTexture_ != tex->id) {
                rlSetTexture(tex->id);
                lastBoundTexture_ = tex->id;

                // Set texture parameters
                SetTextureFilter(*tex, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(*tex, TEXTURE_WRAP_CLAMP);  // CLAMP for stretch-to-fill behavior
            }
        }
    }
}

// Render a single face
void Renderer::RenderFace(const Face& face)
{
    // Safety checks
    if (face.vertices.size() < 3) {
        LOG_WARNING("RenderFace: Face has " + std::to_string(face.vertices.size()) + " vertices, skipping");
        return;
    }
    
    // Decide UV source: either provided UVs or renderer-generated stretch UVs
    const bool needStretchUVs = useStretchUV_ || (face.uvs.size() != face.vertices.size());

    // Compute per-face stretch-to-fill UVs (tangent-space, normalized to 0..1)
    std::vector<Vector2> stretchUVs;
    if (needStretchUVs) {
        // Build tangent space from face normal using a stable world-up reference
        Vector3 normal = Vector3Normalize(face.normal);

        const Vector3 WORLD_UP   = {0.0f, 1.0f, 0.0f};
        const Vector3 WORLD_RIGHT= {1.0f, 0.0f, 0.0f};

        // Prefer bitangent aligned with world up when possible (keeps textures upright)
        Vector3 planeUp = WORLD_UP;
        float upDot = Vector3DotProduct(planeUp, normal);
        planeUp.x -= upDot * normal.x;
        planeUp.y -= upDot * normal.y;
        planeUp.z -= upDot * normal.z;
        float upLenSq = planeUp.x*planeUp.x + planeUp.y*planeUp.y + planeUp.z*planeUp.z;
        if (upLenSq < 1e-8f) {
            // If up is degenerate (face nearly vertical), project world right instead
            planeUp = WORLD_RIGHT;
            float rDot = Vector3DotProduct(planeUp, normal);
            planeUp.x -= rDot * normal.x;
            planeUp.y -= rDot * normal.y;
            planeUp.z -= rDot * normal.z;
        }
        Vector3 bitangent = Vector3Normalize(planeUp);
        Vector3 tangent = Vector3Normalize(Vector3CrossProduct(bitangent, normal));

        // If still degenerate, fallback to longest-edge method
        float tLenSq = tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z;
        if (tLenSq < 1e-8f) {
            float maxLenSq = -1.0f;
            const size_t nVerts = face.vertices.size();
            for (size_t i = 0; i < nVerts; ++i) {
                const Vector3& a = face.vertices[i];
                const Vector3& b = face.vertices[(i + 1) % nVerts];
                Vector3 edge = Vector3Subtract(b, a);
                float d = Vector3DotProduct(edge, normal);
                edge.x -= d * normal.x;
                edge.y -= d * normal.y;
                edge.z -= d * normal.z;
                float eLenSq = edge.x*edge.x + edge.y*edge.y + edge.z*edge.z;
                if (eLenSq > maxLenSq) { maxLenSq = eLenSq; tangent = edge; }
            }
            tangent = Vector3Normalize(tangent);
            bitangent = Vector3Normalize(Vector3CrossProduct(normal, tangent));
        }

        // Find bounds in tangent space
        float minU = FLT_MAX, maxU = -FLT_MAX;
        float minV = FLT_MAX, maxV = -FLT_MAX;
        for (const auto& vtx : face.vertices) {
            float u = Vector3DotProduct(vtx, tangent);
            float vv = Vector3DotProduct(vtx, bitangent);
            if (u < minU) minU = u; if (u > maxU) maxU = u;
            if (vv < minV) minV = vv; if (vv > maxV) maxV = vv;
        }
        const float uRange = maxU - minU;
        const float vRange = maxV - minV;
        stretchUVs.reserve(face.vertices.size());
        for (const auto& vtx : face.vertices) {
            float u = Vector3DotProduct(vtx, tangent);
            float vv = Vector3DotProduct(vtx, bitangent);
            if (uRange > 1e-5f) u = (u - minU) / uRange; else u = 0.5f;
            if (vRange > 1e-5f) vv = (vv - minV) / vRange; else vv = 0.5f;
            stretchUVs.push_back({u, vv});
        }
    }

    // Debug: warn if no texture is currently bound (likely to render white)
    unsigned int currentTexIdForCheck = lastBoundTexture_;
    if (face.vertices.size() != 3) { // triangles set inside rlBegin later
        if (currentTexIdForCheck == 0) {
            LOG_WARNING("RenderFace: No texture bound for face (materialId=" + std::to_string(face.materialId) + ")");
        }
    }

    // Debug: Log face details for problematic walls
    Vector3 faceMin = face.vertices[0];
    Vector3 faceMax = face.vertices[0];
    for (const auto& v : face.vertices) {
        faceMin = Vector3Min(faceMin, v);
        faceMax = Vector3Max(faceMax, v);
    }
    
    // Check if this is a wall that spans Z-axis from -5 to 5
    if (fabsf(faceMin.z + 5.0f) < 0.1f && fabsf(faceMax.z - 5.0f) < 0.1f) {
        LOG_DEBUG("WALL SPANNING Z[-5,5]: Material=" + std::to_string(face.materialId) + 
                  ", Vertices=" + std::to_string(face.vertices.size()) +
                  ", Normal=(" + std::to_string(face.normal.x) + "," + 
                  std::to_string(face.normal.y) + "," + std::to_string(face.normal.z) + ")");
        const auto& uvsToLog = needStretchUVs ? stretchUVs : face.uvs;
        for (size_t i = 0; i < uvsToLog.size(); i++) {
            LOG_DEBUG("  UV[" + std::to_string(i) + "]: (" + 
                      std::to_string(uvsToLog[i].x) + ", " + 
                      std::to_string(uvsToLog[i].y) + ")");
        }
    }

    // Select active UV list
    auto getUV = [&](size_t idx) -> const Vector2& {
        return needStretchUVs ? stretchUVs[idx] : face.uvs[idx];
    };

    // NOTE: Shader is now applied globally via BeginShaderMode() in RenderSystem
    // No need for manual shader application here

    // Render geometry based on vertex count
    size_t vcount = face.vertices.size();
    if (vcount == 3) {
        // Triangle rendering with rlgl - supports textures, normals, and shaders
        LOG_DEBUG("TRIANGLE RENDER: 3 vertices with shader + normal + texture support");
        rlBegin(RL_TRIANGLES);
            // Set texture inside rlBegin() for proper binding
            if (lastBoundTexture_ != 0) rlSetTexture(lastBoundTexture_);
            for (size_t i = 0; i < 3; i++) {
                rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
                rlNormal3f(face.normal.x, face.normal.y, face.normal.z);
                const Vector2& uvSrc = getUV(i);
                Vector2 uv = { uvSrc.x, 1.0f - uvSrc.y }; // Flip V globally
                rlTexCoord2f(uv.x, uv.y);
                rlVertex3f(face.vertices[i].x, face.vertices[i].y, face.vertices[i].z);
            }
        rlEnd();
        trianglesRendered_++;
    } else if (vcount == 4) {
        // Quad rendering with rlgl - supports textures, normals, and shaders
        LOG_DEBUG("QUAD RENDER: 4 vertices with shader + normal + texture support");
        rlBegin(RL_QUADS);
            // Set texture inside rlBegin() for proper binding
            if (lastBoundTexture_ != 0) rlSetTexture(lastBoundTexture_);
            rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
            rlNormal3f(face.normal.x, face.normal.y, face.normal.z);
            // Quad vertices in counter-clockwise order (like Raylib examples)
            for (size_t i = 0; i < 4; i++) {
                const Vector2& uvSrc = getUV(i);
                Vector2 uv = { uvSrc.x, 1.0f - uvSrc.y }; // Flip V globally
                rlTexCoord2f(uv.x, uv.y);
                rlVertex3f(face.vertices[i].x, face.vertices[i].y, face.vertices[i].z);
            }
        rlEnd();
        trianglesRendered_ += 2;
    } else if (vcount > 4) {
        // Triangle fan for polygons with more than 4 vertices using rlgl
        LOG_DEBUG("POLYGON RENDER: " + std::to_string(vcount) + " vertices as triangle fan with shader + normal + texture support");
        rlBegin(RL_TRIANGLES);
            // Set texture inside rlBegin() for proper binding
            if (lastBoundTexture_ != 0) rlSetTexture(lastBoundTexture_);
            for (size_t i = 1; i + 1 < vcount; ++i) {
                size_t idx0 = 0, idx1 = i, idx2 = i + 1;
                rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
                rlNormal3f(face.normal.x, face.normal.y, face.normal.z);
                const Vector2& uv0 = getUV(idx0);
                Vector2 uv0_flipped = { uv0.x, 1.0f - uv0.y };
                rlTexCoord2f(uv0_flipped.x, uv0_flipped.y);
                rlVertex3f(face.vertices[idx0].x, face.vertices[idx0].y, face.vertices[idx0].z);
                
                rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
                rlNormal3f(face.normal.x, face.normal.y, face.normal.z);
                const Vector2& uv1 = getUV(idx1);
                Vector2 uv1_flipped = { uv1.x, 1.0f - uv1.y };
                rlTexCoord2f(uv1_flipped.x, uv1_flipped.y);
                rlVertex3f(face.vertices[idx1].x, face.vertices[idx1].y, face.vertices[idx1].z);
                
                rlColor4ub(face.tint.r, face.tint.g, face.tint.b, face.tint.a);
                rlNormal3f(face.normal.x, face.normal.y, face.normal.z);
                const Vector2& uv2 = getUV(idx2);
                Vector2 uv2_flipped = { uv2.x, 1.0f - uv2.y };
                rlTexCoord2f(uv2_flipped.x, uv2_flipped.y);
                rlVertex3f(face.vertices[idx2].x, face.vertices[idx2].y, face.vertices[idx2].z);
                trianglesRendered_++;
            }
        rlEnd();
    }
}

void Renderer::Clear(Color color)
{
    ClearBackground(color);
}

// Main render command dispatcher - routes to appropriate rendering method
void Renderer::DrawRenderCommand(const RenderCommand& command)
{
    switch (command.type) {
        case RenderType::SPRITE_2D:
            if (!command.transform || !command.sprite) {
                LOG_WARNING("RenderCommand missing transform or sprite for 2D rendering");
                return;
            }
            DrawSprite2D(command);
            break;
        case RenderType::PRIMITIVE_3D:
            if (!command.transform || !command.sprite) {
                LOG_WARNING("RenderCommand missing transform or sprite for 3D primitive rendering");
                return;
            }
            DrawPrimitive3D(command);
            break;
        case RenderType::MESH_3D:
            if (!command.transform || !command.mesh) {
                LOG_WARNING("RenderCommand missing transform or mesh for 3D mesh rendering");
                return;
            }
            DrawMesh3D(command);
            break;
        case RenderType::WORLD_GEOMETRY:
            // World geometry is handled separately in RenderWorldGeometryDirect
            LOG_DEBUG("WORLD_GEOMETRY command encountered - should be handled separately");
            break;
        case RenderType::LIGHT_GIZMO:
            if (!command.transform || !command.entity) {
                LOG_WARNING("RenderCommand missing transform or entity for light gizmo rendering");
                return;
            }
            DrawLightGizmo(command);
            break;
        case RenderType::DEBUG:
            // Debug rendering handled separately
            break;
        default:
            LOG_WARNING("Unknown render type");
            break;
    }

    spritesRendered_++;
}

// Legacy method - now uses the dispatcher
void Renderer::DrawSprite(const RenderCommand& command)
{
    DrawRenderCommand(command);
}

// 2D Sprite rendering (billboards in 3D space)
void Renderer::DrawSprite2D(const RenderCommand& command)
{
    if (!command.sprite || !command.transform) {
        return;
    }

    // Get world position in 3D space
    Vector3 worldPos = command.transform->position;

    // Get rendering properties
    float scale = command.sprite->GetScale();
    Color tint = command.sprite->GetTint();

    if (command.sprite->IsTextureLoaded()) {
        // Draw texture-based sprite as billboard
        Texture2D texture = command.sprite->GetTexture();

        // Calculate billboard size based on texture and scale
        Vector2 size = {texture.width * scale, texture.height * scale};

        // Draw as billboard (always faces camera)
        DrawBillboard(camera_, texture, worldPos, size.x, tint);

        // Draw decal overlay if present
        Texture2D decal = command.sprite->GetDecalOverlay();
        if (decal.id != 0) {
            DrawBillboard(camera_, decal, worldPos, size.x, WHITE);
        }

        LOG_INFO("Rendered 2D sprite billboard at (" + std::to_string(worldPos.x) + ", " +
                 std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
    } else {
        LOG_WARNING("DrawSprite2D called on entity without texture");
    }
}

// 3D Primitive rendering (cubes, spheres, cylinders, etc.)
void Renderer::DrawPrimitive3D(const RenderCommand& command)
{
    if (!command.transform) {
        return;
    }

    // Legacy primitive rendering completely removed - all handled through mesh system now
    LOG_WARNING("Legacy primitive rendering called - should use mesh system instead");
}

// Cache invalidation for mesh changes
void Renderer::InvalidateMeshCache(uint64_t meshId) {
    if (modelCache_) {
        // For now, just log - in a full implementation we'd need to map entity IDs to cache IDs
        LOG_DEBUG("InvalidateMeshCache called for mesh ID " + std::to_string(meshId) + 
                 " - CacheSystem handles automatic cleanup");
        // The CacheSystem's reference counting will handle cleanup when references drop to 0
    }
}

// Mesh rendering
void Renderer::DrawMesh3D(const RenderCommand& command)
{
    if (!command.transform || !command.mesh) {
        return;
    }

    Vector3 worldPos = command.transform->position;
    Vector3 scale = command.transform->scale;
    Quaternion rotation = command.transform->rotation;
    const auto& mesh = *command.mesh;

    // Convert quaternion to axis-angle for Raylib
    Vector3 rotationAxis = {0.0f, 1.0f, 0.0f};
    float rotationAngle = 0.0f;
    if (rotation.x != 0 || rotation.y != 0 || rotation.z != 0 || rotation.w != 1) {
        QuaternionToAxisAngle(rotation, &rotationAxis, &rotationAngle);
    }

    // Get material color for Raylib drawing functions
    Color drawColor = WHITE;
    if (command.material) {
        MaterialSystem* materialSystem = GetEngine().GetSystem<MaterialSystem>();
        if (materialSystem) {
            const MaterialData* materialData = materialSystem->GetMaterial(command.material->materialId);
            if (materialData) {
                drawColor = materialData->primaryColor;
            }
        }
    }

    // Handle composite meshes generically
    if (mesh.meshType == MeshComponent::MeshType::COMPOSITE) {
        RenderCompositeMesh(command, mesh, worldPos, scale);
        return;
    }
    
    // Handle single primitives 
    if (mesh.meshType == MeshComponent::MeshType::PRIMITIVE) {
        // Single primitives fall through to cached model pathway
        // The MeshSystem::CreateXXX() methods create the geometry, and RenderAssetCache converts it to a cached Model
    }

    // For custom meshes (including pyramids), use cached model
    LOG_DEBUG("Drawing custom mesh: " + mesh.meshName + " with " +
             std::to_string(mesh.vertices.size()) + " vertices");

    // Get or create cached model (this replaces the expensive mesh conversion)
    uint32_t modelId = modelCache_->GetOrCreate(mesh);
    if (modelId == 0) {
        LOG_WARNING("Failed to get cached model for mesh: " + mesh.meshName);
        return;
    }
    
    CachedModelData* cachedModelData = modelCache_->GetMutable(modelId);
    if (!cachedModelData || cachedModelData->model.meshCount == 0) {
        LOG_WARNING("Failed to get cached model data for mesh: " + mesh.meshName);
        return;
    }
    
    Model& cachedModel = cachedModelData->model;

    // Apply material via unified MaterialSystem
    if (command.material) {
        MaterialSystem* materialSystem = GetEngine().GetSystem<MaterialSystem>();
        if (materialSystem) {
            materialSystem->ApplyMaterialToModel(command.material->materialId, cachedModel, 0);
            LOG_DEBUG("Applied material to cached model via MaterialSystem");
        }
    } else {
        // Legacy texture application (fallback for entities without MaterialComponent)
        if (meshSystem_) {
            auto texture = meshSystem_->GetTexture(command.entity);
            if (texture.id != 0) {
                SetMaterialTexture(&cachedModel.materials[0], MATERIAL_MAP_DIFFUSE, texture);
                LOG_DEBUG("Applied legacy texture to cached model for entity");
            }
        }
    }

    // Disable backface culling for this model to ensure all faces are visible
    rlDisableBackfaceCulling();

    // Use shadow shader if in shadow rendering mode
    if (inShadowMode_ && shadowShader_) {
        BeginShaderMode(*shadowShader_);
        DrawModelEx(cachedModel, worldPos, rotationAxis, rotationAngle, scale, WHITE);
        EndShaderMode();
    } else {
        // Draw the cached model with default material shader
        DrawModelEx(cachedModel, worldPos, rotationAxis, rotationAngle, scale, WHITE);
    }

    // Re-enable backface culling
    rlEnableBackfaceCulling();

    // No cleanup needed - model is cached and will be reused!

    LOG_DEBUG("Drew custom mesh " + mesh.meshName + " with " +
             std::to_string(mesh.vertices.size()) + " vertices");
}

void Renderer::RenderCompositeMesh(const RenderCommand& command, const MeshComponent& mesh, const Vector3& worldPos, const Vector3& scale) {
    // Look up composite mesh definition by ID (data-oriented approach)
    MeshSystem* meshSystem = GetEngine().GetSystem<MeshSystem>();
    if (!meshSystem) {
        LOG_ERROR("MeshSystem not available for composite mesh rendering");
        return;
    }
    
    const CompositeMeshDefinition* compositeDef = meshSystem->GetCompositeMeshDefinition(mesh.compositeMeshId);
    if (!compositeDef) {
        LOG_WARNING("Composite mesh definition not found for ID: " + std::to_string(mesh.compositeMeshId));
        return;
    }
    
    if (compositeDef->subMeshes.empty()) {
        LOG_WARNING("Composite mesh '" + mesh.meshName + "' has no sub-meshes defined");
        return;
    }
    
    LOG_DEBUG("Rendering composite mesh '" + mesh.meshName + "' with " + std::to_string(compositeDef->subMeshes.size()) + " sub-meshes");
    
    // Render each sub-mesh in the composite
    for (const auto& subMesh : compositeDef->subMeshes) {
        // Calculate world position for this sub-mesh
        Vector3 subMeshWorldPos = {
            worldPos.x + (subMesh.relativePosition.x * scale.x),
            worldPos.y + (subMesh.relativePosition.y * scale.y),
            worldPos.z + (subMesh.relativePosition.z * scale.z)
        };
        
        // Calculate effective scale for this sub-mesh
        Vector3 effectiveScale = {
            scale.x * subMesh.relativeScale.x,
            scale.y * subMesh.relativeScale.y,
            scale.z * subMesh.relativeScale.z
        };
        
        // Create appropriate primitive mesh based on type
        Mesh primitiveMesh = {0};
        if (subMesh.primitiveType == "sphere") {
            float radius = subMesh.radius * effectiveScale.x; // Use X scale for radius
            primitiveMesh = GenMeshSphere(radius, 16, 16);
        } 
        else if (subMesh.primitiveType == "cylinder") {
            float radius = subMesh.radius * effectiveScale.x;
            float height = subMesh.height * effectiveScale.y;
            primitiveMesh = GenMeshCylinder(radius, height, 16);
        }
        else if (subMesh.primitiveType == "cube") {
            primitiveMesh = GenMeshCube(subMesh.size.x * effectiveScale.x, 
                                       subMesh.size.y * effectiveScale.y, 
                                       subMesh.size.z * effectiveScale.z);
        }
        else {
            LOG_WARNING("Unknown primitive type in composite mesh: " + subMesh.primitiveType);
            continue;
        }
        
        // Create temporary MeshComponent for caching the sub-mesh
        if (primitiveMesh.vertexCount > 0) {
            // Create a MeshComponent for this sub-mesh to use with our caching system
            MeshComponent subMeshComponent;
            subMeshComponent.meshType = MeshComponent::MeshType::PRIMITIVE;
            subMeshComponent.primitiveShape = subMesh.primitiveType;
            subMeshComponent.isStatic = true;
            
            // Generate cache-friendly mesh name based on sub-mesh properties
            if (subMesh.primitiveType == "sphere") {
                float radius = subMesh.radius * effectiveScale.x;
                subMeshComponent.meshName = "sphere_" + std::to_string(radius);
            } else if (subMesh.primitiveType == "cylinder") {
                float radius = subMesh.radius * effectiveScale.x;
                float height = subMesh.height * effectiveScale.y;
                subMeshComponent.meshName = "cylinder_" + std::to_string(radius) + "x" + std::to_string(height);
            } else if (subMesh.primitiveType == "cube") {
                Vector3 size = {subMesh.size.x * effectiveScale.x, subMesh.size.y * effectiveScale.y, subMesh.size.z * effectiveScale.z};
                subMeshComponent.meshName = "cube_" + std::to_string(size.x);
            }
            
            // Get or create cached model for this sub-mesh (reuses existing caching system!)
            uint32_t subMeshModelId = modelCache_->GetOrCreate(subMeshComponent);
            if (subMeshModelId != 0) {
                CachedModelData* cachedSubMeshData = modelCache_->GetMutable(subMeshModelId);
                if (cachedSubMeshData && cachedSubMeshData->model.meshCount > 0) {
                    Model& cachedSubMeshModel = cachedSubMeshData->model;
                    
                    // Get material data for per-frame application (don't modify cached model!)
                    Material* raylibMaterial = nullptr;
                    if (command.material) {
                        MaterialSystem* materialSystem = GetEngine().GetSystem<MaterialSystem>();
                        if (materialSystem) {
                            raylibMaterial = materialSystem->GetCachedRaylibMaterial(command.material->materialId);
                            LOG_DEBUG("🎨 RETRIEVED MATERIAL for composite sub-mesh (" + subMesh.primitiveType + "):");
                            LOG_DEBUG("  Entity Material ID: " + std::to_string(command.material->materialId));
                            if (raylibMaterial) {
                                LOG_DEBUG("  Material texture ID: " + std::to_string(raylibMaterial->maps[MATERIAL_MAP_DIFFUSE].texture.id));
                                LOG_DEBUG("  Material shader ID: " + std::to_string(raylibMaterial->shader.id));
                            }
                        } else {
                            LOG_WARNING("❌ MaterialSystem not available for composite sub-mesh");
                        }
                    } else {
                        LOG_WARNING("❌ No material component for composite mesh entity");
                    }
                    
                    // Temporarily apply material to model for this frame only
                    Material originalMaterial = {0};
                    bool materialApplied = false;
                    if (raylibMaterial && cachedSubMeshModel.materialCount > 0) {
                        // Backup original material
                        originalMaterial = cachedSubMeshModel.materials[0];
                        
                        // 🚨 CRITICAL FIX: Copy texture maps and properties WITHOUT overwriting shader!
                        Material& targetMaterial = cachedSubMeshModel.materials[0];
                        
                        // Copy texture maps (preserve shader!)
                        targetMaterial.maps[MATERIAL_MAP_DIFFUSE] = raylibMaterial->maps[MATERIAL_MAP_DIFFUSE];
                        targetMaterial.maps[MATERIAL_MAP_NORMAL] = raylibMaterial->maps[MATERIAL_MAP_NORMAL];
                        targetMaterial.maps[MATERIAL_MAP_SPECULAR] = raylibMaterial->maps[MATERIAL_MAP_SPECULAR];
                        targetMaterial.maps[MATERIAL_MAP_ROUGHNESS] = raylibMaterial->maps[MATERIAL_MAP_ROUGHNESS];
                        targetMaterial.maps[MATERIAL_MAP_METALNESS] = raylibMaterial->maps[MATERIAL_MAP_METALNESS];
                        targetMaterial.maps[MATERIAL_MAP_OCCLUSION] = raylibMaterial->maps[MATERIAL_MAP_OCCLUSION];
                        targetMaterial.maps[MATERIAL_MAP_EMISSION] = raylibMaterial->maps[MATERIAL_MAP_EMISSION];
                        targetMaterial.maps[MATERIAL_MAP_HEIGHT] = raylibMaterial->maps[MATERIAL_MAP_HEIGHT];
                        targetMaterial.maps[MATERIAL_MAP_CUBEMAP] = raylibMaterial->maps[MATERIAL_MAP_CUBEMAP];
                        targetMaterial.maps[MATERIAL_MAP_IRRADIANCE] = raylibMaterial->maps[MATERIAL_MAP_IRRADIANCE];
                        targetMaterial.maps[MATERIAL_MAP_PREFILTER] = raylibMaterial->maps[MATERIAL_MAP_PREFILTER];
                        targetMaterial.maps[MATERIAL_MAP_BRDF] = raylibMaterial->maps[MATERIAL_MAP_BRDF];
                        
                        // Copy material parameters (preserve shader!)
                        for (int paramIdx = 0; paramIdx < 4; paramIdx++) {
                            targetMaterial.params[paramIdx] = raylibMaterial->params[paramIdx];
                        }
                        
                        // 🔥 DO NOT COPY SHADER - keep the cached model's working shader!
                        // targetMaterial.shader = raylibMaterial->shader; // REMOVED - this was breaking rendering!
                        
                        materialApplied = true;
                        
                        LOG_DEBUG("  🎨 TEMP APPLIED material textures (preserved shader ID: " + std::to_string(targetMaterial.shader.id) +
                                 ", diffuse texture: " + std::to_string(targetMaterial.maps[MATERIAL_MAP_DIFFUSE].texture.id) + ")");
                    }
                    
                    // Draw the cached sub-mesh model with entity material
                    DrawModel(cachedSubMeshModel, subMeshWorldPos, 1.0f, WHITE);
                    
                    // Restore original material to keep cache clean
                    if (materialApplied) {
                        cachedSubMeshModel.materials[0] = originalMaterial;
                        LOG_DEBUG("  🔄 RESTORED original material to cached model");
                    }
                    
                    LOG_DEBUG("  ✅ Rendered cached " + subMesh.primitiveType + " sub-mesh at (" + 
                             std::to_string(subMeshWorldPos.x) + "," + std::to_string(subMeshWorldPos.y) + "," + std::to_string(subMeshWorldPos.z) + ")");
                } else {
                    LOG_WARNING("Failed to get cached sub-mesh model data for " + subMesh.primitiveType);
                }
            } else {
                LOG_WARNING("Failed to cache sub-mesh model for " + subMesh.primitiveType);
            }
            
            // No cleanup needed - model is cached and managed by CacheSystem!
        }
    }
    
    LOG_DEBUG("Completed composite mesh rendering for '" + mesh.meshName + "'");
}


void Renderer::FlushMeshBatch()
{
    if (!batchInProgress_ || vertexBuffer_.empty()) {
        return;
    }

    // Use Raylib's built-in drawing functions for better compatibility
    rlBegin(RL_TRIANGLES);

    // Draw all triangles from the batched data
    for (size_t i = 0; i < indexBuffer_.size(); i += 3) {
        // Get vertex indices for this triangle
        unsigned int i1 = indexBuffer_[i];
        unsigned int i2 = indexBuffer_[i + 1];
        unsigned int i3 = indexBuffer_[i + 2];

        // Get vertex data (9 floats per vertex: pos3 + color4 + texcoord2)
        size_t v1_offset = i1 * 9;
        size_t v2_offset = i2 * 9;
        size_t v3_offset = i3 * 9;

        // Vertex 1
        rlColor4f(vertexBuffer_[v1_offset + 3], vertexBuffer_[v1_offset + 4],
                  vertexBuffer_[v1_offset + 5], vertexBuffer_[v1_offset + 6]);
        rlTexCoord2f(vertexBuffer_[v1_offset + 7], vertexBuffer_[v1_offset + 8]);
        rlVertex3f(vertexBuffer_[v1_offset], vertexBuffer_[v1_offset + 1], vertexBuffer_[v1_offset + 2]);

        // Vertex 2
        rlColor4f(vertexBuffer_[v2_offset + 3], vertexBuffer_[v2_offset + 4],
                  vertexBuffer_[v2_offset + 5], vertexBuffer_[v2_offset + 6]);
        rlTexCoord2f(vertexBuffer_[v2_offset + 7], vertexBuffer_[v2_offset + 8]);
        rlVertex3f(vertexBuffer_[v2_offset], vertexBuffer_[v2_offset + 1], vertexBuffer_[v2_offset + 2]);

        // Vertex 3
        rlColor4f(vertexBuffer_[v3_offset + 3], vertexBuffer_[v3_offset + 4],
                  vertexBuffer_[v3_offset + 5], vertexBuffer_[v3_offset + 6]);
        rlTexCoord2f(vertexBuffer_[v3_offset + 7], vertexBuffer_[v3_offset + 8]);
        rlVertex3f(vertexBuffer_[v3_offset], vertexBuffer_[v3_offset + 1], vertexBuffer_[v3_offset + 2]);
    }

    rlEnd();

    // Reset batch state
    batchInProgress_ = false;
    currentTextureId_ = 0;
    vertexBuffer_.clear();
    indexBuffer_.clear();

    LOG_DEBUG("Flushed optimized mesh batch with " + std::to_string(vertexBuffer_.size() / 9) + " vertices");
}

// Instanced rendering support
void Renderer::AddInstance(const std::string& meshName, const Renderer::InstanceData& instance)
{
    if (!instancingEnabled_) {
        return;
    }

    instanceGroups_[meshName].push_back(instance);
    LOG_DEBUG("Added instance for mesh '" + meshName + "' - total instances: " +
              std::to_string(instanceGroups_[meshName].size()));
}

void Renderer::FlushInstances()
{
    if (!instancingEnabled_ || instanceGroups_.empty()) {
        return;
    }

    LOG_DEBUG("Flushing " + std::to_string(instanceGroups_.size()) + " instance groups");

    for (const auto& group : instanceGroups_) {
        const std::string& meshName = group.first;
        const std::vector<InstanceData>& instances = group.second;

        if (instances.empty()) {
            continue;
        }

        LOG_DEBUG("Rendering " + std::to_string(instances.size()) + " instances of mesh '" + meshName + "'");

        // For now, render each instance individually using our optimized DrawMesh3D
        // In a more advanced implementation, we could use Raylib's instancing features
        // or implement our own GPU instancing

        for (const auto& instance : instances) {
            // Create a simple cube mesh for this instance with proper size and position
            MeshComponent tempMesh;
            CreateSimpleCubeMesh(tempMesh, instance.position, instance.scale.x, instance.tint);

            // Create transform data (origin since position is already baked into vertices)
            TransformComponent tempTransform;
            tempTransform.position = {0.0f, 0.0f, 0.0f};
            tempTransform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            tempTransform.scale = {1.0f, 1.0f, 1.0f};
            tempTransform.isActive = true;

            // Apply instance rotation by setting mesh instance data
            tempMesh.instanceRotation = instance.rotation;
            tempMesh.instanceScale = {1.0f, 1.0f, 1.0f}; // Scale already applied to vertices
            tempMesh.isInstanced = true;

            // Create render command with the instance data
            RenderCommand cmd(nullptr, &tempTransform, nullptr, &tempMesh);

            // Render using our optimized method
            DrawMesh3D(cmd);
        }
    }

    ClearInstances();
}

void Renderer::ClearInstances()
{
    instanceGroups_.clear();
    LOG_DEBUG("Cleared all instance groups");
}

// Helper method to create a simple cube mesh for instances
void Renderer::CreateSimpleCubeMesh(MeshComponent& mesh, const Vector3& position, float size, const Color& color)
{
    float halfSize = size * 0.5f;

    // Define cube vertices (8 corners) - centered at origin, then we'll translate by position
    mesh.vertices = {
        {{position.x - halfSize, position.y - halfSize, position.z - halfSize}, {0, 0, -1}, {0, 0}, color}, // 0: front-bottom-left
        {{position.x + halfSize, position.y - halfSize, position.z - halfSize}, {0, 0, -1}, {1, 0}, color}, // 1: front-bottom-right
        {{position.x + halfSize, position.y + halfSize, position.z - halfSize}, {0, 0, -1}, {1, 1}, color}, // 2: front-top-right
        {{position.x - halfSize, position.y + halfSize, position.z - halfSize}, {0, 0, -1}, {0, 1}, color}, // 3: front-top-left
        {{position.x - halfSize, position.y - halfSize, position.z + halfSize}, {0, 0,  1}, {0, 0}, color}, // 4: back-bottom-left
        {{position.x + halfSize, position.y - halfSize, position.z + halfSize}, {0, 0,  1}, {1, 0}, color}, // 5: back-bottom-right
        {{position.x + halfSize, position.y + halfSize, position.z + halfSize}, {0, 0,  1}, {1, 1}, color}, // 6: back-top-right
        {{position.x - halfSize, position.y + halfSize, position.z + halfSize}, {0, 0,  1}, {0, 1}, color}  // 7: back-top-left
    };

    // Define cube triangles (12 triangles for 6 faces, 2 per face)
    mesh.triangles = {
        // Front face
        {0, 1, 2}, {0, 2, 3},
        // Back face
        {5, 4, 7}, {5, 7, 6},
        // Left face
        {4, 0, 3}, {4, 3, 7},
        // Right face
        {1, 5, 6}, {1, 6, 2},
        // Top face
        {3, 2, 6}, {3, 6, 7},
        // Bottom face
        {4, 5, 1}, {4, 1, 0}
    };

    mesh.meshName = "instance_cube";
    mesh.isActive = true;
}

void Renderer::DrawDebugInfo()
{
    // Don't draw debug info during 3D rendering - it will be drawn after EndMode3D()
    // This function is called while still in 3D mode, so we'll skip drawing here
}

void Renderer::DrawGrid(float spacing, Color color)
{
    // Draw a simple 3D grid on the ground plane (y = 0)
    int gridSize = 20;

    for (int i = -gridSize; i <= gridSize; i++) {
        // Draw lines parallel to Z axis
        Vector3 start = {(float)i * spacing, 0.0f, (float)-gridSize * spacing};
        Vector3 end = {(float)i * spacing, 0.0f, (float)gridSize * spacing};
        DrawLine3D(start, end, color);

        // Draw lines parallel to X axis
        start = {(float)-gridSize * spacing, 0.0f, (float)i * spacing};
        end = {(float)gridSize * spacing, 0.0f, (float)i * spacing};
        DrawLine3D(start, end, color);
    }
}

void Renderer::SetCameraPosition(float x, float y, float z)
{
    camera_.position.x = x;
    camera_.position.y = y;
    camera_.position.z = z;
}

void Renderer::SetCameraTarget(float x, float y, float z)
{
    camera_.target.x = x;
    camera_.target.y = y;
    camera_.target.z = z;
}

void Renderer::SetCameraRotation(float rotation)
{
    // For 3D camera, we can rotate around the up axis
    // This is a simple implementation - more complex rotation would need quaternion math
    camera_.position.x = camera_.target.x + cos(rotation) * 10.0f;
    camera_.position.z = camera_.target.z + sin(rotation) * 10.0f;
}

void Renderer::SetCameraZoom(float zoom)
{
    camera_.fovy = (zoom > 5.0f) ? zoom : 5.0f; // Minimum FOV to prevent extreme zoom
    camera_.fovy = (zoom < 120.0f) ? zoom : 120.0f; // Maximum FOV
}

void Renderer::UpdateCameraToFollowPlayer(float playerX, float playerY, float playerZ)
{
    // Set camera position to player position (first-person view)
    const float eyeHeight = 1.5f;  // Eye level above player position
    
    camera_.position.x = playerX;
    camera_.position.y = playerY + eyeHeight;
    camera_.position.z = playerZ;

    // Calculate look direction using yaw and pitch
    Vector3 lookDirection = SphericalToCartesian(yaw_, pitch_, 1.0f);
    
    // Set camera target based on look direction
    camera_.target.x = camera_.position.x + lookDirection.x;
    camera_.target.y = camera_.position.y + lookDirection.y;
    camera_.target.z = camera_.position.z + lookDirection.z;
}

void Renderer::UpdateCameraRotation(float mouseDeltaX, float mouseDeltaY, float deltaTime)
{
    // Input is already scaled by PlayerSystem, just apply the rotation
    yaw_ += mouseDeltaX;    // Horizontal rotation (yaw)
    pitch_ -= mouseDeltaY;  // Vertical rotation (pitch) - inverted for natural up/down

    // Normalize yaw to [0, 2π] for consistency
    while (yaw_ > 2.0f * PI) yaw_ -= 2.0f * PI;
    while (yaw_ < 0.0f) yaw_ += 2.0f * PI;

    // Clamp pitch to prevent camera flipping (standard FPS behavior)
    // Allow looking up/down but prevent going completely upside down
    const float maxPitch = PI * 0.45f;  // About 81° up/down
    if (pitch_ > maxPitch) pitch_ = maxPitch;
    if (pitch_ < -maxPitch) pitch_ = -maxPitch;
}

Vector2 Renderer::ScreenToWorld(Vector2 screenPos) const
{
    // For 3D camera, this is more complex. For now, return a simple approximation
    // In a real implementation, this would use ray casting from screen to world
    return {screenPos.x - screenWidth_ / 2.0f, screenPos.y - screenHeight_ / 2.0f};
}

Vector2 Renderer::WorldToScreen(Vector2 worldPos) const
{
    // For 3D camera, this is more complex. For now, return a simple approximation
    // In a real implementation, this would project 3D world position to 2D screen
    return {worldPos.x + screenWidth_ / 2.0f, worldPos.y + screenHeight_ / 2.0f};
}

void Renderer::UpdateScreenSize()
{
    screenWidth_ = GetScreenWidth();
    screenHeight_ = GetScreenHeight();
}

Vector3 Renderer::SphericalToCartesian(float yaw, float pitch, float radius) const
{
    // Convert yaw/pitch to cartesian coordinates
    // yaw: rotation around Y axis (0 = looking towards -Z, which is "forward" in our coordinate system)
    // pitch: rotation from horizontal plane (0 = horizontal, π/2 = up, -π/2 = down)
    
    Vector3 result;
    result.x = radius * cosf(pitch) * sinf(yaw);    // X component  
    result.y = radius * sinf(pitch);                // Y component (vertical)
    result.z = radius * cosf(pitch) * cosf(yaw);    // Z component
    
    // FIXED: Negate Z because we start looking towards negative Z (forward)
    result.z = -result.z;
    
    return result;
}

void Renderer::UpdateCameraFromAngles()
{
    // This function can be used to update camera when angles change
    // For now, it's called from UpdateCameraToFollowPlayer
    Vector3 lookDirection = SphericalToCartesian(yaw_, pitch_, 1.0f);

    camera_.target.x = camera_.position.x + lookDirection.x;
    camera_.target.y = camera_.position.y + lookDirection.y;
    camera_.target.z = camera_.position.z + lookDirection.z;
}

bool Renderer::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                      Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const
{
    hitEntity = nullptr;
    float closestDistance = maxDistance;

    // Cast ray against BSP world first
    Vector3 worldHitPoint, worldHitNormal;
    if (CastRayWorld(origin, direction, maxDistance, worldHitPoint, worldHitNormal)) {
        Vector3 toHit = Vector3Subtract(worldHitPoint, origin);
        float distance = Vector3Length(toHit);
        if (distance < closestDistance) {
            closestDistance = distance;
            hitPoint = worldHitPoint;
            hitNormal = worldHitNormal;
        }
    }

    // Cast ray against entities
    Vector3 entityHitPoint, entityHitNormal;
    Entity* entityHit = nullptr;
    if (CastRayEntities(origin, direction, closestDistance, entityHitPoint, entityHitNormal, entityHit)) {
        hitPoint = entityHitPoint;
        hitNormal = entityHitNormal;
        hitEntity = entityHit;
    }

    return closestDistance < maxDistance;
}

bool Renderer::CastRayWorld(const Vector3& origin, const Vector3& direction, float maxDistance,
                           Vector3& hitPoint, Vector3& hitNormal) const
{
    // TODO: Implement ray casting using the new World system
    // For now, return false (no hit)
    return false;
}

bool Renderer::CastRayEntities(const Vector3& origin, const Vector3& direction, float maxDistance,
                              Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const
{
    hitEntity = nullptr;
    float closestDistance = maxDistance;
    bool hit = false;

    Vector3 normalizedDir = Vector3Normalize(direction);

    for (Entity* entity : renderableEntities_) {
        if (!entity) continue;

        auto* position = entity->GetComponent<Position>();
        auto* collidable = entity->GetComponent<Collidable>();

        if (!position || !collidable) continue;

        Vector3 entityPos = position->GetPosition();
        AABB entityBounds = collidable->GetBounds();

        // Ray-AABB intersection test
        Vector3 invDir = {1.0f/normalizedDir.x, 1.0f/normalizedDir.y, 1.0f/normalizedDir.z};

        float tmin = 0.0f;
        float tmax = maxDistance;

        for (int i = 0; i < 3; ++i) {
            float originVal = (i == 0) ? origin.x : (i == 1) ? origin.y : origin.z;
            float dirVal = (i == 0) ? invDir.x : (i == 1) ? invDir.y : invDir.z;
            float minVal = (i == 0) ? entityBounds.min.x : (i == 1) ? entityBounds.min.y : entityBounds.min.z;
            float maxVal = (i == 0) ? entityBounds.max.x : (i == 1) ? entityBounds.max.y : entityBounds.max.z;

            float t1 = (minVal - originVal) * dirVal;
            float t2 = (maxVal - originVal) * dirVal;

            tmin = std::max(tmin, std::min(t1, t2));
            tmax = std::min(tmax, std::max(t1, t2));
        }

        if (tmax >= tmin && tmin < closestDistance && tmin >= 0) {
            Vector3 entityHitPoint = Vector3Add(origin, Vector3Scale(normalizedDir, tmin));
            if (tmin < closestDistance) {
                closestDistance = tmin;
                hitPoint = entityHitPoint;
                hitNormal = {0, 0, -1}; // Default normal, could be improved
                hitEntity = entity;
                hit = true;
            }
        }
    }

    return hit;
}

// Culling methods
bool Renderer::IsEntityVisible(const Vector3& position, float boundingRadius) const {
    cullingStats_.totalEntitiesChecked++;

    if (!enableFrustumCulling_) {
        cullingStats_.entitiesVisible++;
        return true;
    }

    // TEMPORARILY DISABLE FRUSTUM CULLING FOR DEBUGGING
    cullingStats_.entitiesVisible++;
    return true;

    // First do distance-based culling (cheapest check)
    Vector3 cameraPos = GetCameraPosition();
    float distance = Vector3Distance(cameraPos, position);

    // Check far clip distance
    if (distance > farClipDistance_ + boundingRadius) {
        cullingStats_.entitiesCulledByDistance++;
        LOG_DEBUG("Entity culled: distance (" + std::to_string(distance) + ") > far clip (" + std::to_string(farClipDistance_) + ")");
        return false;
    }

    // Create bounding box for the entity
    BoundingBox entityBounds = {
        {position.x - boundingRadius, position.y - boundingRadius, position.z - boundingRadius},
        {position.x + boundingRadius, position.y + boundingRadius, position.z + boundingRadius}
    };

    // Get camera frustum and test intersection
    // Note: We need to calculate frustum planes manually since Raylib doesn't expose GetCameraFrustum()
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera_.up));
    Vector3 up = Vector3CrossProduct(right, forward);

    float halfFovY = camera_.fovy * 0.5f * DEG2RAD;
    float aspectRatio = (float)screenWidth_ / (float)screenHeight_;
    float halfFovX = atanf(tanf(halfFovY) * aspectRatio);

    // Calculate frustum corner directions
    Vector3 nearCenter = Vector3Add(camera_.position, Vector3Scale(forward, 0.1f)); // Near plane at 0.1 units
    Vector3 farCenter = Vector3Add(camera_.position, Vector3Scale(forward, farClipDistance_));

    float nearHeight = 2.0f * tanf(halfFovY) * 0.1f;
    float nearWidth = nearHeight * aspectRatio;
    float farHeight = 2.0f * tanf(halfFovY) * farClipDistance_;
    float farWidth = farHeight * aspectRatio;

    // Simple frustum-AABB intersection test using separating axis theorem
    // Check if entity is completely outside any frustum plane
    
    // For performance, we'll use a simplified check:
    // 1. Distance culling (already done above)
    // 2. Check if entity is roughly within the view cone
    
    Vector3 toEntity = Vector3Subtract(position, camera_.position);
    float entityDistance = Vector3Length(toEntity);
    
    if (entityDistance > 0.001f) { // Avoid division by zero
        Vector3 toEntityNorm = Vector3Scale(toEntity, 1.0f / entityDistance);
        
        // Check if entity is roughly in front of camera
        float dotForward = Vector3DotProduct(toEntityNorm, forward);
        if (dotForward < -0.1f) { // Allow slight tolerance for entities just behind camera
            cullingStats_.entitiesCulledByFrustum++;
            LOG_DEBUG("Entity culled: behind camera (dot=" + std::to_string(dotForward) + ")");
            return false;
        }
        
        // Check horizontal field of view
        float dotRight = Vector3DotProduct(toEntityNorm, right);
        float horizontalAngle = acosf(fmaxf(-1.0f, fminf(1.0f, dotForward))); // Clamp to avoid NaN
        float maxHorizontalAngle = halfFovX + atanf(boundingRadius / entityDistance);
        
        if (fabsf(dotRight) > sinf(maxHorizontalAngle)) {
            cullingStats_.entitiesCulledByFrustum++;
            LOG_DEBUG("Entity culled: outside horizontal FOV");
            return false;
        }
        
        // Check vertical field of view
        float dotUp = Vector3DotProduct(toEntityNorm, up);
        float maxVerticalAngle = halfFovY + atanf(boundingRadius / entityDistance);
        
        if (fabsf(dotUp) > sinf(maxVerticalAngle)) {
            cullingStats_.entitiesCulledByFrustum++;
            LOG_DEBUG("Entity culled: outside vertical FOV");
            return false;
        }
    }

    // Entity passed all culling tests
    cullingStats_.entitiesVisible++;
    LOG_DEBUG("Entity visible: distance=" + std::to_string(distance) + ", position=(" +
              std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) + ")");
    return true;
}

// Face visibility check for rendering - proper culling logic
bool Renderer::IsFaceVisibleForRendering(const Face& face, const Camera3D& camera) const {
    // Skip faces with rendering flags
    if (static_cast<unsigned int>(face.flags) & static_cast<unsigned int>(FaceFlags::Invisible)) return false;
    if (static_cast<unsigned int>(face.flags) & static_cast<unsigned int>(FaceFlags::NoDraw)) return false;

    // Calculate face center for backface culling
    Vector3 center{0, 0, 0};
    for (const auto& v : face.vertices) center = Vector3Add(center, v);
    center = Vector3Scale(center, 1.0f / (float)face.vertices.size());

    // TEMPORARILY DISABLE FRUSTUM CULLING FOR DEBUGGING
    // Frustum culling: check if face intersects view frustum
    // Use face center as primary test, with vertex checks as backup
    /*
    if (!IsPointInViewFrustum(center, camera)) {
        // Check individual vertices - if any vertex is visible, face might be visible
        bool anyVertexVisible = false;
        for (const auto& vertex : face.vertices) {
            if (IsPointInViewFrustum(vertex, camera)) {
                anyVertexVisible = true;
                break;
            }
        }
        if (!anyVertexVisible) {
            return false; // Face is completely outside frustum
        }
    }
    */

    // Backface culling: reject faces facing away from camera
    // Direction from camera to face center
    Vector3 viewDir = Vector3Normalize(Vector3Subtract(center, camera.position));
    float dot = Vector3DotProduct(face.normal, viewDir);

    // Standard backface culling - reject faces with normal pointing away from camera
    // Allow small epsilon for floating point precision
    bool backfaceVisible = dot >= -0.1f;

    // DEBUG: Log first few faces
    static int debugCount = 0;
    if (debugCount < 5) {
        LOG_DEBUG("Face " + std::to_string(debugCount) + " center: (" +
                  std::to_string(center.x) + "," + std::to_string(center.y) + "," + std::to_string(center.z) +
                  ") normal: (" + std::to_string(face.normal.x) + "," + std::to_string(face.normal.y) + "," +
                  std::to_string(face.normal.z) + ") dot: " + std::to_string(dot) + " visible: " +
                  (backfaceVisible ? "YES" : "NO"));
        debugCount++;
    }

    return backfaceVisible;
}

// Check if a point is within the camera's view frustum
bool Renderer::IsPointInViewFrustum(const Vector3& point, const Camera3D& camera) const {
    // Compute camera forward
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 toPoint = Vector3Normalize(Vector3Subtract(point, camera.position));

    // Vertical FOV is camera.fovy (degrees). Convert to radians/half-angle.
    float halfVertFovRad = camera.fovy * (PI / 180.0f) * 0.5f;

    // Derive horizontal FOV from aspect ratio
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float aspect = (screenH > 0) ? (float)screenW / (float)screenH : 1.0f;
    float halfHorizFovRad = atanf(tanf(halfVertFovRad) * aspect);

    // Build camera basis (right, up)
    Vector3 up = camera.up;
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, up));
    up = Vector3Normalize(Vector3CrossProduct(right, forward));

    // Project toPoint onto basis to get angles
    float forwardDot = Vector3DotProduct(forward, toPoint);
    float rightDot = Vector3DotProduct(right, toPoint);
    float upDot = Vector3DotProduct(up, toPoint);

    // Reject points behind the camera
    if (forwardDot <= 0.0f) return false;

    // Compute angles via atan2 of lateral vs forward components
    float horizAngle = fabsf(atan2f(rightDot, forwardDot));
    float vertAngle = fabsf(atan2f(upDot, forwardDot));

    return (horizAngle <= halfHorizFovRad) && (vertAngle <= halfVertFovRad);
}

// Check if an AABB intersects the camera's view frustum
bool Renderer::IsAABBInViewFrustum(const AABB& box, const Camera3D& camera) const {
    // Check all 8 corners of the AABB against the view frustum
    Vector3 corners[8] = {
        {box.min.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.min.z},
        {box.min.x, box.max.y, box.min.z},
        {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.max.x, box.min.y, box.max.z},
        {box.min.x, box.max.y, box.max.z},
        {box.max.x, box.max.y, box.max.z}
    };

    // If any corner is inside the frustum, the AABB intersects
    for (const auto& corner : corners) {
        if (IsPointInViewFrustum(corner, camera)) {
            return true;
        }
    }

    // Check if camera is inside the AABB
    Vector3 cameraPos = camera.position;
    if (cameraPos.x >= box.min.x && cameraPos.x <= box.max.x &&
        cameraPos.y >= box.min.y && cameraPos.y <= box.max.y &&
        cameraPos.z >= box.min.z && cameraPos.z <= box.max.z) {
        return true;
    }

    // AABB is outside the frustum
    return false;
}

// === PVS Debug Visualization Methods ===

void Renderer::DebugDrawClusters(bool showAllClusters, bool showVisibilityLines) const {
    // TODO: Implement cluster debug drawing for new World system
    // For now, draw leaf nodes as clusters since we simplified the clustering
    if (!worldGeometry_ || !worldGeometry_->GetWorld()) return;

    const World* world = worldGeometry_->GetWorld();

    // Draw all leaf nodes (each leaf is currently a cluster)
    std::function<void(const BSPNode*)> drawNode = [&](const BSPNode* node) {
        if (!node) return;

        if (node->IsLeaf() && !node->surfaceIndices.empty()) {
            // Choose color based on cluster ID (using node address as hash for now)
            uintptr_t addr = reinterpret_cast<uintptr_t>(node);
            Color clusterColor = {
                static_cast<unsigned char>((addr * 37) % 255),
                static_cast<unsigned char>((addr * 71) % 255),
                static_cast<unsigned char>((addr * 113) % 255),
                100
            };

            // Draw leaf bounds
            Vector3 size = {
                node->maxs.x - node->mins.x,
                node->maxs.y - node->mins.y,
                node->maxs.z - node->mins.z
            };
            Vector3 center = {
                (node->mins.x + node->maxs.x) * 0.5f,
                (node->mins.y + node->maxs.y) * 0.5f,
                (node->mins.z + node->maxs.z) * 0.5f
            };

            DrawCubeWires(center, size.x, size.y, size.z, clusterColor);
        }

        // Recurse to children
        if (!node->IsLeaf()) {
            drawNode(node->children[0]);
            drawNode(node->children[1]);
        }
    };

    drawNode(world->root);
}

void Renderer::DebugDrawClusterPVS(int32_t clusterId) const {
    // Implement cluster PVS debug drawing for new World system
    if (!worldGeometry_ || !worldGeometry_->GetWorld()) return;

    const World* world = worldGeometry_->GetWorld();

    // For now, since we simplified clustering, clusterId corresponds to leaf index
    // Find the Nth leaf in the tree
    std::vector<const BSPNode*> leaves;
    std::function<void(const BSPNode*)> collectLeaves = [&](const BSPNode* node) {
        if (!node) return;
        if (node->IsLeaf()) {
            leaves.push_back(node);
        } else {
            collectLeaves(node->children[0]);
            collectLeaves(node->children[1]);
        }
    };
    collectLeaves(world->root);

    if (clusterId < 0 || clusterId >= static_cast<int32_t>(leaves.size())) {
        return;
    }

    const BSPNode* selectedLeaf = leaves[clusterId];

    // Highlight selected cluster/leaf in red
    Vector3 size = {
        selectedLeaf->maxs.x - selectedLeaf->mins.x,
        selectedLeaf->maxs.y - selectedLeaf->mins.y,
        selectedLeaf->maxs.z - selectedLeaf->mins.z
    };
    Vector3 center = {
        (selectedLeaf->mins.x + selectedLeaf->maxs.x) * 0.5f,
        (selectedLeaf->mins.y + selectedLeaf->maxs.y) * 0.5f,
        (selectedLeaf->mins.z + selectedLeaf->maxs.z) * 0.5f
    };

    DrawCubeWires(center, size.x, size.y, size.z, RED);

    // Since we currently make all clusters visible to all others (no PVS culling),
    // highlight all other leaves in green
    for (size_t i = 0; i < leaves.size(); ++i) {
        if (static_cast<int32_t>(i) != clusterId) {
            const BSPNode* leaf = leaves[i];

            Vector3 leafSize = {
                leaf->maxs.x - leaf->mins.x,
                leaf->maxs.y - leaf->mins.y,
                leaf->maxs.z - leaf->mins.z
            };
            Vector3 leafCenter = {
                (leaf->mins.x + leaf->maxs.x) * 0.5f,
                (leaf->mins.y + leaf->maxs.y) * 0.5f,
                (leaf->mins.z + leaf->maxs.z) * 0.5f
            };

            // Draw connection line
            DrawLine3D(center, leafCenter, YELLOW);

            // Highlight visible leaf bounds
            DrawCubeWires(leafCenter, leafSize.x, leafSize.y, leafSize.z, GREEN);
        }
    }
}

void Renderer::DebugDrawAllClusterBounds() const {
    if (!worldGeometry_ || !worldGeometry_->GetWorld()) return;

    const World* world = worldGeometry_->GetWorld();

    // Draw all leaf bounds in white (each leaf is currently a cluster)
    std::function<void(const BSPNode*)> drawAllLeaves = [&](const BSPNode* node) {
        if (!node) return;

        if (node->IsLeaf() && !node->surfaceIndices.empty()) {
            Vector3 size = {
                node->maxs.x - node->mins.x,
                node->maxs.y - node->mins.y,
                node->maxs.z - node->mins.z
            };
            Vector3 center = {
                (node->mins.x + node->maxs.x) * 0.5f,
                (node->mins.y + node->maxs.y) * 0.5f,
                (node->mins.z + node->maxs.z) * 0.5f
            };

            DrawCubeWires(center, size.x, size.y, size.z, WHITE);
        }

        // Recurse to children
        if (!node->IsLeaf()) {
            drawAllLeaves(node->children[0]);
            drawAllLeaves(node->children[1]);
        }
    };

    drawAllLeaves(world->root);
}

// Shader management for BSP geometry
void Renderer::SetCurrentShader(const Shader& shader) {
    currentShader_ = const_cast<Shader*>(&shader);
    hasCurrentShader_ = true;
    LOG_DEBUG("🎨 Renderer: Set current shader for BSP geometry (ID: " + std::to_string(shader.id) + ")");
}

void Renderer::ClearCurrentShader() {
    currentShader_ = nullptr;
    hasCurrentShader_ = false;
    LOG_DEBUG("🎨 Renderer: Cleared current shader for BSP geometry");
}

void Renderer::DrawLightGizmo(const RenderCommand& command) {
    if (!command.entity || !command.transform) {
        LOG_WARNING("DrawLightGizmo: Missing entity or transform");
        return;
    }

    // Get light component from entity
    auto light = command.entity->GetComponent<LightComponent>();
    if (!light) {
        LOG_WARNING("DrawLightGizmo: Entity missing LightComponent");
        return;
    }

    // Temporarily end shader mode to avoid lighting interference with gizmo primitives
    bool shaderWasActive = (currentShader_ != nullptr);
    if (shaderWasActive) {
        EndShaderMode();
    }

    // Disable depth testing for gizmos so they don't block light rays
    rlDisableDepthTest();

    Vector3 position = command.transform->position;
    Color lightColor = light->color;
    
    // Render different gizmo types based on light type
    switch (light->type) {
        case LightType::POINT: {
            // Draw sphere for point light
            float radius = 0.2f; // Small visible radius
            DrawSphere(position, radius, lightColor);
            
            // Draw wireframe sphere to show light range using radius field
            DrawSphereWires(position, light->radius * 0.001f, 8, 8, ColorAlpha(lightColor, 0.3f)); // Scale down radius for visibility
            break;
        }
        
        case LightType::DIRECTIONAL: {
            // Draw arrow showing light direction (pointing down for typical directional light)
            Vector3 direction = {0.0f, -1.0f, 0.0f}; // Default downward direction
            Vector3 endPos = Vector3Add(position, Vector3Scale(direction, 2.0f));
            
            // Draw line showing direction
            DrawLine3D(position, endPos, lightColor);
            
            // Draw sun-like rays
            for (int i = 0; i < 8; i++) {
                float angle = i * 45.0f * DEG2RAD;
                Vector3 rayDir = {cosf(angle), 0.0f, sinf(angle)};
                Vector3 rayEnd = Vector3Add(position, Vector3Scale(rayDir, 1.0f));
                DrawLine3D(position, rayEnd, ColorAlpha(lightColor, 0.6f));
            }
            
            // Draw small sphere at light position
            DrawSphere(position, 0.15f, lightColor);
            break;
        }
        
        case LightType::SPOT: {
            // Draw cone showing spot light coverage
            Vector3 direction = {0.0f, -1.0f, 0.0f}; // Default downward direction
            float height = light->range * 0.001f; // Scale down range
            
            // Calculate cone radius from outer angle
            float coneRadius = tanf(light->outerAngle * DEG2RAD) * height;
            
            // Draw cone wireframe
            DrawCylinderWires(position, 0.0f, coneRadius, height, 8, ColorAlpha(lightColor, 0.4f));
            
            // Draw small sphere at light position  
            DrawSphere(position, 0.1f, lightColor);
            break;
        }
    }

    // Re-enable depth testing
    rlEnableDepthTest();

    // Restore shader mode if it was active before drawing gizmos
    if (shaderWasActive && currentShader_) {
        BeginShaderMode(*currentShader_);
        LOG_DEBUG("🔄 Restored shader mode after drawing light gizmo");
    }

    LOG_DEBUG("Drew light gizmo for " + std::to_string((int)light->type) + " light at (" +
             std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
}

// Shadow rendering mode
void Renderer::BeginShadowMode(Shader& depthShader) {
    shadowShader_ = &depthShader;
    inShadowMode_ = true;
    LOG_DEBUG("🌑 Entered shadow rendering mode");
}

void Renderer::EndShadowMode() {
    shadowShader_ = nullptr;
    inShadowMode_ = false;
    LOG_DEBUG("🌑 Exited shadow rendering mode");
}

