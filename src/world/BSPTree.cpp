#include "BSPTree.h"
#include "BSPTreeSystem.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <queue>

BSPTree::BSPTree() = default;

const AABB& BSPTree::GetClusterBounds(int32_t clusterId) const {
    static const AABB emptyBounds;

    if (clusterId < 0 || clusterId >= static_cast<int32_t>(clusters_.size())) {
        return emptyBounds;
    }

    return clusters_[clusterId].bounds;
}

void BSPTree::Clear() {
    root_.reset();
    clusters_.clear();
    pvsData_.reset();
    allFaces_.clear();
    visCount_ = 0;
}


// BSPTree is pure data - no rendering or drawing code
