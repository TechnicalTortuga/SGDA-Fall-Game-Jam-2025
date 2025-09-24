#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "raylib.h"
#include "raymath.h"
#include "BSPTree.h"
#include "WorldGeometry.h" // For World struct
#include "../math/AABB.h"
#include "../ecs/System.h"

// Forward declarations
struct BSPNode;

// Frustum plane for culling (like Quake 3's cplane_s)
struct FrustumPlane {
    Vector3 normal;
    float dist;
    int type;        // For axial plane optimization
    int signbits;    // For fast distance calculation
};

// Complete frustum with 6 planes
struct Frustum {
    FrustumPlane planes[6]; // left, right, bottom, top, near, far
    int numPlanes = 6;
};


// Quake-style World Geometry System
// Implements the complete Quake 3 BSP/PVS/Rendering pipeline
class BSPTreeSystem : public System {
public:
    BSPTreeSystem();
    ~BSPTreeSystem() = default;

    // System interface implementation
    void Update(float deltaTime) override;
    void Initialize() override;
    void Shutdown() override;

    // === QUAKE-STYLE WORLD LOADING ===
    // Load and build world from parsed map data
    std::unique_ptr<World> LoadWorld(const std::vector<Face>& faces);

    // === QUAKE-STYLE VISIBILITY SYSTEM ===

    // Mark leaves visible from current camera position (R_MarkLeaves equivalent)
    void MarkLeaves(World& world, const Vector3& cameraPosition);

    // Traverse world and render visible surfaces (R_RecursiveWorldNode equivalent)
    void TraverseForRendering(const World& world, const Camera3D& camera,
                            std::function<void(const Face& face)> faceCallback);

    // === UTILITY FUNCTIONS ===

    // Find which leaf contains a point (R_PointInLeaf equivalent)
    const BSPNode* FindLeafForPoint(const World& world, const Vector3& point) const;

    // TEMPORARY: Legacy methods for compatibility during transition
    float CastRay(const BSPTree& bspTree, const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance = 1000.0f) const;
    bool ContainsPoint(const BSPTree& bspTree, const Vector3& point) const;

private:
    // === BSP CONSTRUCTION (Quake-style) ===
    std::unique_ptr<BSPNode> BuildBSPTree(const std::vector<Face>& faces, std::vector<Face>& outSurfaces);

    // === PVS GENERATION ===
    void BuildClustersFromLeaves(World& world);
    void GeneratePVSData(World& world);
    bool TestClusterVisibility(const World& world, int clusterA, int clusterB) const;
    bool TestLineOfSight(const World& world, const Vector3& start, const Vector3& end) const;
    bool IsAABBVisibleInFrustum(const Vector3& mins, const Vector3& maxs, const Frustum& frustum) const;

    // === VISIBILITY MARKING ===
    const uint8_t* GetClusterPVS(const World& world, int cluster) const;

    // === BSP TREE BUILDING HELPERS ===
    std::unique_ptr<BSPNode> BuildBSPRecursive(const std::vector<size_t>& faceIndices,
                                             const std::vector<Face>& allFaces,
                                             int depth = 0);
    size_t ChooseSplitterFace(const std::vector<size_t>& faceIndices,
                            const std::vector<Face>& allFaces) const;

    // === PLANE AND FRUSTUM UTILITIES ===
    struct Plane { Vector3 n; float d; };
    Plane PlaneFromFace(const Face& face);
    float SignedDistanceToPlane(const Plane& p, const Vector3& point) const;
    int ClassifyFace(const Face& face, const Plane& plane) const;
    void SplitFaceByPlane(const Face& face, const Plane& plane,
                         bool& hasFront, Face& outFront,
                         bool& hasBack, Face& outBack) const;

    // === FRUSTUM CULLING ===
    void ExtractFrustumPlanes(Frustum& frustum, const Camera3D& camera) const;
    void SetPlaneSignbits(FrustumPlane& plane) const;
    int BoxOnPlaneSide(const Vector3& mins, const Vector3& maxs, const FrustumPlane& plane) const;

    // === LEGACY METHODS (to be removed) ===
    // Old BSPTree-based methods for backward compatibility during transition
    void BuildClusters(BSPTree& bspTree);
    AABB ComputeClusterBounds(const std::vector<BSPNode*>& leafNodes);

    // Visibility frame counter (like Quake's visCount)
    int32_t visCount_ = 0;

    // Far clip distance for frustum culling
    float farClipDistance_ = 100.0f;
};
