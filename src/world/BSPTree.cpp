#include "BSPTree.h"
#include <algorithm>
#include <cmath>
#include <limits>

BSPTree::BSPTree() : root_(nullptr) {}

void BSPTree::Build(const std::vector<Surface>& surfaces) {
    allSurfaces_ = surfaces;
    root_ = BuildRecursive(surfaces);
}

std::unique_ptr<BSPNode> BSPTree::BuildRecursive(std::vector<Surface> surfaces) {
    if (surfaces.empty()) {
        return nullptr;
    }

    // Create a new node
    auto node = std::make_unique<BSPNode>();

    // If only one surface left, make it a leaf
    if (surfaces.size() == 1) {
        node->surfaces = surfaces;
        return node;
    }

    // Choose the best splitter
    size_t splitterIndex = ChooseSplitter(surfaces);
    node->splitter = surfaces[splitterIndex];

    // Remove splitter from surfaces list
    surfaces.erase(surfaces.begin() + splitterIndex);

    // Partition remaining surfaces
    std::vector<Surface> frontSurfaces;
    std::vector<Surface> backSurfaces;

    for (const auto& surface : surfaces) {
        int classification = ClassifySurface(surface, node->splitter);

        if (classification == 0) {
            // Surface is on the plane, add to current node
            node->surfaces.push_back(surface);
        } else if (classification > 0) {
            // Surface is in front
            frontSurfaces.push_back(surface);
        } else {
            // Surface is behind
            backSurfaces.push_back(surface);
        }
    }

    // Recursively build child nodes
    if (!frontSurfaces.empty()) {
        node->front = BuildRecursive(frontSurfaces);
    }
    if (!backSurfaces.empty()) {
        node->back = BuildRecursive(backSurfaces);
    }

    return node;
}

size_t BSPTree::ChooseSplitter(const std::vector<Surface>& surfaces) const {
    // Simple heuristic: choose the surface that splits the fewest other surfaces
    size_t bestIndex = 0;
    size_t bestSplits = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < surfaces.size(); ++i) {
        size_t splits = 0;
        for (size_t j = 0; j < surfaces.size(); ++j) {
            if (i != j) {
                int classification = ClassifySurface(surfaces[j], surfaces[i]);
                if (classification == 0) {
                    splits++; // Surface lies on the plane
                }
            }
        }

        if (splits < bestSplits) {
            bestSplits = splits;
            bestIndex = i;
        }
    }

    return bestIndex;
}

int BSPTree::ClassifySurface(const Surface& surface, const Surface& splitter) const {
    // Get the plane defined by the splitter
    Vector3 splitterNormal = splitter.GetNormal();
    float splitterDistance = splitter.GetDistance();

    // Classify the surface's start and end points
    float startDist = Vector3DotProduct(splitterNormal, surface.start) - splitterDistance;
    float endDist = Vector3DotProduct(splitterNormal, surface.end) - splitterDistance;

    // Handle floating point precision issues
    const float epsilon = 0.001f;

    if (fabsf(startDist) < epsilon && fabsf(endDist) < epsilon) {
        return 0; // On plane
    } else if (startDist >= -epsilon && endDist >= -epsilon) {
        return 1; // In front
    } else if (startDist <= epsilon && endDist <= epsilon) {
        return -1; // Behind
    } else {
        return 0; // Spans plane (will be handled by splitting)
    }
}

bool BSPTree::SplitSurface(const Surface& surface, const Surface& splitter,
                          Surface& front, Surface& back) const {
    // For simplicity, if surface spans the plane, we'll just classify it as being on the plane
    // A full implementation would split the surface into front and back pieces
    // This is a simplified version for the prototype
    int classification = ClassifySurface(surface, splitter);
    if (classification == 0) {
        return false; // No split needed
    }

    // For now, just return the original surface in the appropriate child
    if (classification > 0) {
        front = surface;
    } else {
        back = surface;
    }

    return true;
}

void BSPTree::TraverseForRendering(const Vector3& camera, std::vector<const Surface*>& visibleSurfaces) const {
    if (!root_) return;
    TraverseRenderRecursive(root_.get(), camera, visibleSurfaces);
}

void BSPTree::TraverseRenderRecursive(const BSPNode* node, const Vector3& camera,
                                     std::vector<const Surface*>& visibleSurfaces) const {
    if (!node) return;

    // Classify camera relative to this node's splitter
    int cameraSide = ClassifySurface({camera, camera}, node->splitter);

    // Traverse back first (farther from camera), then front (closer to camera)
    if (cameraSide <= 0 && node->back) {
        TraverseRenderRecursive(node->back.get(), camera, visibleSurfaces);
    }
    if (cameraSide >= 0 && node->front) {
        TraverseRenderRecursive(node->front.get(), camera, visibleSurfaces);
    }

    // Add surfaces in this node if they're potentially visible
    for (const auto& surface : node->surfaces) {
        if (IsSurfaceVisible(surface, camera)) {
            visibleSurfaces.push_back(&surface);
        }
    }

    // Traverse the other side
    if (cameraSide <= 0 && node->front) {
        TraverseRenderRecursive(node->front.get(), camera, visibleSurfaces);
    }
    if (cameraSide >= 0 && node->back) {
        TraverseRenderRecursive(node->back.get(), camera, visibleSurfaces);
    }
}

float BSPTree::CastRay(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance) const {
    if (!root_) return maxDistance;
    return CastRayRecursive(root_.get(), rayOrigin, rayDirection, maxDistance);
}

float BSPTree::CastRayRecursive(const BSPNode* node, const Vector3& rayOrigin,
                               const Vector3& rayDirection, float maxDistance) const {
    if (!node) return maxDistance;

    float closestHit = maxDistance;

    // Check surfaces in this node
    for (const auto& surface : node->surfaces) {
        // Simple ray-plane intersection for walls
        if (surface.isWall) {
            Vector3 normal = surface.GetNormal();
            float denom = Vector3DotProduct(normal, rayDirection);

            if (fabsf(denom) > 0.001f) {
                Vector3 toPlane = Vector3Subtract(surface.start, rayOrigin);
                float t = Vector3DotProduct(toPlane, normal) / denom;

                if (t > 0.001f && t < closestHit) {
                    Vector3 hitPoint = Vector3Add(rayOrigin, Vector3Scale(rayDirection, t));

                    // Check if hit point is within surface bounds
                    Vector3 surfaceVec = Vector3Subtract(surface.end, surface.start);
                    Vector3 hitVec = Vector3Subtract(hitPoint, surface.start);

                    float surfaceLength = Vector3Length(surfaceVec);
                    float projection = Vector3DotProduct(hitVec, Vector3Normalize(surfaceVec));

                    if (projection >= 0 && projection <= surfaceLength) {
                        closestHit = t;
                    }
                }
            }
        }
    }

    // Classify ray relative to splitter and traverse appropriate children
    int raySide = ClassifySurface({rayOrigin, rayOrigin}, node->splitter);

    // Traverse children in ray direction order
    if (raySide <= 0 && node->back) {
        float backHit = CastRayRecursive(node->back.get(), rayOrigin, rayDirection, closestHit);
        closestHit = std::min(closestHit, backHit);
    }
    if (raySide >= 0 && node->front) {
        float frontHit = CastRayRecursive(node->front.get(), rayOrigin, rayDirection, closestHit);
        closestHit = std::min(closestHit, frontHit);
    }

    return closestHit;
}

bool BSPTree::ContainsPoint(const Vector3& point) const {
    // For now, just check if point is within reasonable bounds
    // A full implementation would check against the actual BSP partitions
    return (point.x >= -1000.0f && point.x <= 1000.0f &&
            point.y >= -1000.0f && point.y <= 1000.0f &&
            point.z >= -1000.0f && point.z <= 1000.0f);
}

void BSPTree::Clear() {
    root_.reset();
    allSurfaces_.clear();
}

bool BSPTree::IsSurfaceVisible(const Surface& surface, const Vector3& camera) const {
    // Simple visibility check: surface is visible if camera can see it
    // A more sophisticated version would check frustum culling
    Vector3 toSurface = Vector3Subtract(surface.start, camera);
    float distance = Vector3Length(toSurface);

    // Don't render surfaces that are too far away
    if (distance > 1000.0f) return false;

    // Basic backface culling
    Vector3 normal = surface.GetNormal();
    Vector3 viewDir = Vector3Normalize(toSurface);
    float dotProduct = Vector3DotProduct(normal, viewDir);

    // For walls, normal should point towards camera
    if (surface.isWall && dotProduct > 0) return false;

    return true;
}
