#include "MeshSystem.h"
#include "../../utils/Logger.h"
#include <cmath>

MeshSystem::MeshSystem()
    : initialized_(false) {
    LOG_INFO("MeshSystem created");
}

MeshSystem::~MeshSystem() {
    LOG_INFO("MeshSystem destroyed");
}

void MeshSystem::Initialize() {
    if (initialized_) {
        LOG_WARNING("MeshSystem already initialized");
        return;
    }

    LOG_INFO("Initializing MeshSystem");

    // Set up component signature for MeshComponent
    SetSignature<MeshComponent>();

    initialized_ = true;
    LOG_INFO("MeshSystem initialized successfully");
}

void MeshSystem::Update(float deltaTime) {
    // Mesh system doesn't need per-frame updates for now
    // Could be used for mesh animations or procedural generation later
}

void MeshSystem::Shutdown() {
    if (!initialized_) return;

    LOG_INFO("Shutting down MeshSystem");
    initialized_ = false;
}

// Entity-based mesh operations
void MeshSystem::CreateCube(Entity* entity, float size, const Color& color) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreateCube - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreateCube - Entity has no MeshComponent");
        return;
    }

    CreateCube(*mesh, size, color);
    mesh->needsRebuild = true;
    LOG_DEBUG("Created cube mesh for entity " + std::to_string(entity->GetId()));
}

void MeshSystem::CreatePyramid(Entity* entity, float baseSize, float height,
                              const std::vector<Color>& faceColors) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreatePyramid - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreatePyramid - Entity has no MeshComponent");
        return;
    }

    CreatePyramid(*mesh, baseSize, height, faceColors);
    mesh->needsRebuild = true;
    LOG_DEBUG("Created pyramid mesh for entity " + std::to_string(entity->GetId()));
}

void MeshSystem::CreateCustomMesh(Entity* entity, const std::vector<MeshVertex>& vertices,
                                 const std::vector<MeshTriangle>& triangles) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreateCustomMesh - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreateCustomMesh - Entity has no MeshComponent");
        return;
    }

    mesh->vertices = vertices;
    mesh->triangles = triangles;
    mesh->needsRebuild = true;
    LOG_DEBUG("Created custom mesh for entity " + std::to_string(entity->GetId()) +
              " with " + std::to_string(vertices.size()) + " vertices, " +
              std::to_string(triangles.size()) + " triangles");
}

void MeshSystem::ClearMesh(Entity* entity) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    ClearMesh(*mesh);
}

void MeshSystem::AddVertex(Entity* entity, const Vector3& position, const Vector3& normal,
                           const Vector2& texCoord, const Color& color) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddVertex(*mesh, position, normal, texCoord, color);
}

void MeshSystem::AddTriangle(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddTriangle(*mesh, v1, v2, v3);
}

void MeshSystem::AddQuad(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddQuad(*mesh, v1, v2, v3, v4);
}

// Legacy compatibility methods (direct MeshComponent operations)
void MeshSystem::CreateCube(MeshComponent& mesh, float size, const Color& color) {
    ClearMesh(mesh);
    CreateCubeGeometry(mesh.vertices, mesh.triangles, size, color);
    LOG_INFO("Created cube mesh with " + std::to_string(mesh.vertices.size()) + " vertices, " +
             std::to_string(mesh.triangles.size()) + " triangles");
}

void MeshSystem::CreatePyramid(MeshComponent& mesh, float baseSize, float height,
                              const std::vector<Color>& faceColors) {
    ClearMesh(mesh);
    CreatePyramidGeometry(mesh.vertices, mesh.triangles, baseSize, height, faceColors);
    LOG_INFO("Created pyramid mesh with " + std::to_string(mesh.vertices.size()) + " vertices, " +
             std::to_string(mesh.triangles.size()) + " triangles");
}

void MeshSystem::ClearMesh(MeshComponent& mesh) {
    mesh.vertices.clear();
    mesh.triangles.clear();
}

void MeshSystem::AddVertex(MeshComponent& mesh, const Vector3& position, const Vector3& normal,
                           const Vector2& texCoord, const Color& color) {
    mesh.vertices.push_back({position, normal, texCoord, color});
}

void MeshSystem::AddTriangle(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3) {
    mesh.triangles.push_back({v1, v2, v3});
}

void MeshSystem::AddQuad(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
    AddTriangle(mesh, v1, v2, v3);
    AddTriangle(mesh, v1, v3, v4);
}

// Utility methods
size_t MeshSystem::GetVertexCount(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->vertices.size() : 0;
}

size_t MeshSystem::GetTriangleCount(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->triangles.size() : 0;
}

const std::vector<MeshVertex>* MeshSystem::GetVertices(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? &mesh->vertices : nullptr;
}

const std::vector<MeshTriangle>* MeshSystem::GetTriangles(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? &mesh->triangles : nullptr;
}

// Transform operations
float MeshSystem::GetRotationAngle(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->rotationAngle : 0.0f;
}

Vector3 MeshSystem::GetRotationAxis(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->rotationAxis : Vector3{0, 1, 0};
}

void MeshSystem::SetRotation(Entity* entity, float angle, const Vector3& axis) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    mesh->rotationAngle = angle;
    mesh->rotationAxis = axis;
}

// Material operations
int MeshSystem::GetMaterial(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->materialId : 0;
}

void MeshSystem::SetMaterial(Entity* entity, int materialId) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    mesh->materialId = materialId;
}

// Texture operations
Texture2D MeshSystem::GetTexture(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->texture : Texture2D{0, 0, 0, 0, 0};
}

void MeshSystem::SetTexture(Entity* entity, Texture2D texture) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    mesh->texture = texture;
}

// Private helper methods
void MeshSystem::CreateCubeGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                                   float size, const Color& color) {
    float halfSize = size * 0.5f;

    // Define 8 vertices of the cube
    vertices.push_back({{-halfSize, -halfSize, -halfSize}, {0, 0, -1}, {0, 0}, color}); // 0: front-bottom-left
    vertices.push_back({{ halfSize, -halfSize, -halfSize}, {0, 0, -1}, {1, 0}, color}); // 1: front-bottom-right
    vertices.push_back({{ halfSize,  halfSize, -halfSize}, {0, 0, -1}, {1, 1}, color}); // 2: front-top-right
    vertices.push_back({{-halfSize,  halfSize, -halfSize}, {0, 0, -1}, {0, 1}, color}); // 3: front-top-left

    vertices.push_back({{-halfSize, -halfSize,  halfSize}, {0, 0, 1}, {0, 0}, color}); // 4: back-bottom-left
    vertices.push_back({{ halfSize, -halfSize,  halfSize}, {0, 0, 1}, {1, 0}, color}); // 5: back-bottom-right
    vertices.push_back({{ halfSize,  halfSize,  halfSize}, {0, 0, 1}, {1, 1}, color}); // 6: back-top-right
    vertices.push_back({{-halfSize,  halfSize,  halfSize}, {0, 0, 1}, {0, 1}, color}); // 7: back-top-left

    // Front face
    triangles.push_back({0, 1, 2});
    triangles.push_back({0, 2, 3});

    // Back face
    triangles.push_back({5, 4, 7});
    triangles.push_back({5, 7, 6});

    // Left face
    triangles.push_back({4, 0, 3});
    triangles.push_back({4, 3, 7});

    // Right face
    triangles.push_back({1, 5, 6});
    triangles.push_back({1, 6, 2});

    // Top face
    triangles.push_back({3, 2, 6});
    triangles.push_back({3, 6, 7});

    // Bottom face
    triangles.push_back({4, 5, 1});
    triangles.push_back({4, 1, 0});
}

void MeshSystem::CreatePyramidGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                                      float baseSize, float height, const std::vector<Color>& faceColors) {
    float halfBase = baseSize * 0.5f;

    // Base vertices (square base on XZ plane, Y=0)
    Color baseColor = faceColors.size() > 4 ? faceColors[4] : GRAY;
    vertices.push_back({{-halfBase, 0, -halfBase}, {0, -1, 0}, {0, 0}, baseColor}); // 0: base-bottom-left
    vertices.push_back({{ halfBase, 0, -halfBase}, {0, -1, 0}, {1, 0}, baseColor}); // 1: base-bottom-right
    vertices.push_back({{ halfBase, 0,  halfBase}, {0, -1, 0}, {1, 1}, baseColor}); // 2: base-top-right
    vertices.push_back({{-halfBase, 0,  halfBase}, {0, -1, 0}, {0, 1}, baseColor}); // 3: base-top-left

    // Apex vertex (top of pyramid)
    vertices.push_back({{0, height, 0}, {0, 1, 0}, {0.5f, 0.5f}, WHITE}); // 4: apex

    // Base (bottom face)
    triangles.push_back({0, 1, 2});
    triangles.push_back({0, 2, 3});

    // Front face (red)
    Color frontColor = faceColors.size() > 0 ? faceColors[0] : RED;
    vertices[0].color = frontColor;
    vertices[1].color = frontColor;
    vertices[4].color = frontColor;
    triangles.push_back({0, 4, 1});

    // Right face (green)
    Color rightColor = faceColors.size() > 1 ? faceColors[1] : GREEN;
    vertices[1].color = rightColor;
    vertices[2].color = rightColor;
    triangles.push_back({1, 4, 2});

    // Back face (blue)
    Color backColor = faceColors.size() > 2 ? faceColors[2] : BLUE;
    vertices[2].color = backColor;
    vertices[3].color = backColor;
    triangles.push_back({2, 4, 3});

    // Left face (yellow)
    Color leftColor = faceColors.size() > 3 ? faceColors[3] : YELLOW;
    vertices[3].color = leftColor;
    vertices[0].color = leftColor;
    triangles.push_back({3, 4, 0});
}

MeshComponent* MeshSystem::GetMeshComponent(Entity* entity) const {
    if (!entity) return nullptr;
    return entity->GetComponent<MeshComponent>();
}
