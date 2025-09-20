#include "MeshComponent.h"
#include "../../utils/Logger.h"
#include <cmath>

MeshComponent::MeshComponent()
    : materialId_(0), texture_({0}), rotationAngle_(0.0f), rotationAxis_({0, 1, 0}) {
}

MeshComponent::~MeshComponent() {
    Clear();
}

void MeshComponent::Clear() {
    vertices_.clear();
    triangles_.clear();
}

void MeshComponent::AddVertex(const Vector3& position, const Vector3& normal,
                     const Vector2& texCoord, const Color& color) {
    vertices_.push_back({position, normal, texCoord, color});
}

void MeshComponent::AddTriangle(unsigned int v1, unsigned int v2, unsigned int v3) {
    triangles_.push_back({v1, v2, v3});
}

void MeshComponent::AddQuad(unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
    // Add two triangles to form a quad
    AddTriangle(v1, v2, v3);
    AddTriangle(v1, v3, v4);
}

void MeshComponent::CreateCube(float size, const Color& color) {
    Clear();

    float halfSize = size * 0.5f;

    // Define 8 vertices of the cube
    AddVertex({-halfSize, -halfSize, -halfSize}, {0, 0, -1}, {0, 0}, color); // 0: front-bottom-left
    AddVertex({ halfSize, -halfSize, -halfSize}, {0, 0, -1}, {1, 0}, color); // 1: front-bottom-right
    AddVertex({ halfSize,  halfSize, -halfSize}, {0, 0, -1}, {1, 1}, color); // 2: front-top-right
    AddVertex({-halfSize,  halfSize, -halfSize}, {0, 0, -1}, {0, 1}, color); // 3: front-top-left

    AddVertex({-halfSize, -halfSize,  halfSize}, {0, 0, 1}, {0, 0}, color); // 4: back-bottom-left
    AddVertex({ halfSize, -halfSize,  halfSize}, {0, 0, 1}, {1, 0}, color); // 5: back-bottom-right
    AddVertex({ halfSize,  halfSize,  halfSize}, {0, 0, 1}, {1, 1}, color); // 6: back-top-right
    AddVertex({-halfSize,  halfSize,  halfSize}, {0, 0, 1}, {0, 1}, color); // 7: back-top-left

    // Front face
    AddQuad(0, 1, 2, 3);

    // Back face
    AddQuad(5, 4, 7, 6);

    // Left face
    AddQuad(4, 0, 3, 7);

    // Right face
    AddQuad(1, 5, 6, 2);

    // Top face
    AddQuad(3, 2, 6, 7);

    // Bottom face
    AddQuad(4, 5, 1, 0);

    LOG_INFO("Created cube mesh with " + std::to_string(vertices_.size()) + " vertices, " +
             std::to_string(triangles_.size()) + " triangles");
}

void MeshComponent::CreatePyramid(float baseSize, float height, const std::vector<Color>& faceColors) {
    Clear();

    float halfBase = baseSize * 0.5f;

    // Base vertices (square base on XZ plane, Y=0)
    AddVertex({-halfBase, 0, -halfBase}, {0, -1, 0}, {0, 0}, faceColors.size() > 4 ? faceColors[4] : GRAY); // 0: base-bottom-left
    AddVertex({ halfBase, 0, -halfBase}, {0, -1, 0}, {1, 0}, faceColors.size() > 4 ? faceColors[4] : GRAY); // 1: base-bottom-right
    AddVertex({ halfBase, 0,  halfBase}, {0, -1, 0}, {1, 1}, faceColors.size() > 4 ? faceColors[4] : GRAY); // 2: base-top-right
    AddVertex({-halfBase, 0,  halfBase}, {0, -1, 0}, {0, 1}, faceColors.size() > 4 ? faceColors[4] : GRAY); // 3: base-top-left

    // Apex vertex (top of pyramid)
    AddVertex({0, height, 0}, {0, 1, 0}, {0.5f, 0.5f}, WHITE); // 4: apex

    // Base (bottom face)
    AddQuad(0, 1, 2, 3);

    // Front face (red) - counter-clockwise winding for Raylib
    Color frontColor = faceColors.size() > 0 ? faceColors[0] : RED;
    vertices_[0].color = frontColor;
    vertices_[1].color = frontColor;
    vertices_[4].color = frontColor;
    AddTriangle(0, 4, 1); // Changed from (0, 1, 4) to (0, 4, 1) for counter-clockwise

    // Right face (green) - counter-clockwise winding
    Color rightColor = faceColors.size() > 1 ? faceColors[1] : GREEN;
    vertices_[1].color = rightColor;
    vertices_[2].color = rightColor;
    AddTriangle(1, 4, 2); // Changed from (1, 2, 4) to (1, 4, 2) for counter-clockwise

    // Back face (blue) - counter-clockwise winding
    Color backColor = faceColors.size() > 2 ? faceColors[2] : BLUE;
    vertices_[2].color = backColor;
    vertices_[3].color = backColor;
    AddTriangle(2, 4, 3); // Changed from (2, 3, 4) to (2, 4, 3) for counter-clockwise

    // Left face (yellow) - counter-clockwise winding
    Color leftColor = faceColors.size() > 3 ? faceColors[3] : YELLOW;
    vertices_[3].color = leftColor;
    vertices_[0].color = leftColor;
    AddTriangle(3, 4, 0); // Changed from (3, 0, 4) to (3, 4, 0) for counter-clockwise

    LOG_INFO("Created pyramid mesh with " + std::to_string(vertices_.size()) + " vertices, " +
             std::to_string(triangles_.size()) + " triangles");
}

void MeshComponent::OnAttach() {
    LOG_INFO("Mesh component attached to entity");
}

void MeshComponent::OnDetach() {
    LOG_INFO("Mesh component detached from entity");
    Clear();
}
