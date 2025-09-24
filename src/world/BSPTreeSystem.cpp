#include "BSPTreeSystem.h"
#include "BSPTree.h"
#include "../math/AABB.h"
#include "Brush.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <vector>
#include <memory>
#include <functional>
#include <string>

#ifndef DEG2RAD
#define DEG2RAD (PI/180.0f)
#endif

// Helper function to compute AABB from face indices
AABB ComputeBoundsFromFaceIndices(const std::vector<size_t>& faceIndices,
                                const std::vector<Face>& allFaces) {
    if (faceIndices.empty()) return AABB(Vector3{0,0,0}, Vector3{0,0,0});

    Vector3 minBounds = allFaces[faceIndices[0]].vertices[0];
    Vector3 maxBounds = minBounds;

    for (size_t faceIdx : faceIndices) {
        const Face& face = allFaces[faceIdx];
        for (const auto& vertex : face.vertices) {
            minBounds.x = std::min(minBounds.x, vertex.x);
            minBounds.y = std::min(minBounds.y, vertex.y);
            minBounds.z = std::min(minBounds.z, vertex.z);
            maxBounds.x = std::max(maxBounds.x, vertex.x);
            maxBounds.y = std::max(maxBounds.y, vertex.y);
            maxBounds.z = std::max(maxBounds.z, vertex.z);
        }
    }
    return AABB(minBounds, maxBounds);
}

BSPTreeSystem::BSPTreeSystem() : visCount_(0) {
    LOG_INFO("BSPTreeSystem created");
}

void BSPTreeSystem::Update(float deltaTime) {
    // BSPTreeSystem doesn't need per-frame updates
    // All operations are on-demand (building trees, PVS queries, etc.)
}

void BSPTreeSystem::Initialize() {
    LOG_INFO("BSPTreeSystem initialized");
}

void BSPTreeSystem::Shutdown() {
    LOG_INFO("BSPTreeSystem shutdown");
}

// === QUAKE-STYLE WORLD LOADING ===

std::unique_ptr<World> BSPTreeSystem::LoadWorld(const std::vector<Face>& faces) {
    LOG_INFO("=== BSPTreeSystem::LoadWorld called with " + std::to_string(faces.size()) + " faces ===");

    if (faces.empty()) {
        LOG_WARNING("LoadWorld: No faces provided, returning null");
        return nullptr;
    }

    auto world = std::make_unique<World>();
    world->name = "world";
    world->surfaces = faces; // Store all faces in global array

    // Build BSP tree from faces
    world->nodes.push_back(BuildBSPTree(faces, world->surfaces));
    world->root = world->nodes.back().get();

    if (!world->root) {
        LOG_ERROR("Failed to build BSP tree");
        return nullptr;
    }

    // Build clusters from leaves
    BuildClustersFromLeaves(*world);

    // Generate PVS data
    GeneratePVSData(*world);

    LOG_INFO("World loaded successfully:");
    LOG_INFO("  - " + std::to_string(world->surfaces.size()) + " surfaces");
    LOG_INFO("  - " + std::to_string(world->nodes.size()) + " BSP nodes");
    LOG_INFO("  - " + std::to_string(world->numClusters) + " clusters");

    return world;
}

// === BSP Tree Building Implementation ===

std::unique_ptr<BSPNode> BSPTreeSystem::BuildBSPTree(const std::vector<Face>& faces, std::vector<Face>& outSurfaces) {
    LOG_INFO("Building BSP tree from " + std::to_string(faces.size()) + " faces");

    // Store faces in global array and create indices
    outSurfaces = faces;
    std::vector<size_t> faceIndices(faces.size());
    for (size_t i = 0; i < faces.size(); ++i) {
        faceIndices[i] = i;
    }

    // Build BSP tree recursively
    auto root = BuildBSPRecursive(faceIndices, outSurfaces);

    LOG_INFO("BSP tree built with " + std::to_string(outSurfaces.size()) + " surfaces");
    return root;
}

std::unique_ptr<BSPNode> BSPTreeSystem::BuildBSPRecursive(const std::vector<size_t>& faceIndices,
                                                        const std::vector<Face>& allFaces,
                                                        int depth) {
    // Simplified: just create a leaf node for now
    auto node = std::make_unique<BSPNode>();
    node->contents = 0; // Leaf
    node->visframe = 0;
    node->cluster = -1;
    node->area = 0;
    node->surfaceIndices = faceIndices; // Store all faces in this leaf

    // Compute bounds from faces
    if (!faceIndices.empty()) {
        auto bounds = ComputeBoundsFromFaceIndices(faceIndices, allFaces);
        node->mins = bounds.min;
        node->maxs = bounds.max;
    } else {
        node->mins = node->maxs = Vector3{0, 0, 0};
    }

    LOG_DEBUG("Created leaf with " + std::to_string(faceIndices.size()) + " faces");
    return node;
}

size_t BSPTreeSystem::ChooseSplitterFace(const std::vector<size_t>& faceIndices,
                                       const std::vector<Face>& allFaces) const {
    // Simple heuristic: choose first face as splitter
    if (!faceIndices.empty()) {
        return 0;
    }
    return faceIndices.size(); // Invalid index
}

BSPTreeSystem::Plane BSPTreeSystem::PlaneFromFace(const Face& face) {
    if (face.vertices.size() < 3) {
        return Plane{Vector3{0, 1, 0}, 0}; // Default up-facing plane
    }

    // Compute plane from first three vertices
    Vector3 v1 = face.vertices[1] - face.vertices[0];
    Vector3 v2 = face.vertices[2] - face.vertices[0];
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(v1, v2));
    float d = -Vector3DotProduct(normal, face.vertices[0]);

    return Plane{normal, d};
}

float BSPTreeSystem::SignedDistanceToPlane(const Plane& p, const Vector3& point) const {
    return Vector3DotProduct(p.n, point) + p.d;
}

int BSPTreeSystem::ClassifyFace(const Face& face, const Plane& plane) const {
    int inFront = 0, behind = 0, onPlane = 0;
    const float EPS = 1e-5f;

    for (const auto& v : face.vertices) {
        float dist = SignedDistanceToPlane(plane, v);
        if (dist > EPS) inFront++;
        else if (dist < -EPS) behind++;
        else onPlane++;
    }

    if (inFront > 0 && behind > 0) return 0; // spanning
    if (inFront > 0) return 1; // front
    if (behind > 0) return -1; // back
    if (onPlane == (int)face.vertices.size()) return 2; // coplanar
    return 0; // on-plane but not all vertices
}

void BSPTreeSystem::SplitFaceByPlane(const Face& face, const Plane& plane,
                                   bool& hasFront, Face& outFront,
                                   bool& hasBack, Face& outBack) const {
    hasFront = false; hasBack = false;
    std::vector<Vector3> frontVerts, backVerts;
    const float EPS = 1e-5f;

    size_t count = face.vertices.size();
    if (count < 3) return;

    for (size_t i = 0; i < count; ++i) {
        const Vector3& a = face.vertices[i];
        const Vector3& b = face.vertices[(i + 1) % count];
        float da = SignedDistanceToPlane(plane, a);
        float db = SignedDistanceToPlane(plane, b);

        if (da >= -EPS) frontVerts.push_back(a);
        if (da <= EPS) backVerts.push_back(a);

        // Check for edge intersection
        if ((da > EPS && db < -EPS) || (da < -EPS && db > EPS)) {
            // Edge crosses plane, compute intersection
            float t = da / (da - db);
            Vector3 intersection = Vector3Add(a, Vector3Scale(Vector3Subtract(b, a), t));
            frontVerts.push_back(intersection);
            backVerts.push_back(intersection);
        }
    }

    // Create output faces if we have enough vertices
    if (frontVerts.size() >= 3) {
        hasFront = true;
        outFront = face;
        outFront.vertices = frontVerts;
    }

    if (backVerts.size() >= 3) {
        hasBack = true;
        outBack = face;
        outBack.vertices = backVerts;
    }
}

// === PVS AND CLUSTERING ===

void BSPTreeSystem::BuildClustersFromLeaves(World& world) {
    LOG_INFO("Building clusters from leaves");

    // Collect all leaf nodes
    std::vector<BSPNode*> leaves;
    std::function<void(BSPNode*)> collectLeaves = [&](BSPNode* node) {
        if (!node) return;
        if (node->IsLeaf()) {
            leaves.push_back(node);
        } else {
            collectLeaves(node->children[0]);
            collectLeaves(node->children[1]);
        }
    };
    collectLeaves(world.root);

    LOG_INFO("Found " + std::to_string(leaves.size()) + " leaves");

    // Assign cluster IDs to leaves (simplified: each leaf is its own cluster for now)
    world.numClusters = leaves.size();
    for (size_t i = 0; i < leaves.size(); ++i) {
        leaves[i]->cluster = static_cast<int>(i);
    }

    // Calculate cluster bytes for PVS (1 bit per cluster)
    world.clusterBytes = (world.numClusters + 7) / 8;
}

void BSPTreeSystem::GeneratePVSData(World& world) {
    LOG_INFO("Generating PVS data for " + std::to_string(world.numClusters) + " clusters");

    // Allocate PVS data
    size_t pvsSize = world.numClusters * world.clusterBytes;
    world.visData.resize(pvsSize, 0);

    // For now, make all clusters visible to all others (no PVS culling)
    // TODO: Implement proper portal-based PVS generation
    for (int clusterA = 0; clusterA < world.numClusters; ++clusterA) {
        for (int clusterB = 0; clusterB < world.numClusters; ++clusterB) {
            // Set bit for clusterB in clusterA's PVS
            int byteIndex = clusterA * world.clusterBytes + (clusterB / 8);
            int bitIndex = clusterB % 8;
            world.visData[byteIndex] |= (1 << bitIndex);
        }
    }

    LOG_INFO("PVS data generated (" + std::to_string(pvsSize) + " bytes)");
}

const uint8_t* BSPTreeSystem::GetClusterPVS(const World& world, int cluster) const {
    if (cluster < 0 || cluster >= world.numClusters) {
        return nullptr;
    }
    return &world.visData[cluster * world.clusterBytes];
}

// === VISIBILITY MARKING ===

void BSPTreeSystem::MarkLeaves(World& world, const Vector3& cameraPosition) {
    visCount_++;

    // Find which leaf the camera is in
    const BSPNode* cameraLeaf = FindLeafForPoint(world, cameraPosition);
    if (!cameraLeaf || cameraLeaf->cluster < 0) {
        // Camera not in a valid cluster, mark all nodes visible
        std::function<void(BSPNode*)> markAllVisible = [&](BSPNode* node) {
            if (!node) return;
            node->visframe = visCount_;
            if (!node->IsLeaf()) {
                markAllVisible(node->children[0]);
                markAllVisible(node->children[1]);
            }
        };
        markAllVisible(world.root);
        return;
    }

    int cameraCluster = cameraLeaf->cluster;

    // Get PVS for camera cluster
    const uint8_t* pvs = GetClusterPVS(world, cameraCluster);
    if (!pvs) {
        // No PVS data, mark all visible
        std::function<void(BSPNode*)> markAllVisible = [&](BSPNode* node) {
            if (!node) return;
            node->visframe = visCount_;
            if (!node->IsLeaf()) {
                markAllVisible(node->children[0]);
                markAllVisible(node->children[1]);
            }
        };
        markAllVisible(world.root);
        return;
    }

    // Mark visible nodes (like Quake's R_MarkLeaves)
    std::function<void(BSPNode*)> markVisibleNodes = [&](BSPNode* node) {
        if (!node) return;
        
        // Check if this node is visible
        if (node->IsLeaf()) {
            // Check if leaf's cluster is visible
            if (node->cluster >= 0 && node->cluster < world.numClusters) {
                int byteIndex = node->cluster / 8;
                int bitIndex = node->cluster % 8;
                if (pvs[byteIndex] & (1 << bitIndex)) {
                    // Mark path from this leaf to root
                    BSPNode* current = node;
                    while (current) {
                        if (current->visframe == visCount_) break;
                        current->visframe = visCount_;
                        current = current->parent;
                    }
                }
            }
        } else {
            // Recurse to children first
            markVisibleNodes(node->children[0]);
            markVisibleNodes(node->children[1]);

            // If both children are marked, mark this node too
            if ((node->children[0] && node->children[0]->visframe == visCount_) ||
                (node->children[1] && node->children[1]->visframe == visCount_)) {
                node->visframe = visCount_;
            }
        }
    };

    markVisibleNodes(world.root);
}

// === RENDERING TRAVERSAL ===

void BSPTreeSystem::TraverseForRendering(const World& world, const Camera3D& camera,
                                       std::function<void(const Face& face)> faceCallback) {
    if (!world.root) return;

    Frustum frustum;
    ExtractFrustumPlanes(frustum, camera);

    std::function<void(BSPNode*)> traverseNode = [&](BSPNode* node) {
    if (!node) return;

        // PVS culling first
        if (node->visframe != visCount_) return;

        // TEMPORARILY DISABLE FRUSTUM CULLING FOR DEBUGGING
        // Frustum culling - check against all 6 frustum planes
        // bool isVisible = true;
        // for (int i = 0; i < 6; ++i) {
        //     int cullResult = BoxOnPlaneSide(node->mins, node->maxs, frustum.planes[i]);
        //     if (cullResult == 2) { // Completely behind this plane
        //         isVisible = false;
        //         break;
        //     }
        // }
        // if (!isVisible) return;

    if (node->IsLeaf()) {
            // Render all surfaces in this leaf
            for (size_t surfaceIdx : node->surfaceIndices) {
                if (surfaceIdx < world.surfaces.size()) {
                    faceCallback(world.surfaces[surfaceIdx]);
                }
            }
    } else {
            // Recurse to children
            traverseNode(node->children[0]);
            traverseNode(node->children[1]);
        }
    };

    traverseNode(world.root);
}

// === UTILITY FUNCTIONS ===

const BSPNode* BSPTreeSystem::FindLeafForPoint(const World& world, const Vector3& point) const {
    BSPNode* node = world.root;

    while (node && !node->IsLeaf()) {
        // Determine which side of the plane the point is on
        // For now, use a simple heuristic since we don't store plane equations
        // TODO: Store plane equations in nodes
        float dist = 0.0f; // Would compute signed distance to plane

        if (dist >= 0) {
            node = node->children[0];
        } else {
            node = node->children[1];
        }
    }

    return node;
}

// === TEMPORARY LEGACY METHODS (for compatibility) ===

float BSPTreeSystem::CastRay(const BSPTree& bspTree, const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance) const {
    // TODO: Implement proper ray casting for legacy BSPTree
    return maxDistance; // No hit
}

bool BSPTreeSystem::ContainsPoint(const BSPTree& bspTree, const Vector3& point) const {
    // TODO: Implement proper point containment for legacy BSPTree
    return false;
}

// === FRUSTUM CULLING IMPLEMENTATION ===

void BSPTreeSystem::ExtractFrustumPlanes(Frustum& frustum, const Camera3D& camera) const {
    // Based on Quake 3's R_SetupFrustum
    float angle = camera.fovy * DEG2RAD * 0.5f;
    float tang = tanf(angle);
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();

    float nearHeight = tang * 0.1f; // Near plane distance (like raylib default)
    float nearWidth = nearHeight * aspect;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);

    Vector3 nearCenter = Vector3Add(camera.position, Vector3Scale(forward, 0.1f)); // Near plane at 0.1 units

    // Left plane
    Vector3 leftNormal = Vector3Normalize(Vector3Add(
        Vector3Scale(forward, nearWidth),
        Vector3Scale(right, -1.0f)
    ));
    frustum.planes[0].normal = leftNormal;
    frustum.planes[0].dist = -Vector3DotProduct(leftNormal, Vector3Add(nearCenter, Vector3Scale(right, -nearWidth)));

    // Right plane
    Vector3 rightNormal = Vector3Normalize(Vector3Add(
        Vector3Scale(forward, -nearWidth),
        right
    ));
    frustum.planes[1].normal = rightNormal;
    frustum.planes[1].dist = -Vector3DotProduct(rightNormal, Vector3Add(nearCenter, Vector3Scale(right, nearWidth)));

    // Bottom plane
    Vector3 bottomNormal = Vector3Normalize(Vector3Add(
        Vector3Scale(forward, nearHeight),
        Vector3Scale(up, -1.0f)
    ));
    frustum.planes[2].normal = bottomNormal;
    frustum.planes[2].dist = -Vector3DotProduct(bottomNormal, Vector3Add(nearCenter, Vector3Scale(up, -nearHeight)));

    // Top plane
    Vector3 topNormal = Vector3Normalize(Vector3Add(
        Vector3Scale(forward, -nearHeight),
        up
    ));
    frustum.planes[3].normal = topNormal;
    frustum.planes[3].dist = -Vector3DotProduct(topNormal, Vector3Add(nearCenter, Vector3Scale(up, nearHeight)));

    // Near plane
    frustum.planes[4].normal = forward;
    frustum.planes[4].dist = -Vector3DotProduct(forward, nearCenter);

    // Far plane
    frustum.planes[5].normal = Vector3Scale(forward, -1.0f);
    frustum.planes[5].dist = -Vector3DotProduct(Vector3Scale(forward, -1.0f),
                                               Vector3Add(camera.position, Vector3Scale(forward, farClipDistance_)));

    // Set signbits for optimization
    for (int i = 0; i < 6; i++) {
        SetPlaneSignbits(frustum.planes[i]);
    }
}

void BSPTreeSystem::SetPlaneSignbits(FrustumPlane& plane) const {
    // Based on Quake 3's BoxOnPlaneSide optimization
    plane.signbits = 0;
    if (plane.normal.x < 0) plane.signbits |= 1;
    if (plane.normal.y < 0) plane.signbits |= 2;
    if (plane.normal.z < 0) plane.signbits |= 4;

    // Determine axial type
    if (plane.normal.x == 1.0f || plane.normal.x == -1.0f) plane.type = 0; // X axis
    else if (plane.normal.y == 1.0f || plane.normal.y == -1.0f) plane.type = 1; // Y axis
    else if (plane.normal.z == 1.0f || plane.normal.z == -1.0f) plane.type = 2; // Z axis
    else plane.type = 3; // Non-axial
}

int BSPTreeSystem::BoxOnPlaneSide(const Vector3& mins, const Vector3& maxs, const FrustumPlane& plane) const {
    // Based on Quake 3's BoxOnPlaneSide function
    float dist1, dist2;
    int sides = 0;

    // Fast axial cases
    switch (plane.type) {
        case 0: // Axial X
            dist1 = plane.normal.x * mins.x + plane.dist;
            dist2 = plane.normal.x * maxs.x + plane.dist;
            break;
        case 1: // Axial Y
            dist1 = plane.normal.y * mins.y + plane.dist;
            dist2 = plane.normal.y * maxs.y + plane.dist;
            break;
        case 2: // Axial Z
            dist1 = plane.normal.z * mins.z + plane.dist;
            dist2 = plane.normal.z * maxs.z + plane.dist;
            break;
        default: // General case
            // Use the signbits to optimize
            if (plane.signbits & 1) dist1 = plane.normal.x * maxs.x + plane.normal.y * mins.y + plane.normal.z * mins.z + plane.dist;
            else dist1 = plane.normal.x * mins.x + plane.normal.y * mins.y + plane.normal.z * mins.z + plane.dist;

            if (plane.signbits & 2) dist2 = plane.normal.x * mins.x + plane.normal.y * maxs.y + plane.normal.z * mins.z + plane.dist;
            else dist2 = plane.normal.x * mins.x + plane.normal.y * mins.y + plane.normal.z * mins.z + plane.dist;

            if (plane.signbits & 4) dist2 += plane.normal.z * (maxs.z - mins.z);
            else dist1 += plane.normal.z * (maxs.z - mins.z);
            break;
    }

    if (dist1 >= 0) sides = 1;
    if (dist2 < 0) sides |= 2;

    return sides;
}

bool BSPTreeSystem::IsAABBVisibleInFrustum(const Vector3& mins, const Vector3& maxs, const Frustum& frustum) const {
    // Test AABB against all 6 frustum planes
    for (int i = 0; i < 6; ++i) {
        int result = BoxOnPlaneSide(mins, maxs, frustum.planes[i]);
        if (result == 2) { // Completely behind this plane
            return false;
        }
    }
    return true; // AABB is visible (not completely behind any plane)
}

bool BSPTreeSystem::TestClusterVisibility(const World& world, int clusterA, int clusterB) const {
    // For now, implement simple visibility: all clusters can see each other
    // TODO: Implement proper PVS checking
    return true;
}

bool BSPTreeSystem::TestLineOfSight(const World& world, const Vector3& start, const Vector3& end) const {
    // TODO: Implement proper line-of-sight testing through BSP tree
    // For now, always return true
    return true;
}
