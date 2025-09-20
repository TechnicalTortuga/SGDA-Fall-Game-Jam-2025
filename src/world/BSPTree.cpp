#include "BSPTree.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <climits>

BSPTree::BSPTree() : root_(nullptr) {}

// --- View frustum helpers ---

bool BSPTree::IsPointInViewFrustum(const Vector3& point, const Camera3D& camera) const {
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

void BSPTree::UpdateNodeBounds(BSPNode* node) {
    if (!node) return;
    node->bounds = ComputeBoundsForFaces(node->faces);
    if (node->front) node->bounds.Encapsulate(node->front->bounds);
    if (node->back)  node->bounds.Encapsulate(node->back->bounds);
}

bool BSPTree::IsAABBInViewFrustum(const AABB& box, const Camera3D& camera) const {
    // Test the 8 corners against the frustum; if any corner is inside, accept.
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
    for (const auto& c : corners) {
        if (IsPointInViewFrustum(c, camera)) return true;
    }
    return false;
}

bool BSPTree::SubtreeInViewFrustum(const BSPNode* node, const Camera3D& camera) const {
    // Conservative test: accept if AABB intersects the view frustum cone
    return IsAABBInViewFrustum(node->bounds, camera);
}

void BSPTree::BuildFromFaces(const std::vector<Face>& faces) {
    allFaces_ = faces;
    root_ = BuildRecursiveFaces(faces);
}

void BSPTree::BuildFromBrushes(const std::vector<Brush>& brushes) {
    std::vector<Face> faces;
    faces.reserve(brushes.size() * 6);
    for (const auto& b : brushes) {
        for (const auto& f : b.faces) faces.push_back(f);
    }
    BuildFromFaces(faces);
}

 

// --- Ray casting helpers ---
static bool RayIntersectsTriangleMT(const Vector3& ro, const Vector3& rd,
                                    const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                    float& outT) {
    // Möller–Trumbore ray/triangle intersection
    const float EPS = 1e-6f;
    Vector3 e1 = Vector3Subtract(v1, v0);
    Vector3 e2 = Vector3Subtract(v2, v0);
    Vector3 pvec = Vector3CrossProduct(rd, e2);
    float det = Vector3DotProduct(e1, pvec);
    if (fabsf(det) < EPS) return false; // parallel
    float invDet = 1.0f / det;
    Vector3 tvec = Vector3Subtract(ro, v0);
    float u = Vector3DotProduct(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    Vector3 qvec = Vector3CrossProduct(tvec, e1);
    float v = Vector3DotProduct(rd, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    float t = Vector3DotProduct(e2, qvec) * invDet;
    if (t <= EPS) return false; // behind or at origin
    outT = t;
    return true;
}

static bool RayIntersectsAABB(const Vector3& ro, const Vector3& rd, const AABB& box, float& tminOut, float& tmaxOut) {
    float tmin = -FLT_MAX;
    float tmax =  FLT_MAX;
    // X
    if (fabsf(rd.x) < 1e-8f) {
        if (ro.x < box.min.x || ro.x > box.max.x) return false;
    } else {
        float inv = 1.0f / rd.x;
        float t1 = (box.min.x - ro.x) * inv;
        float t2 = (box.max.x - ro.x) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    // Y
    if (fabsf(rd.y) < 1e-8f) {
        if (ro.y < box.min.y || ro.y > box.max.y) return false;
    } else {
        float inv = 1.0f / rd.y;
        float t1 = (box.min.y - ro.y) * inv;
        float t2 = (box.max.y - ro.y) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    // Z
    if (fabsf(rd.z) < 1e-8f) {
        if (ro.z < box.min.z || ro.z > box.max.z) return false;
    } else {
        float inv = 1.0f / rd.z;
        float t1 = (box.min.z - ro.z) * inv;
        float t2 = (box.max.z - ro.z) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    tminOut = tmin; tmaxOut = tmax;
    return true;
}

float BSPTree::CastRay(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance) const {
    if (!root_) return maxDistance;
    return CastRayRecursive(root_.get(), rayOrigin, rayDirection, maxDistance);
}

float BSPTree::CastRayWithNormal(const Vector3& rayOrigin, const Vector3& rayDirection, float maxDistance, Vector3& hitNormal) const {
    if (!root_) {
        hitNormal = {0.0f, 1.0f, 0.0f}; // Default up normal
        return maxDistance;
    }
    return CastRayRecursiveWithNormal(root_.get(), rayOrigin, rayDirection, maxDistance, hitNormal);
}

float BSPTree::CastRayRecursive(const BSPNode* node, const Vector3& rayOrigin,
                               const Vector3& rayDirection, float maxDistance) const {
    if (!node) return maxDistance;

    // Early-out AABB prune
    float tmin=0.0f, tmax=0.0f;
    if (!RayIntersectsAABB(rayOrigin, rayDirection, node->bounds, tmin, tmax)) {
        return maxDistance;
    }
    if (tmin > maxDistance) return maxDistance;

    float closest = maxDistance;

    // Traverse near child first based on which side of the plane the origin lies
    float originSide = Vector3DotProduct(node->planeNormal, rayOrigin) - node->planeDistance;
    const BSPNode* nearChild = originSide >= 0.0f ? node->front.get() : node->back.get();
    const BSPNode* farChild  = originSide >= 0.0f ? node->back.get()  : node->front.get();

    if (nearChild) {
        closest = CastRayRecursive(nearChild, rayOrigin, rayDirection, closest);
    }

    // Test faces at this node
    for (const auto& face : node->faces) {
        // Respect collidable flag
        if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;
        const auto& v = face.vertices;
        if (v.size() < 3) continue;

        // Triangle fan (v0, vi, vi+1)
        for (size_t i = 1; i + 1 < v.size(); ++i) {
            float t;
            if (RayIntersectsTriangleMT(rayOrigin, rayDirection, v[0], v[i], v[i+1], t)) {
                if (t < closest) closest = t;
            }
        }
    }

    if (farChild) {
        closest = CastRayRecursive(farChild, rayOrigin, rayDirection, closest);
    }

    return closest;
}

float BSPTree::CastRayRecursiveWithNormal(const BSPNode* node, const Vector3& rayOrigin,
                                         const Vector3& rayDirection, float maxDistance, Vector3& hitNormal) const {
    if (!node) return maxDistance;

    // Early-out AABB prune
    float tmin=0.0f, tmax=0.0f;
    if (!RayIntersectsAABB(rayOrigin, rayDirection, node->bounds, tmin, tmax)) {
        return maxDistance;
    }
    if (tmin > maxDistance) return maxDistance;

    float closest = maxDistance;
    Vector3 closestNormal = {0.0f, 1.0f, 0.0f}; // Default up normal

    // Traverse near child first based on which side of the plane the origin lies
    float originSide = Vector3DotProduct(node->planeNormal, rayOrigin) - node->planeDistance;
    const BSPNode* nearChild = originSide >= 0.0f ? node->front.get() : node->back.get();
    const BSPNode* farChild  = originSide >= 0.0f ? node->back.get()  : node->front.get();

    if (nearChild) {
        Vector3 childNormal;
        float childDistance = CastRayRecursiveWithNormal(nearChild, rayOrigin, rayDirection, closest, childNormal);
        if (childDistance < closest) {
            closest = childDistance;
            closestNormal = childNormal;
        }
    }

    // Test faces at this node
    for (const auto& face : node->faces) {
        // Respect collidable flag
        if (!HasFlag(face.flags, FaceFlags::Collidable)) continue;
        const auto& v = face.vertices;
        if (v.size() < 3) continue;

        // Triangle fan (v0, vi, vi+1)
        for (size_t i = 1; i + 1 < v.size(); ++i) {
            float t;
            if (RayIntersectsTriangleMT(rayOrigin, rayDirection, v[0], v[i], v[i+1], t)) {
                if (t < closest) {
                    closest = t;
                    // Use the face normal for industry-standard collision response
                    closestNormal = face.normal;

                    // Debug: Log when we hit a non-flat surface (potential slope)
                    if (face.normal.y < 0.99f && face.normal.y > 0.5f) {
                        LOG_INFO("BSP RAYCAST: *** HIT SLOPE SURFACE *** normal=(" +
                                std::to_string(face.normal.x) + "," + std::to_string(face.normal.y) + "," +
                                std::to_string(face.normal.z) + ") at distance " + std::to_string(t));
                    }

                    // Debug: Log ALL triangle intersections to see what's being hit
                    LOG_INFO("BSP RAYCAST: Triangle hit - normal=(" +
                            std::to_string(face.normal.x) + "," + std::to_string(face.normal.y) + "," +
                            std::to_string(face.normal.z) + ") at distance " + std::to_string(t));
                }
            }
        }
    }

    if (farChild) {
        Vector3 childNormal;
        float childDistance = CastRayRecursiveWithNormal(farChild, rayOrigin, rayDirection, closest, childNormal);
        if (childDistance < closest) {
            closest = childDistance;
            closestNormal = childNormal;
        }
    }

    hitNormal = closestNormal;
    return closest;
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
    allFaces_.clear();
}

// --- Face-based BSP building & traversal ---

// Compute bounds for a set of faces
AABB BSPTree::ComputeBoundsForFaces(const std::vector<Face>& faces) {
    AABB b = AABB::Infinite();
    for (const auto& f : faces) {
        for (const auto& v : f.vertices) b.Encapsulate(v);
    }
    return b;
}

// Plane helpers
BSPTree::Plane BSPTree::PlaneFromFace(const Face& face) {
    Plane p;
    if (face.vertices.size() >= 3) {
        Vector3 e1 = Vector3Subtract(face.vertices[1], face.vertices[0]);
        Vector3 e2 = Vector3Subtract(face.vertices[2], face.vertices[0]);
        p.n = Vector3Normalize(Vector3CrossProduct(e1, e2));
        p.d = Vector3DotProduct(p.n, face.vertices[0]);
    } else {
        p.n = {0,1,0};
        p.d = 0.0f;
    }
    return p;
}

float BSPTree::SignedDistanceToPlane(const Plane& p, const Vector3& point) {
    return Vector3DotProduct(p.n, point) - p.d;
}

int BSPTree::ClassifyFace(const Face& face, const Plane& plane) const {
    int inFront = 0, behind = 0;
    const float EPS = 1e-5f;
    for (const auto& v : face.vertices) {
        float dist = SignedDistanceToPlane(plane, v);
        if (dist > EPS) inFront++;
        else if (dist < -EPS) behind++;
    }
    if (inFront > 0 && behind > 0) return 0; // spanning
    if (inFront > 0) return 1; // front
    if (behind > 0) return -1; // back
    return 0; // coplanar or on-plane
}

// Split convex face by plane into front/back polygons
void BSPTree::SplitFaceByPlane(const Face& face, const Plane& plane,
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

        auto addVertex = [](std::vector<Vector3>& arr, const Vector3& v){ arr.push_back(v); };

        // classify point a
        if (da >= -EPS) addVertex(frontVerts, a); // on or front
        if (da <= EPS)  addVertex(backVerts,  a); // on or back

        // if edge ab crosses plane, add intersection
        if ((da > EPS && db < -EPS) || (da < -EPS && db > EPS)) {
            float t = da / (da - db);
            Vector3 diff = Vector3Subtract(b, a);
            Vector3 hit = Vector3Add(a, Vector3Scale(diff, t));
            addVertex(frontVerts, hit);
            addVertex(backVerts,  hit);
        }
    }

    if (frontVerts.size() >= 3) {
        hasFront = true;
        outFront = face;
        outFront.vertices = std::move(frontVerts);
        outFront.RecalculateNormal();
    }
    if (backVerts.size() >= 3) {
        hasBack = true;
        outBack = face;
        outBack.vertices = std::move(backVerts);
        outBack.RecalculateNormal();
    }
}

size_t BSPTree::ChooseSplitterFaces(const std::vector<Face>& faces) const {
    // Simple heuristic: pick the face whose plane causes fewest spanning
    size_t best = 0; int bestScore = INT_MAX;
    for (size_t i = 0; i < faces.size(); ++i) {
        Plane p = PlaneFromFace(faces[i]);
        int span = 0;
        for (size_t j = 0; j < faces.size(); ++j) {
            if (i == j) continue;
            int c = ClassifyFace(faces[j], p);
            if (c == 0) span++;
        }
        if (span < bestScore) { bestScore = span; best = i; }
    }
    return best;
}

std::unique_ptr<BSPNode> BSPTree::BuildRecursiveFaces(std::vector<Face> faces) {
    if (faces.empty()) return nullptr;
    auto node = std::make_unique<BSPNode>();

    if (faces.size() == 1) {
        node->faces = faces;
        Plane p = PlaneFromFace(faces[0]);
        node->planeNormal = p.n;
        node->planeDistance = p.d;
        node->bounds = ComputeBoundsForFaces(node->faces);
        return node;
    }

    size_t splitterIndex = ChooseSplitterFaces(faces);
    Face splitterFace = faces[splitterIndex];
    Plane splitter = PlaneFromFace(splitterFace);
    node->planeNormal = splitter.n;
    node->planeDistance = splitter.d;

    // remove splitter
    faces.erase(faces.begin() + splitterIndex);

    std::vector<Face> frontFaces;
    std::vector<Face> backFaces;

    for (const auto& f : faces) {
        int cls = ClassifyFace(f, splitter);
        if (cls > 0) {
            frontFaces.push_back(f);
        } else if (cls < 0) {
            backFaces.push_back(f);
        } else {
            bool hf=false, hb=false; Face ff, fb;
            SplitFaceByPlane(f, splitter, hf, ff, hb, fb);
            if (hf) frontFaces.push_back(ff);
            if (hb) backFaces.push_back(fb);
            if (!hf && !hb) node->faces.push_back(f); // coplanar
        }
    }

    if (!frontFaces.empty()) node->front = BuildRecursiveFaces(frontFaces);
    if (!backFaces.empty())  node->back  = BuildRecursiveFaces(backFaces);

    node->bounds = ComputeBoundsForFaces(node->faces);
    if (node->front) node->bounds.Encapsulate(node->front->bounds);
    if (node->back)  node->bounds.Encapsulate(node->back->bounds);
    return node;
}

bool BSPTree::IsFaceVisible(const Face& face, const Camera3D& camera) const {
    // Backface and frustum check using face centroid
    Vector3 center{0,0,0};
    for (const auto& v : face.vertices) center = Vector3Add(center, v);
    center = Vector3Scale(center, 1.0f / (float)face.vertices.size());
    if (!IsPointInViewFrustum(center, camera)) return false;
    Vector3 viewDir = Vector3Normalize(Vector3Subtract(camera.position, center));
    float dot = Vector3DotProduct(face.normal, viewDir);
    return dot > 0.0f;
}

void BSPTree::TraverseRenderRecursiveCamera3D_Faces(const BSPNode* node, const Camera3D& camera,
                                                    std::vector<const Face*>& visibleFaces) const {
    if (!node) return;
    if (!SubtreeInViewFrustum(node, camera)) return;

    // classify camera position relative to plane
    float camSide = Vector3DotProduct(node->planeNormal, camera.position) - node->planeDistance;
    const BSPNode* nearChild = camSide >= 0.0f ? node->front.get() : node->back.get();
    const BSPNode* farChild  = camSide >= 0.0f ? node->back.get()  : node->front.get();

    if (nearChild) TraverseRenderRecursiveCamera3D_Faces(nearChild, camera, visibleFaces);

    for (const auto& f : node->faces) {
        if (HasFlag(f.flags, FaceFlags::Invisible) || HasFlag(f.flags, FaceFlags::NoDraw)) continue;
        if (IsFaceVisible(f, camera)) visibleFaces.push_back(&f);
    }

    if (farChild) TraverseRenderRecursiveCamera3D_Faces(farChild, camera, visibleFaces);
}

void BSPTree::TraverseForRenderingFaces(const Camera3D& camera, std::vector<const Face*>& visibleFaces) const {
    visibleFaces.clear();
    TraverseRenderRecursiveCamera3D_Faces(root_.get(), camera, visibleFaces);
}
