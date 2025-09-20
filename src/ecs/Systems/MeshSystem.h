#pragma once

#include "../System.h"
#include "../Components/MeshComponent.h"
#include "../Entity.h"
#include <unordered_map>
#include <vector>

/*
MeshSystem - Handles all mesh operations for ECS

This system manages mesh creation, modification, and operations for all
MeshComponents in the ECS. It provides the logic layer for mesh data
while keeping MeshComponent as pure data.

Features:
- Mesh creation (cubes, pyramids, custom shapes)
- Mesh modification and optimization
- Legacy compatibility with existing code
- Entity-based mesh management
- Performance optimizations for static meshes
*/

class MeshSystem : public System {
public:
    MeshSystem();
    ~MeshSystem();

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;
    const char* GetName() const { return "MeshSystem"; }

    // Mesh creation operations
    void CreateCube(Entity* entity, float size = 1.0f, const Color& color = WHITE);
    void CreatePyramid(Entity* entity, float baseSize = 1.0f, float height = 1.5f,
                      const std::vector<Color>& faceColors = {RED, GREEN, BLUE, YELLOW});
    void CreateCustomMesh(Entity* entity, const std::vector<MeshVertex>& vertices,
                         const std::vector<MeshTriangle>& triangles);

    // Mesh modification operations
    void ClearMesh(Entity* entity);
    void AddVertex(Entity* entity, const Vector3& position, const Vector3& normal = {0,1,0},
                   const Vector2& texCoord = {0,0}, const Color& color = WHITE);
    void AddTriangle(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3);
    void AddQuad(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4);

    // Legacy compatibility methods (for existing code)
    void CreateCube(MeshComponent& mesh, float size = 1.0f, const Color& color = WHITE);
    void CreatePyramid(MeshComponent& mesh, float baseSize = 1.0f, float height = 1.5f,
                      const std::vector<Color>& faceColors = {RED, GREEN, BLUE, YELLOW});
    void ClearMesh(MeshComponent& mesh);
    void AddVertex(MeshComponent& mesh, const Vector3& position, const Vector3& normal = {0,1,0},
                   const Vector2& texCoord = {0,0}, const Color& color = WHITE);
    void AddTriangle(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3);
    void AddQuad(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4);

    // Mesh utility methods
    size_t GetVertexCount(Entity* entity) const;
    size_t GetTriangleCount(Entity* entity) const;
    const std::vector<MeshVertex>* GetVertices(Entity* entity) const;
    const std::vector<MeshTriangle>* GetTriangles(Entity* entity) const;

    // Legacy compatibility utility methods
    size_t GetVertexCount(const MeshComponent& mesh) const { return mesh.vertices.size(); }
    size_t GetTriangleCount(const MeshComponent& mesh) const { return mesh.triangles.size(); }
    const std::vector<MeshVertex>& GetVertices(const MeshComponent& mesh) const { return mesh.vertices; }
    const std::vector<MeshTriangle>& GetTriangles(const MeshComponent& mesh) const { return mesh.triangles; }

    // Transform operations (temporary - should use TransformComponent)
    float GetRotationAngle(Entity* entity) const;
    Vector3 GetRotationAxis(Entity* entity) const;
    void SetRotation(Entity* entity, float angle, const Vector3& axis);

    // Legacy transform operations
    float GetRotationAngle(const MeshComponent& mesh) const { return mesh.rotationAngle; }
    Vector3 GetRotationAxis(const MeshComponent& mesh) const { return mesh.rotationAxis; }
    void SetRotation(MeshComponent& mesh, float angle, const Vector3& axis) {
        mesh.rotationAngle = angle;
        mesh.rotationAxis = axis;
    }

    // Material operations (temporary - should use MaterialComponent)
    int GetMaterial(Entity* entity) const;
    void SetMaterial(Entity* entity, int materialId);

    // Legacy material operations
    int GetMaterial(const MeshComponent& mesh) const { return mesh.materialId; }
    void SetMaterial(MeshComponent& mesh, int materialId) { mesh.materialId = materialId; }

    // Texture operations (temporary - should use TextureComponent)
    Texture2D GetTexture(Entity* entity) const;
    void SetTexture(Entity* entity, Texture2D texture);

    // Legacy texture operations
    Texture2D GetTexture(const MeshComponent& mesh) const { return mesh.texture; }
    void SetTexture(MeshComponent& mesh, Texture2D texture) { mesh.texture = texture; }

private:
    // Internal mesh creation helpers
    void CreateCubeGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                           float size, const Color& color);
    void CreatePyramidGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                              float baseSize, float height, const std::vector<Color>& faceColors);

    // Helper to get MeshComponent from entity
    MeshComponent* GetMeshComponent(Entity* entity) const;

    // System state
    bool initialized_;
};
