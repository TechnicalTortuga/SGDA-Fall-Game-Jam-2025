#pragma once

#include <vector>
#include <string>
#include <cfloat>
#include <algorithm>
#include <cstdint>  // For uint64_t
#include "raylib.h"
#include "raymath.h"

// Forward declarations
struct AABB;

// Simple AABB (duplicate-friendly lightweight copy to avoid circular includes)
struct BrushAABB {
    Vector3 min;
    Vector3 max;

    BrushAABB()
        : min{FLT_MAX, FLT_MAX, FLT_MAX}
        , max{-FLT_MAX, -FLT_MAX, -FLT_MAX} {}

    void Encapsulate(const Vector3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }
};

// Face rendering mode
enum class FaceRenderMode {
    Default = 0,        // Use material/texture
    VertexColors = 1,   // Use vertex colors only
    Wireframe = 2,      // Wireframe rendering
    Invisible = 3       // Don't render
};

// Face flags
enum class FaceFlags : unsigned int {
    None     = 0,
    NoDraw   = 1 << 0,
    Invisible= 1 << 1,
    Collidable = 1 << 2
};

inline FaceFlags operator|(FaceFlags a, FaceFlags b) {
    return static_cast<FaceFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}
inline bool HasFlag(FaceFlags a, FaceFlags b) {
    return (static_cast<unsigned int>(a) & static_cast<unsigned int>(b)) != 0;
}

// A single planar face (typically a quad) with material + lightmap data
struct Face {
    // Geometry
    std::vector<Vector3> vertices;   // Expect 3+ verts; rendered as triangles/quad
    std::vector<Vector2> uvs;        // UV coordinates for texture mapping (pre-calculated)
    Vector3 normal;                  // Cached normal

    // Material
    int materialId = 0;              // Index into WorldGeometry materials (legacy)
    uint64_t materialEntityId = 0;   // Reference to MaterialComponent entity
    Color tint = WHITE;

    // Rendering mode
    FaceRenderMode renderMode = FaceRenderMode::Default;

    // Lightmapping (non-breaking defaults)
    int lightmapIndex = -1;          // -1 means no lightmap
    Vector2 lightmapUVScale = {1.0f, 1.0f};
    Vector2 lightmapUVOffset = {0.0f, 0.0f};

    // Flags
    FaceFlags flags = FaceFlags::Collidable;

    // Helpers
    void RecalculateNormal() {
        if (vertices.size() >= 3) {
            Vector3 e1 = Vector3Subtract(vertices[1], vertices[0]);
            Vector3 e2 = Vector3Subtract(vertices[2], vertices[0]);
            normal = Vector3Normalize(Vector3CrossProduct(e1, e2));

        } else {
            normal = {0,1,0};
        }
    }

    BrushAABB ComputeBounds() const {
        BrushAABB box;
        for (const auto& v : vertices) box.Encapsulate(v);
        return box;
    }
};

// A solid brush composed of multiple planar faces
struct Brush {
    std::vector<Face> faces;
    bool isDetail = false;           // Structural vs detail (for future vis/bsp tuning)
    BrushAABB bounds;                // Cached bounds for culling

    void RecalculateBounds() {
        bounds = BrushAABB{};
        for (const auto& f : faces) {
            auto fb = f.ComputeBounds();
            bounds.Encapsulate(fb.min);
            bounds.Encapsulate(fb.max);
        }
    }

    // Convenience: add a quadrilateral face from 4 points (in CCW order)
    Face& AddQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                  int materialId, Color color, FaceFlags f = FaceFlags::Collidable) {
        Face face;
        face.vertices = {p0, p1, p2, p3};
        face.tint = color;
        face.materialId = materialId;
        face.materialEntityId = 0; // Will be updated later
        face.flags = f;
        face.RecalculateNormal();
        faces.push_back(face);
        RecalculateBounds();
        return faces.back();
    }

    // Convenience: add a quadrilateral face from 4 points with explicit UVs (in CCW order)
    Face& AddQuadWithUVs(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                        const Vector2& uv0, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3,
                        int materialId, Color color, FaceFlags f = FaceFlags::Collidable) {
        Face face;
        face.vertices = {p0, p1, p2, p3};
        face.uvs = {uv0, uv1, uv2, uv3};
        face.tint = color;
        face.materialId = materialId;
        face.materialEntityId = 0; // Will be updated later
        face.flags = f;
        face.RecalculateNormal();
        faces.push_back(face);
        RecalculateBounds();
        return faces.back();
    }
};
