#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <cfloat>
#include "raylib.h"
#include "raymath.h"
#include "Brush.h"
#include "../math/AABB.h"

// Forward declarations
struct BSPNode;

// A node in the BSP tree
struct BSPNode {
    std::unique_ptr<BSPNode> front;    // Front child (relative to splitter)
    std::unique_ptr<BSPNode> back;     // Back child (relative to splitter)
    std::vector<Face> faces;           // Faces in this node (brush-based pipeline)
    Vector3 planeNormal;               // Splitter plane normal (face-based builds)
    float planeDistance = 0.0f;        // Splitter plane distance from origin (dot(n, x) - d = 0)
    AABB bounds;                       // Subtree bounds for frustum culling

    BSPNode() = default;

    bool IsLeaf() const { return !front && !back; }
};

// Binary Space Partitioning tree for efficient 3D rendering and collision detection
class BSPTree {
public:
    BSPTree();
    ~BSPTree() = default;
    // Build BSP from faces (brush-based pipeline)
    void BuildFromFaces(const std::vector<Face>& faces);
    // Convenience: build BSP from brushes by flattening to faces
    void BuildFromBrushes(const std::vector<Brush>& brushes);

    // Traverse and collect visible faces (brush-based)
    void TraverseForRenderingFaces(const Camera3D& camera, std::vector<const Face*>& visibleFaces) const;

    // Perform ray casting for collision detection
    // rayOrigin: Ray origin
    // rayDirection: Ray direction (normalized)
    // maxDistance: Maximum distance to check
    // Returns: Distance to hit, or maxDistance if no hit
    float CastRay(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance = 1000.0f) const;

    // Perform ray casting for collision detection with surface normal
    // rayOrigin: Ray origin
    // rayDirection: Ray direction (normalized)
    // maxDistance: Maximum distance to check
    // hitNormal: Output parameter for surface normal at hit point
    // Returns: Distance to hit, or maxDistance if no hit
    float CastRayWithNormal(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance, Vector3& hitNormal) const;

    // Check if a point is inside the BSP tree bounds
    // point: Point to check
    // Returns: True if point is contained within the tree
    bool ContainsPoint(const Vector3& point) const;

    // Get all faces in the tree
    const std::vector<Face>& GetAllFaces() const { return allFaces_; }

    // Clear the BSP tree
    void Clear();

private:
    std::unique_ptr<BSPNode> root_;
    std::vector<Face> allFaces_;

    std::unique_ptr<BSPNode> BuildRecursiveFaces(std::vector<Face> faces);

    size_t ChooseSplitterFaces(const std::vector<Face>& faces) const;

    // Plane representation for face-based build
    struct Plane { Vector3 n; float d; };
    static Plane PlaneFromFace(const Face& face);
    static float SignedDistanceToPlane(const Plane& p, const Vector3& point);
    int ClassifyFace(const Face& face, const Plane& plane) const; // -1 back, 0 spanning/on, 1 front

    // Split a convex face by plane into front/back pieces. Returns which sides are produced.
    void SplitFaceByPlane(const Face& face, const Plane& plane,
                          bool& hasFront, Face& outFront,
                          bool& hasBack, Face& outBack) const;

    void TraverseRenderRecursiveCamera3D_Faces(const BSPNode* node, const Camera3D& camera,
                                               std::vector<const Face*>& visibleFaces) const;

    // Recursively cast ray
    // node: Current node
    // rayOrigin: Ray origin
    // rayDirection: Ray direction
    // maxDistance: Maximum distance
    // Returns: Hit distance
    float CastRayRecursive(const BSPNode* node, const Vector3& rayOrigin,
                          const Vector3& rayDirection, float maxDistance) const;

    // Recursively cast ray with surface normal calculation
    // node: Current node
    // rayOrigin: Ray origin
    // rayDirection: Ray direction
    // maxDistance: Maximum distance
    // hitNormal: Output parameter for surface normal at hit point
    // Returns: Hit distance
    float CastRayRecursiveWithNormal(const BSPNode* node, const Vector3& rayOrigin,
                                    const Vector3& rayDirection, float maxDistance, Vector3& hitNormal) const;

    bool IsFaceVisible(const Face& face, const Camera3D& camera) const;

    // View frustum helpers
    bool IsPointInViewFrustum(const Vector3& point, const Camera3D& camera) const;
    bool IsAABBInViewFrustum(const AABB& box, const Camera3D& camera) const;
    bool SubtreeInViewFrustum(const BSPNode* node, const Camera3D& camera) const;

    // Bounds helpers
    static AABB ComputeBoundsForFaces(const std::vector<Face>& faces);
    static void UpdateNodeBounds(BSPNode* node);
};
