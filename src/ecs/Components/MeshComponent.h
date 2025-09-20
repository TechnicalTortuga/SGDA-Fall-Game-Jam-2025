#pragma once

#include "../Component.h"
#include "raylib.h"
#include <vector>
#include <string>

struct MeshVertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
    Color color;
};

struct MeshTriangle {
    unsigned int v1, v2, v3;
};

class MeshComponent : public Component {
public:
    MeshComponent();
    ~MeshComponent();

    // Mesh building
    void Clear();
    void AddVertex(const Vector3& position, const Vector3& normal = {0,1,0},
                   const Vector2& texCoord = {0,0}, const Color& color = WHITE);
    void AddTriangle(unsigned int v1, unsigned int v2, unsigned int v3);
    void AddQuad(unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4);

    // Predefined shapes
    void CreateCube(float size = 1.0f, const Color& color = WHITE);
    void CreatePyramid(float baseSize = 1.0f, float height = 1.5f,
                      const std::vector<Color>& faceColors = {RED, GREEN, BLUE, YELLOW});

    // Material/texture
    void SetMaterial(int materialId) { materialId_ = materialId; }
    int GetMaterial() const { return materialId_; }

    // Direct texture setting (for custom meshes)
    void SetTexture(Texture2D texture) { texture_ = texture; }
    Texture2D GetTexture() const { return texture_; }

    // Transform
    void SetRotation(float angle, const Vector3& axis = {0,1,0}) {
        rotationAngle_ = angle;
        rotationAxis_ = axis;
    }
    float GetRotationAngle() const { return rotationAngle_; }
    Vector3 GetRotationAxis() const { return rotationAxis_; }

    // Accessors
    const std::vector<MeshVertex>& GetVertices() const { return vertices_; }
    const std::vector<MeshTriangle>& GetTriangles() const { return triangles_; }
    size_t GetVertexCount() const { return vertices_.size(); }
    size_t GetTriangleCount() const { return triangles_.size(); }

    // Component lifecycle
    void OnAttach() override;
    void OnDetach() override;

private:
    std::vector<MeshVertex> vertices_;
    std::vector<MeshTriangle> triangles_;
    int materialId_;
    Texture2D texture_;  // Direct texture for custom meshes
    float rotationAngle_;
    Vector3 rotationAxis_;
};
