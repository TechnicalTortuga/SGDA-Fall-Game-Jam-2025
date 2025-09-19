#pragma once

#include <vector>
#include <memory>
#include "raylib.h"
#include "raymath.h"

// Forward declarations
struct Surface;
struct BSPNode;

// Represents a surface (wall, floor, or ceiling) in the BSP tree
struct Surface {
    Vector3 start;           // Start position of the surface
    Vector3 end;             // End position of the surface
    float height;            // Height of the surface (for walls)
    float floorHeight;       // Floor height (for sectors)
    float ceilingHeight;     // Ceiling height (for sectors)
    Color color;             // Color/texture index
    int textureIndex;        // Index into texture atlas
    bool isWall;             // True for walls, false for floors/ceilings
    bool isFloor;            // True for floors
    bool isCeiling;          // True for ceilings

    Surface() = default;
    Surface(const Vector3& s, const Vector3& e, float h = 0.0f, Color c = GRAY)
        : start(s), end(e), height(h), floorHeight(0.0f), ceilingHeight(0.0f),
          color(c), textureIndex(0), isWall(true), isFloor(false), isCeiling(false) {}

    // Calculate normal vector for the surface
    Vector3 GetNormal() const {
        if (isWall) {
            Vector3 direction = Vector3Subtract(end, start);
            return Vector3Normalize(Vector3{-direction.z, 0.0f, direction.x});
        }
        return {0.0f, isFloor ? 1.0f : -1.0f, 0.0f}; // Up for floors, down for ceilings
    }

    // Get distance from origin to plane
    float GetDistance() const {
        Vector3 normal = GetNormal();
        return Vector3DotProduct(normal, start);
    }
};

// A node in the BSP tree
struct BSPNode {
    std::unique_ptr<BSPNode> front;    // Front child (relative to splitter)
    std::unique_ptr<BSPNode> back;     // Back child (relative to splitter)
    std::vector<Surface> surfaces;     // Surfaces in this leaf node
    Surface splitter;                  // Splitting plane for this node

    BSPNode() = default;
    BSPNode(const Surface& split) : splitter(split) {}

    bool IsLeaf() const { return !front && !back; }
};

// Binary Space Partitioning tree for efficient 3D rendering and collision detection
class BSPTree {
public:
    BSPTree();
    ~BSPTree() = default;

    // Build the BSP tree from a list of surfaces
    // surfaces: List of surfaces to partition
    void Build(const std::vector<Surface>& surfaces);

    // Traverse the tree and collect visible surfaces for rendering
    // camera: Camera position for visibility determination
    // visibleSurfaces: Output vector for visible surfaces
    void TraverseForRendering(const Vector3& camera, std::vector<const Surface*>& visibleSurfaces) const;

    // Perform ray casting for collision detection
    // rayOrigin: Ray origin
    // rayDirection: Ray direction (normalized)
    // maxDistance: Maximum distance to check
    // Returns: Distance to hit, or maxDistance if no hit
    float CastRay(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance = 1000.0f) const;

    // Check if a point is inside the BSP tree bounds
    // point: Point to check
    // Returns: True if point is contained within the tree
    bool ContainsPoint(const Vector3& point) const;

    // Get all surfaces in the tree
    // Returns: Vector of all surfaces
    const std::vector<Surface>& GetAllSurfaces() const { return allSurfaces_; }

    // Clear the BSP tree
    void Clear();

private:
    std::unique_ptr<BSPNode> root_;
    std::vector<Surface> allSurfaces_;

    // Recursively build the BSP tree
    // surfaces: Surfaces to partition
    // Returns: Root node of the subtree
    std::unique_ptr<BSPNode> BuildRecursive(std::vector<Surface> surfaces);

    // Choose the best splitter surface from a list
    // surfaces: Available surfaces
    // Returns: Index of the best splitter surface
    size_t ChooseSplitter(const std::vector<Surface>& surfaces) const;

    // Classify a surface relative to a splitter plane
    // surface: Surface to classify
    // splitter: Splitter plane
    // Returns: -1 (back), 0 (on plane), 1 (front)
    int ClassifySurface(const Surface& surface, const Surface& splitter) const;

    // Split a surface by a plane
    // surface: Surface to split
    // splitter: Splitting plane
    // front: Output front piece
    // back: Output back piece
    // Returns: True if surface was split, false if on plane
    bool SplitSurface(const Surface& surface, const Surface& splitter,
                     Surface& front, Surface& back) const;

    // Recursively traverse for rendering
    // node: Current node
    // camera: Camera position
    // visibleSurfaces: Output vector
    void TraverseRenderRecursive(const BSPNode* node, const Vector3& camera,
                                std::vector<const Surface*>& visibleSurfaces) const;

    // Recursively cast ray
    // node: Current node
    // rayOrigin: Ray origin
    // rayDirection: Ray direction
    // maxDistance: Maximum distance
    // Returns: Hit distance
    float CastRayRecursive(const BSPNode* node, const Vector3& rayOrigin,
                          const Vector3& rayDirection, float maxDistance) const;

    // Check if surface is visible from camera position
    // surface: Surface to check
    // camera: Camera position
    // Returns: True if potentially visible
    bool IsSurfaceVisible(const Surface& surface, const Vector3& camera) const;
};
