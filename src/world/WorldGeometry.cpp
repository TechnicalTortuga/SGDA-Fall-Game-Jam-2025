#include "WorldGeometry.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <unordered_map>
#include <cfloat>
#include "../rendering/Skybox.h"

// Constants for tangent space UV calculation
static constexpr float NORMAL_VERTICAL_THRESHOLD = 0.9f; // Dot product threshold for vertical normals

// CalculateTangentSpaceUV
//
// Creates a local coordinate system for the face and projects vertices onto
// tangent/bitangent vectors to generate proper UV coordinates that work
// consistently regardless of face orientation.
//
// Parameters:
// - vertex: The vertex position to calculate UV for
// - vertices: All vertices in the face (for bounds calculation)
// - faceNormal: The face normal vector
// - minVert: Minimum vertex bounds of the face
// - maxVert: Maximum vertex bounds of the face
//
// Returns: std::pair<float, float> UV coordinates (U, V)
static std::pair<float, float> CalculateTangentSpaceUV(const Vector3& vertex,
                                                        const std::vector<Vector3>& vertices,
                                                        const Vector3& faceNormal,
                                                        const Vector3& minVert,
                                                        const Vector3& maxVert) {
    Vector3 normal = Vector3Normalize(faceNormal);

    // Find tangent vector (U direction) - perpendicular to normal
    Vector3 up = {0, 1, 0};
    Vector3 tangent;
    if (fabsf(Vector3DotProduct(normal, up)) > NORMAL_VERTICAL_THRESHOLD) {
        // Normal is nearly vertical, use right vector instead to avoid degenerate tangent
        Vector3 right = {1, 0, 0};
        tangent = Vector3Normalize(Vector3CrossProduct(normal, right));
    } else {
        tangent = Vector3Normalize(Vector3CrossProduct(normal, up));
    }

    // Find bitangent vector (V direction) - perpendicular to both normal and tangent
    Vector3 bitangent = Vector3Normalize(Vector3CrossProduct(normal, tangent));

    // Calculate face center for relative positioning
    Vector3 faceCenter = {
        (minVert.x + maxVert.x) * 0.5f,
        (minVert.y + maxVert.y) * 0.5f,
        (minVert.z + maxVert.z) * 0.5f
    };

    // Find UV bounds by projecting all vertices onto tangent/bitangent
    float uMin = FLT_MAX, uMax = -FLT_MAX;
    float vMin = FLT_MAX, vMax = -FLT_MAX;

    for (const auto& faceVertex : vertices) {
        Vector3 vertexRelative = Vector3Subtract(faceVertex, faceCenter);
        float uProj = Vector3DotProduct(vertexRelative, tangent);
        float vProj = Vector3DotProduct(vertexRelative, bitangent);
        uMin = fminf(uMin, uProj);
        uMax = fmaxf(uMax, uProj);
        vMin = fminf(vMin, vProj);
        vMax = fmaxf(vMax, vProj);
    }

    // Project current vertex and map to UV coordinates (0-1 range)
    Vector3 vertexRelative = Vector3Subtract(vertex, faceCenter);
    float uProj = Vector3DotProduct(vertexRelative, tangent);
    float vProj = Vector3DotProduct(vertexRelative, bitangent);

    // Map to 0-1 range with horizontal flip (U) for correct orientation
    float uRange = uMax - uMin;
    float vRange = vMax - vMin;
    float u = (uRange > 0.0f) ? 1.0f - ((uProj - uMin) / uRange) : 0.0f; // Horizontal flip
    float v = (vRange > 0.0f) ? ((vProj - vMin) / vRange) : 0.0f;         // No vertical flip

    return {u, v};
}

WorldGeometry::~WorldGeometry() = default;

WorldGeometry::WorldGeometry() {
    Initialize();
}

void WorldGeometry::Initialize() {
    bspTree = nullptr;
    batches.clear();
    materials.clear();
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
    materials.clear();
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
    if (!bspTree) return false;
    return bspTree->ContainsPoint(point);
}

float WorldGeometry::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    if (!bspTree) return maxDistance;
    return bspTree->CastRay(origin, direction, maxDistance);
}

// Surface-based queries removed (face-only pipeline)

const WorldMaterial* WorldGeometry::GetMaterial(int surfaceId) const {
    auto it = materials.find(surfaceId);
    return (it != materials.end()) ? &it->second : nullptr;
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
    std::vector<const Face*> visibleFaces;
    if (!bspTree) return visibleFaces;
    bspTree->TraverseForRenderingFaces(camera, visibleFaces);
    return visibleFaces;
}

void WorldGeometry::BuildBSPFromFaces(const std::vector<Face>& inFaces) {
    if (!bspTree) bspTree = std::make_unique<BSPTree>();
    faces = inFaces;
    bspTree->BuildFromFaces(inFaces);
    BuildBatchesFromFaces(faces);
    size_t triCount = 0; for (const auto& b : batches) triCount += b.indices.size()/3;
    LOG_INFO("WorldGeometry: built " + std::to_string(batches.size()) + " batches, ~" + std::to_string(triCount) + " tris");
}

void WorldGeometry::BuildBSPFromBrushes(const std::vector<Brush>& inBrushes) {
    if (!bspTree) bspTree = std::make_unique<BSPTree>();
    brushes = inBrushes;
    std::vector<Face> flat;
    for (const auto& b : brushes) {
        for (const auto& f : b.faces) flat.push_back(f);
    }
    faces = flat;
    bspTree->BuildFromFaces(flat);
    BuildBatchesFromFaces(faces);
    size_t triCount = 0; for (const auto& b : batches) triCount += b.indices.size()/3;
    LOG_INFO("WorldGeometry: built " + std::to_string(batches.size()) + " batches from brushes, ~" + std::to_string(triCount) + " tris");
}

void WorldGeometry::ClearBatches() {
    batches.clear();
}

void WorldGeometry::UpdateBatchColors() {
    LOG_INFO("Updating batch colors with loaded materials");
    
    // Update colors for all batches based on current material state
    for (auto& batch : batches) {
        const WorldMaterial* mat = GetMaterial(batch.materialId);
        
        // Update all colors in this batch
        for (size_t i = 0; i < batch.colors.size(); i++) {
            // Find the corresponding face tint (this is a simplification - we'll use white for textured)
            if (mat && mat->hasTexture) {
                batch.colors[i] = {255, 255, 255, 255}; // White for textured surfaces
                LOG_INFO("Updated batch color for material " + std::to_string(batch.materialId) + " to WHITE (textured)");
            }
            // Non-textured surfaces keep their original tint
        }
    }
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
    std::unordered_map<int, size_t> batchIndexByMaterial;
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

            for (size_t i = 0; i < 4; i++) {
                float u, vCoord;
                if (isHorizontal) {
                    float sizeX = maxVert.x - minVert.x;
                    float sizeZ = maxVert.z - minVert.z;
                    u = (sizeX > 0.0f) ? (v[i].x - minVert.x) / sizeX : 0.0f;
                    vCoord = (sizeZ > 0.0f) ? (v[i].z - minVert.z) / sizeZ : 0.0f;
                } else {
                    auto [calculatedU, calculatedV] = CalculateTangentSpaceUV(v[i], v, faceNormal, minVert, maxVert);
                    u = calculatedU;
                    vCoord = calculatedV;
                }
                batch.uvs.push_back({u, vCoord});
            }

            // Colors
            Color finalColor = f.tint;
            const WorldMaterial* mat = GetMaterial(f.materialId);
            if (mat && mat->hasTexture) {
                finalColor = {255, 255, 255, 255};
                LOG_INFO("BATCH COLOR: materialId=" + std::to_string(f.materialId) + " hasTexture=true, using WHITE");
            } else {
                LOG_INFO("BATCH COLOR: materialId=" + std::to_string(f.materialId) + " hasTexture=false, using tint (" +
                         std::to_string(f.tint.r) + "," + std::to_string(f.tint.g) + "," + std::to_string(f.tint.b) + ")");
            }

            for (int i = 0; i < 4; i++) {
                batch.colors.push_back(finalColor);
            }
        } else if (v.size() == 3) {
            // Convert triangle to degenerate quad by duplicating the last vertex
            // This ensures all geometry is rendered as quads
            unsigned int base = (unsigned int)batch.positions.size();

            // Add the 3 vertices + 1 duplicate
            for (size_t i = 0; i < 3; i++) {
                batch.positions.push_back(v[i]);
            }
            // Duplicate the last vertex to make it a quad
            batch.positions.push_back(v[2]);

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

            for (size_t i = 0; i < 3; i++) {
                float u, vCoord;
                if (isHorizontal) {
                    float sizeX = maxVert.x - minVert.x;
                    float sizeZ = maxVert.z - minVert.z;
                    u = (sizeX > 0.0f) ? (v[i].x - minVert.x) / sizeX : 0.0f;
                    vCoord = (sizeZ > 0.0f) ? (v[i].z - minVert.z) / sizeZ : 0.0f;
                } else {
                    auto [calculatedU, calculatedV] = CalculateTangentSpaceUV(v[i], v, faceNormal, minVert, maxVert);
                    u = calculatedU;
                    vCoord = calculatedV;
                }
                batch.uvs.push_back({u, vCoord});
            }
            // Duplicate UV for the last vertex
            batch.uvs.push_back(batch.uvs.back());

            // Colors
            Color finalColor = f.tint;
            const WorldMaterial* mat = GetMaterial(f.materialId);
            if (mat && mat->hasTexture) {
                finalColor = {255, 255, 255, 255};
                LOG_INFO("BATCH COLOR (triangle->quad): materialId=" + std::to_string(f.materialId) + " hasTexture=true, using WHITE");
            } else {
                LOG_INFO("BATCH COLOR (triangle->quad): materialId=" + std::to_string(f.materialId) + " hasTexture=false, using tint (" +
                         std::to_string(f.tint.r) + "," + std::to_string(f.tint.g) + "," + std::to_string(f.tint.b) + ")");
            }

            // Add 4 colors (duplicate the last one)
            for (int i = 0; i < 4; i++) {
                batch.colors.push_back(finalColor);
            }
        }
    }
}
