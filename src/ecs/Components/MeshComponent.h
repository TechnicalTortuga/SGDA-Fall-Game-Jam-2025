#pragma once

#include "../Component.h"
#include "raylib.h"
#include <vector>
#include <string>

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

    // Primitive shape information (for Raylib integration)
    enum class MeshType { MODEL, PRIMITIVE, COMPOSITE };
    MeshType meshType = MeshType::PRIMITIVE;
    std::string primitiveShape = "cube";
    
    // Composite mesh reference (lightweight - just an ID)
    uint64_t compositeMeshId = 0;     // References composite mesh definition in MeshSystem

    // Simple state flags (pure data)
    bool isActive = true;
    bool needsRebuild = false;
    bool isStatic = false;                // For optimization (static meshes don't change)

    // Instancing support (temporary override for instanced rendering)
    bool isInstanced = false;
    Vector3 instancePosition = {0.0f, 0.0f, 0.0f};
    Quaternion instanceRotation = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 instanceScale = {1.0f, 1.0f, 1.0f};
};
