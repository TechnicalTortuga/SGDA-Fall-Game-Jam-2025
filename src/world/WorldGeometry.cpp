#include "WorldGeometry.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <unordered_map>
#include <cfloat>
#include "../rendering/Skybox.h"
#include <cfloat>  // For FLT_MAX

// Constants for tangent space UV calculation
static constexpr float NORMAL_VERTICAL_THRESHOLD = 0.9f; // Dot product threshold for vertical normals

// CalculateSurfaceProjectedUV
//
// Uses surface-oriented projection to generate consistent UV coordinates
// that work reliably regardless of face orientation. This mathematical
// approach analyzes the face normal to determine the optimal projection
// plane and ensures consistent texture orientation.
//
// Parameters:
// - vertex: The vertex position to calculate UV for
// - vertices: All vertices in the face (for bounds calculation)
// - faceNormal: The face normal vector
// - minVert: Minimum vertex bounds of the face
// - maxVert: Maximum vertex bounds of the face
//
// Returns: std::pair<float, float> UV coordinates (U, V)
static std::pair<float, float> CalculateSurfaceProjectedUV(const Vector3& vertex,
                                                            const std::vector<Vector3>& vertices,
                                                            const Vector3& faceNormal,
                                                            const Vector3& minVert,
                                                            const Vector3& maxVert) {
    Vector3 normal = Vector3Normalize(faceNormal);
    
    // Create a proper tangent space for the face
    // This works for ANY orientation, including slopes and arbitrary angles
    
    // Step 1: Find the most stable tangent vector
    // We'll use the edge that's most perpendicular to the normal
    Vector3 tangent = {1.0f, 0.0f, 0.0f}; // Default tangent
    
    // If the normal is too aligned with X axis, use Y axis as starting tangent
    if (fabsf(normal.x) > 0.9f) {
        tangent = {0.0f, 1.0f, 0.0f};
    }
    
    // Project tangent onto the plane defined by the normal
    // This ensures our tangent is actually on the surface
    float dot = Vector3DotProduct(tangent, normal);
    tangent.x -= dot * normal.x;
    tangent.y -= dot * normal.y;
    tangent.z -= dot * normal.z;
    tangent = Vector3Normalize(tangent);
    
    // Step 2: Calculate bitangent using cross product
    // This gives us the second axis of our UV space
    Vector3 bitangent = Vector3CrossProduct(normal, tangent);
    bitangent = Vector3Normalize(bitangent);
    
    // Step 3: Calculate face bounds in the tangent space
    float minU = FLT_MAX, maxU = -FLT_MAX;
    float minV = FLT_MAX, maxV = -FLT_MAX;
    
    // Project all vertices onto our tangent/bitangent axes
    for (const auto& v : vertices) {
        float u = Vector3DotProduct(v, tangent);
        float v_coord = Vector3DotProduct(v, bitangent);
        
        minU = fminf(minU, u);
        maxU = fmaxf(maxU, u);
        minV = fminf(minV, v_coord);
        maxV = fmaxf(maxV, v_coord);
    }
    
    float uRange = maxU - minU;
    float vRange = maxV - minV;
    
    // Step 4: Calculate UV for this vertex
    // Project vertex onto tangent space and normalize to 0-1 range
    float u = Vector3DotProduct(vertex, tangent);
    float v = Vector3DotProduct(vertex, bitangent);
    
    // Normalize to 0-1 range (texture stretches to fill the entire face)
    if (uRange > 0.001f) {
        u = (u - minU) / uRange;
    } else {
        u = 0.5f;
    }
    
    if (vRange > 0.001f) {
        v = (v - minV) / vRange;
    } else {
        v = 0.5f;
    }
    
    // Ensure UV coordinates are in valid range
    u = fmaxf(0.0f, fminf(1.0f, u));
    v = fmaxf(0.0f, fminf(1.0f, v));
    
    LOG_DEBUG("UV Generation: vertex(" + std::to_string(vertex.x) + "," + 
              std::to_string(vertex.y) + "," + std::to_string(vertex.z) + 
              ") -> UV(" + std::to_string(u) + "," + std::to_string(v) + ")");
    
    return {u, v};
}

WorldGeometry::~WorldGeometry() = default;

WorldGeometry::WorldGeometry() {
    Initialize();
}

void WorldGeometry::Initialize() {
    bspTree = nullptr;
    batches.clear();
    materialIdMap.clear();
    levelName = "Untitled Level";
    levelBoundsMin = {0.0f, 0.0f, 0.0f};
    levelBoundsMax = {0.0f, 0.0f, 0.0f};
    skyColor = SKYBLUE;

        // Initialize skybox system
        skybox = std::make_unique<Skybox>();
}

void WorldGeometry::Clear() {
    if (bspTree) {
        bspTree->Clear();
        bspTree.reset();
    }
    ClearBatches();
    materialIdMap.clear();
    brushes.clear();
    faces.clear();
    levelName = "Untitled Level";
    levelBoundsMin = {0.0f, 0.0f, 0.0f};
    levelBoundsMax = {0.0f, 0.0f, 0.0f};
    skyColor = SKYBLUE;

    // Unload skybox resources
    if (skybox) {
        skybox->Unload();
    }
}

bool WorldGeometry::ContainsPoint(const Vector3& point) const {
    // WorldGeometry no longer handles BSP queries directly
    // Use WorldSystem or CollisionSystem for BSP queries
    return false;
}

float WorldGeometry::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    // WorldGeometry no longer handles BSP queries directly
    // Use WorldSystem or CollisionSystem for BSP queries
    return maxDistance;
}

// Surface-based queries removed (face-only pipeline)

uint32_t WorldGeometry::GetMaterialId(int surfaceId) const {
    auto it = materialIdMap.find(surfaceId);
    return (it != materialIdMap.end()) ? it->second : 0; // Return 0 for default material
}

void WorldGeometry::CalculateBounds() {
    if (!bspTree) return;

    const auto& faces = bspTree->GetAllFaces();
    if (faces.empty()) return;

    levelBoundsMin = faces[0].vertices.empty() ? Vector3{0,0,0} : faces[0].vertices[0];
    levelBoundsMax = levelBoundsMin;

    for (const auto& face : faces) {
        for (const auto& v : face.vertices) {
            levelBoundsMin.x = std::min(levelBoundsMin.x, v.x);
            levelBoundsMin.y = std::min(levelBoundsMin.y, v.y);
            levelBoundsMin.z = std::min(levelBoundsMin.z, v.z);

            levelBoundsMax.x = std::max(levelBoundsMax.x, v.x);
            levelBoundsMax.y = std::max(levelBoundsMax.y, v.y);
            levelBoundsMax.z = std::max(levelBoundsMax.z, v.z);
        }
    }
}

// Skybox logic is now handled by the Skybox class.




std::vector<const Face*> WorldGeometry::GetVisibleFaces(const Camera3D& camera) const {
    // Since visibility logic is now handled by the Renderer,
    // this method returns all faces (conservative approach)
    std::vector<const Face*> visibleFaces;
    for (const auto& face : faces) {
        visibleFaces.push_back(&face);
    }
    return visibleFaces;
}

void WorldGeometry::BuildBSPFromFaces(const std::vector<Face>& inFaces) {
    faces = inFaces;

    // Pre-calculate UV coordinates for faces that don't already have them
    for (auto& face : faces) {
        if (face.uvs.empty()) {
            CalculateFaceUVs(face);
        }
    }

    // Note: BSP tree is now built externally and set via SetBSPTree()
    BuildBatchesFromFaces(faces);
    size_t triCount = 0; for (const auto& b : batches) triCount += b.indices.size()/3;
    LOG_INFO("WorldGeometry: built " + std::to_string(batches.size()) + " batches, ~" + std::to_string(triCount) + " tris");
    if (bspTree) {
        LOG_INFO("WorldGeometry: BSP tree has " + std::to_string(bspTree->GetClusterCount()) + " clusters");
    }
}

void WorldGeometry::BuildBSPFromBrushes(const std::vector<Brush>& inBrushes) {
    brushes = inBrushes;
    std::vector<Face> flat;
    for (const auto& b : brushes) {
        for (const auto& f : b.faces) flat.push_back(f);
    }
    faces = flat;

    // Pre-calculate UV coordinates for faces that don't already have them
    for (auto& face : faces) {
        if (face.uvs.empty()) {
            CalculateFaceUVs(face);
        }
    }

    // Note: BSP tree is now built externally and set via SetBSPTree()
    BuildBatchesFromFaces(faces);
    size_t triCount = 0; for (const auto& b : batches) triCount += b.indices.size()/3;
    LOG_INFO("WorldGeometry: built " + std::to_string(batches.size()) + " batches from brushes, ~" + std::to_string(triCount) + " tris");
    if (bspTree) {
        LOG_INFO("WorldGeometry: BSP tree has " + std::to_string(bspTree->GetClusterCount()) + " clusters");
    }
}

// Calculate UV coordinates for a face and store them in the face.uvs vector
void WorldGeometry::CalculateFaceUVs(Face& face) {
    if (face.vertices.empty()) {
        LOG_WARNING("CalculateFaceUVs: Face has no vertices, skipping UV calculation");
        return;
    }

    // Validate face normal
    if (Vector3Length(face.normal) < 0.1f) {
        LOG_WARNING("CalculateFaceUVs: Face normal is invalid, recalculating");
        face.RecalculateNormal();
    }

    // Clear existing UVs
    face.uvs.clear();
    face.uvs.reserve(face.vertices.size());

    // Calculate face bounds for surface projection UV calculation
    Vector3 minVert = face.vertices[0];
    Vector3 maxVert = face.vertices[0];
    for (const auto& vertex : face.vertices) {
        minVert = Vector3Min(minVert, vertex);
        maxVert = Vector3Max(maxVert, vertex);
    }

    // Validate face bounds (ensure face isn't degenerate)
    Vector3 faceSize = Vector3Subtract(maxVert, minVert);
    if (faceSize.x < 0.001f && faceSize.y < 0.001f && faceSize.z < 0.001f) {
        LOG_WARNING("CalculateFaceUVs: Face is degenerate (too small), using default UVs");
        GenerateDefaultUVsForFace(face);
        return;
    }

    // Use surface-oriented projection for consistent UV coordinates regardless of face normal
    // This mathematical approach analyzes the face normal to determine optimal projection
    // plane and ensures consistent texture orientation
    for (const auto& vertex : face.vertices) {
        auto [u, v] = CalculateSurfaceProjectedUV(vertex, face.vertices, face.normal, minVert, maxVert);
        face.uvs.push_back({u, v});
    }

    // Validate generated UVs
    ValidateAndFixUVs(face);

    LOG_DEBUG("CalculateFaceUVs: Generated " + std::to_string(face.uvs.size()) + " UV coordinates");
}

// Validate UV coordinates and fix any issues
void WorldGeometry::ValidateAndFixUVs(Face& face) {
    // Check if face has matching number of UVs and vertices
    if (face.uvs.size() != face.vertices.size()) {
        LOG_WARNING("ValidateAndFixUVs: UV count (" + std::to_string(face.uvs.size()) + 
                   ") doesn't match vertex count (" + std::to_string(face.vertices.size()) + ")");
        GenerateDefaultUVsForFace(face);
        return;
    }

    // Only check for NaN or invalid values, don't clamp since we want stretch-to-fill
    for (const auto& uv : face.uvs) {
        // Check for NaN or invalid values
        if (std::isnan(uv.x) || std::isnan(uv.y) || std::isinf(uv.x) || std::isinf(uv.y)) {
            LOG_WARNING("ValidateAndFixUVs: Found invalid UV values (NaN/Inf), regenerating");
            GenerateDefaultUVsForFace(face);
            return;
        }
    }
    
    // Log if we have any UVs outside 0-1 range (this is OK for our stretch-to-fill approach)
    bool hasOutOfRange = false;
    for (const auto& uv : face.uvs) {
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
            hasOutOfRange = true;
            break;
        }
    }
    
    if (hasOutOfRange) {
        LOG_DEBUG("ValidateAndFixUVs: UVs outside 0-1 range detected (this is expected for stretch-to-fill)");
    }
}

// Generate default UV coordinates for a face using simple planar projection
void WorldGeometry::GenerateDefaultUVsForFace(Face& face) {
    LOG_DEBUG("GenerateDefaultUVsForFace: Generating default UVs for face with " + 
              std::to_string(face.vertices.size()) + " vertices");

    face.uvs.clear();
    face.uvs.reserve(face.vertices.size());

    if (face.vertices.empty()) {
        LOG_WARNING("GenerateDefaultUVsForFace: No vertices to generate UVs for");
        return;
    }

    // Calculate face bounds
    Vector3 minVert = face.vertices[0];
    Vector3 maxVert = face.vertices[0];
    for (const auto& vertex : face.vertices) {
        minVert = Vector3Min(minVert, vertex);
        maxVert = Vector3Max(maxVert, vertex);
    }

    // Generate simple planar UVs based on dominant axis
    Vector3 faceSize = Vector3Subtract(maxVert, minVert);
    Vector3 absNormal = {fabsf(face.normal.x), fabsf(face.normal.y), fabsf(face.normal.z)};

    for (const auto& vertex : face.vertices) {
        float u, v;
        
        if (absNormal.z >= absNormal.x && absNormal.z >= absNormal.y) {
            // Z-dominant face - use XY projection
            u = faceSize.x > 0.001f ? (vertex.x - minVert.x) / faceSize.x : 0.5f;
            v = faceSize.y > 0.001f ? (vertex.y - minVert.y) / faceSize.y : 0.5f;
        } else if (absNormal.y >= absNormal.x) {
            // Y-dominant face - use XZ projection
            u = faceSize.x > 0.001f ? (vertex.x - minVert.x) / faceSize.x : 0.5f;
            v = faceSize.z > 0.001f ? (vertex.z - minVert.z) / faceSize.z : 0.5f;
        } else {
            // X-dominant face - use YZ projection (Z=U horizontal, Y=V vertical)
            u = faceSize.z > 0.001f ? (vertex.z - minVert.z) / faceSize.z : 0.5f;
            v = faceSize.y > 0.001f ? (vertex.y - minVert.y) / faceSize.y : 0.5f;
        }

        // Clamp to valid range
        u = fmaxf(0.0f, fminf(1.0f, u));
        v = fmaxf(0.0f, fminf(1.0f, v));
        
        face.uvs.push_back({u, v});
    }

    LOG_DEBUG("GenerateDefaultUVsForFace: Generated " + std::to_string(face.uvs.size()) + " default UV coordinates");
}

void WorldGeometry::ClearBatches() {
    batches.clear();
}



void WorldGeometry::BuildBatchesFromFaces(const std::vector<Face>& inFaces) {
    // Build renderable geometry batches from BSP faces
    //
    // This function converts BSP face data into optimized render batches with proper UV coordinates.
    // Key features:
    // - Groups faces by material for efficient rendering
    // - Triangulates polygons (quads -> 2 triangles, n-gons -> triangle fan)
    // - Calculates UV coordinates using tangent space projection for consistent texture orientation
    // - Handles both horizontal (floor/ceiling) and vertical (wall) faces appropriately

    // Reset existing batches
    ClearBatches();

    // Group faces by materialId
    std::unordered_map<unsigned int, size_t> batchIndexByMaterial;
    for (const auto& f : inFaces) {
        if (batchIndexByMaterial.find(f.materialId) == batchIndexByMaterial.end()) {
            StaticBatch batch;
            batch.materialId = f.materialId;
            batches.push_back(std::move(batch));
            batchIndexByMaterial[f.materialId] = batches.size() - 1;
        }
        size_t bi = batchIndexByMaterial[f.materialId];
        auto& batch = batches[bi];

        // Handle polygons - prefer quads, triangulate if necessary
        const auto& v = f.vertices;
        if (v.size() == 4) {
            // Handle as quad
            unsigned int base = (unsigned int)batch.positions.size();

            // Add the 4 vertices
            for (size_t i = 0; i < 4; i++) {
                batch.positions.push_back(v[i]);
            }

            // Calculate UVs
            Vector3 edge1 = Vector3Subtract(v[1], v[0]);
            Vector3 edge2 = Vector3Subtract(v[2], v[1]);
            Vector3 faceNormal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
            bool isHorizontal = fabsf(faceNormal.y) > 0.9f;

            Vector3 minVert = v[0];
            Vector3 maxVert = v[0];
            for (const auto& vertex : v) {
                minVert = Vector3Min(minVert, vertex);
                maxVert = Vector3Max(maxVert, vertex);
            }

            // Use pre-calculated UVs from the face
            for (size_t i = 0; i < 4; i++) {
                batch.uvs.push_back(f.uvs[i]);
            }

            // Colors - store face tint, material handling done in renderer
            Color finalColor = f.tint;
            LOG_INFO("BATCH COLOR (quad): materialId=" + std::to_string(f.materialId) + " using face tint (" +
                     std::to_string(f.tint.r) + "," + std::to_string(f.tint.g) + "," + std::to_string(f.tint.b) + ")");

            for (int i = 0; i < 4; i++) {
                batch.colors.push_back(finalColor);
            }
        } else if (v.size() == 3) {
            // Handle triangle natively - no conversion to quad
            unsigned int base = (unsigned int)batch.positions.size();

            // Add the 3 vertices exactly as they are
            for (size_t i = 0; i < 3; i++) {
                batch.positions.push_back(v[i]);
            }

            // Use pre-calculated UVs from the face (exactly 3 UVs for 3 vertices)
            for (size_t i = 0; i < 3; i++) {
                batch.uvs.push_back(f.uvs[i]);
            }

            // Colors - unified material handling will be done in renderer
            Color finalColor = f.tint;
            LOG_INFO("BATCH COLOR (triangle): materialId=" + std::to_string(f.materialId) + " using face tint (" +
                     std::to_string(f.tint.r) + "," + std::to_string(f.tint.g) + "," + std::to_string(f.tint.b) + ")");

            // Add exactly 3 colors for 3 vertices
            for (int i = 0; i < 3; i++) {
                batch.colors.push_back(finalColor);
            }
        }
    }
}
