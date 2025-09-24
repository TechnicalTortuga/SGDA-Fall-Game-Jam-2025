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
#include "Brush.h"
#include "../math/AABB.h"

// Forward declarations
struct BSPNode;

// Maximum number of clusters (adjust based on level size)
constexpr int32_t MAX_CLUSTERS = 4096;

// PVS (Potentially Visible Set) data structure
struct PVSData {
    int32_t numClusters = 0;
    std::vector<uint8_t> visibilityData;  // Compressed visibility data

    // Get visibility between two clusters
    bool IsVisible(int32_t fromCluster, int32_t toCluster) const {
        if (fromCluster < 0 || toCluster < 0 || fromCluster >= numClusters || toCluster >= numClusters) {
            return true;  // Default to visible for invalid clusters
        }

        // Simple bit vector implementation (1 bit per cluster)
        size_t byteIndex = (fromCluster * ((numClusters + 7) / 8)) + (toCluster / 8);
        uint8_t bitMask = 1 << (toCluster % 8);
        return (visibilityData[byteIndex] & bitMask) != 0;
    }
};

// Cluster information for a leaf node
struct BSPCluster {
    int32_t id = -1;
    AABB bounds;
    std::vector<BSPNode*> leafNodes;  // All leaf nodes in this cluster

    // For building PVS
    std::vector<Vector3> visibilityPoints;  // Points used for visibility testing

    bool IsValid() const { return id >= 0; }
};

// Quake-style BSP Node (unified node/leaf structure)
struct BSPNode {
    int contents;              // -1 for nodes, leaf contents for leaves
    int visframe;              // Visibility frame counter
    Vector3 mins, maxs;        // Bounding box
    BSPNode* parent;
    BSPNode* children[2];      // Node specific (nullptr for leaves)
    int cluster;               // Leaf specific (-1 for internal nodes)
    int area;                  // Leaf specific
    std::vector<size_t> surfaceIndices; // Indices into world->surfaces (leaf specific)

    BSPNode() : contents(-1), visframe(0), parent(nullptr), cluster(-1), area(0) {
        children[0] = children[1] = nullptr;
        mins = maxs = Vector3{0, 0, 0};
    }

    bool IsLeaf() const { return contents != -1; }
};

// Binary Space Partitioning tree data component
// Just contains the tree structure and data, all logic is in BSPTreeSystem
class BSPTree {
public:
    BSPTree();
    ~BSPTree() = default;

    // Get the number of clusters in the PVS
    int32_t GetClusterCount() const { return static_cast<int32_t>(clusters_.size()); }

    // Get the bounds of a cluster
    const AABB& GetClusterBounds(int32_t clusterId) const;

    // Get all faces in the tree
    const std::vector<Face>& GetAllFaces() const { return allFaces_; }

    // Clear the BSP tree
    void Clear();

    // --- Data members (public for BSPTreeSystem access) ---
    std::vector<BSPCluster> clusters_;
    std::shared_ptr<PVSData> pvsData_;

    // Visibility marking system (like Quake's visframe)
    int32_t visCount_ = 0;  // Current visibility frame counter

    // Tree structure
    std::unique_ptr<BSPNode> root_;
    std::vector<Face> allFaces_;

private:
    // BSPTree is pure data - no rendering or drawing code
};
