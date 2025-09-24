 #pragma once

#include "../System.h"
#include "../Components/MeshComponent.h"
#include "../Entity.h"
#include "../Systems/WorldSystem.h"
#include "../../core/Engine.h"
#include <unordered_map>
#include <vector>

/*
SubMesh - Defines a single part of a composite mesh
Contains the primitive type and relative transform for general composite rendering
*/
struct SubMesh {
    std::string primitiveType;     // "sphere", "cylinder", "cube", etc.
    Vector3 relativePosition;      // Position relative to parent mesh
    Vector3 relativeScale;         // Scale relative to parent mesh  
    Quaternion relativeRotation;   // Rotation relative to parent mesh
    
    // Primitive-specific parameters (stored as flexible data)
    float radius = 1.0f;           // For spheres, cylinders
    float height = 1.0f;           // For cylinders, capsules
    Vector3 size = {1,1,1};        // For cubes, boxes
    
    SubMesh() : relativePosition{0,0,0}, relativeScale{1,1,1}, relativeRotation{0,0,0,1} {}
    SubMesh(const std::string& type, const Vector3& pos, const Vector3& scale = {1,1,1}) 
        : primitiveType(type), relativePosition(pos), relativeScale(scale), relativeRotation{0,0,0,1} {}
};

/*
CompositeMeshDefinition - Lightweight registry entry for composite mesh types
Stores the sub-mesh definitions for reuse across multiple entities
*/
struct CompositeMeshDefinition {
    std::string name;
    std::vector<SubMesh> subMeshes;
    
    CompositeMeshDefinition() = default;
    CompositeMeshDefinition(const std::string& meshName) : name(meshName) {}
};

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
    void CreateSphere(Entity* entity, float radius = 1.0f);
    void CreateCapsule(Entity* entity, float radius = 0.5f, float height = 2.0f);
    void CreateCylinder(Entity* entity, float radius = 1.0f, float height = 2.0f);
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

    // Material operations removed - now handled by MaterialSystem

    // Gradient color generation helpers
    std::vector<Color> GenerateLinearGradientColors(Color primary, Color secondary, int numFaces);
    std::vector<Color> GenerateRadialGradientColors(Color primary, Color secondary, int numFaces);

    // Texture operations (use AssetSystem integration)
    Texture2D GetTexture(Entity* entity) const;
    void SetTexture(Entity* entity, Texture2D texture);

    // Resolve textures for meshes that have material IDs set
    void ResolvePendingTextures();
    
    // Composite mesh management (data-oriented)
    uint64_t RegisterCompositeMesh(const std::string& name, const std::vector<SubMesh>& subMeshes);
    const CompositeMeshDefinition* GetCompositeMeshDefinition(uint64_t compositeMeshId) const;

private:
    // Internal mesh creation helpers
    void CreateCubeGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                           float size, const Color& color);
    void CreatePyramidGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                              float baseSize, float height, const std::vector<Color>& faceColors);
    void CreatePyramidWithGradient(MeshComponent& mesh, float baseSize, float height,
                                 const Color& gradientStart, const Color& gradientEnd,
                                 const Vector3& gradientDirection);

    // Helper to get MeshComponent from entity
    MeshComponent* GetMeshComponent(Entity* entity) const;
    
    // Cache invalidation support
    void InvalidateEntityCache(Entity* entity) const;

    // System state
    bool initialized_;
    
    // Composite mesh registry (data-oriented storage)
    std::unordered_map<uint64_t, CompositeMeshDefinition> compositeMeshRegistry_;
    uint64_t nextCompositeMeshId_;
    
    // Mesh cache (similar to MaterialSystem's material cache)
    std::unordered_map<std::string, Mesh> meshCache_;
    
    // Helper methods for mesh creation
    Mesh CreatePrimitiveMesh(const std::string& primitiveType, float size = 1.0f, float radius = 1.0f, float height = 2.0f) const;
    Mesh ConvertCustomMesh(const MeshComponent& meshComponent) const;
};
