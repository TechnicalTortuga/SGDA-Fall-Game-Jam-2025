#pragma once

#include "../Component.h"
#include "raylib.h"
#include <vector>
#include <string>
#include <cstdint>

/*
MeshVertex and MeshTriangle - Pure data structures for mesh geometry
These structs contain only the essential vertex and triangle data.
*/
struct MeshVertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
    Color color;
};

struct MeshTriangle {
    unsigned int v1, v2, v3;
};

/*
MeshComponent - Pure data mesh component for ECS

This struct contains ONLY essential mesh data. All mesh operations
(creation, modification, rendering) are handled by dedicated systems
that reference this component by entity ID.

This is the purest form of data-oriented ECS design for meshes.
*/
struct MeshComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "MeshComponent"; }

    // Core mesh geometry data (pure data only - NO methods)
    std::vector<MeshVertex> vertices;
    std::vector<MeshTriangle> triangles;

    // Entity relationships (ECR-compliant - entity IDs only)
    uint64_t materialEntityId = 0;        // References MaterialComponent entity
    uint64_t textureEntityId = 0;         // References TextureComponent entity

    // Reference IDs for related systems (data-oriented decoupling)
    uint64_t meshSystemId = 0;            // For mesh operations and modifications
    uint64_t renderSystemId = 0;          // For rendering coordination
    uint64_t physicsSystemId = 0;         // For physics integration

    // Mesh metadata (pure data)
    std::string meshName = "default";

    // Simple state flags (pure data)
    bool isActive = true;
    bool needsRebuild = false;
    bool isStatic = false;                // For optimization (static meshes don't change)

    // Legacy compatibility fields (to be removed after MeshSystem integration)
    // These maintain compatibility during transition to MeshSystem
    float rotationAngle = 0.0f;
    Vector3 rotationAxis = {0, 1, 0};
    int materialId = 0;
    Texture2D texture = {0, 0, 0, 0, 0};  // id, width, height, mipmaps, format

    // Legacy compatibility methods (temporary - delegate to MeshSystem when available)
    // These maintain compatibility with existing rendering code
    float GetRotationAngle() const { return rotationAngle; }
    Vector3 GetRotationAxis() const { return rotationAxis; }
    void SetRotation(float angle, const Vector3& axis) { rotationAngle = angle; rotationAxis = axis; }

    int GetMaterial() const { return materialId; }
    void SetMaterial(int id) { materialId = id; }

    Texture2D GetTexture() const { return texture; }
    void SetTexture(Texture2D tex) { texture = tex; }

    void Clear() { vertices.clear(); triangles.clear(); }
    size_t GetVertexCount() const { return vertices.size(); }
    size_t GetTriangleCount() const { return triangles.size(); }
    const std::vector<MeshVertex>& GetVertices() const { return vertices; }
    const std::vector<MeshTriangle>& GetTriangles() const { return triangles; }

    // Legacy mesh creation methods (temporary)
    void CreateCube(float size = 1.0f, const Color& color = WHITE);
    void CreatePyramid(float baseSize = 1.0f, float height = 1.5f,
                      const std::vector<Color>& faceColors = {RED, GREEN, BLUE, YELLOW});
};
