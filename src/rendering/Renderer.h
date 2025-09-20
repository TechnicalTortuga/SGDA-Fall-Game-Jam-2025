#pragma once

#include "raylib.h"
#include "rlgl.h"
#include <vector>
#include <memory>
#include "../world/BSPTree.h"
#include "../ecs/Components/Collidable.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Systems/AssetSystem.h"

class Entity;
class Position;
class Sprite;

// Different types of renderable objects
enum class RenderType {
    SPRITE_2D,      // 2D sprite as billboard in 3D space
    PRIMITIVE_3D,   // 3D primitive (cube, sphere, etc.)
    MESH_3D,        // 3D mesh/model
    DEBUG           // Debug visualization
};

// Types of 3D primitives supported by Raylib
enum class PrimitiveType {
    CUBE,           // Standard cube
    SPHERE,         // Sphere with optional rings/slices
    CYLINDER,       // Cylinder/cone with optional base/end radius
    CAPSULE,        // Capsule with radius and height
    PLANE,          // XZ plane
    TRIANGLE,       // Single triangle
    LINE,           // 3D line
    POINT,          // Single point
    CIRCLE,         // 3D circle
    RAY             // Ray line
};

struct RenderCommand {
    Entity* entity;
    Position* position;
    Sprite* sprite;
    MeshComponent* mesh;
    RenderType type;
    float depth; // For sorting (higher = rendered later)

    // 3D primitive properties
    struct {
        PrimitiveType type;
        Vector3 size;           // For cubes, cylinders, etc. (width, height, length)
        float radius;           // For spheres, cylinders, capsules, circles
        float topRadius;        // For cylinders/cones (start radius)
        float bottomRadius;     // For cylinders/cones (end radius)
        float height;           // For cylinders, capsules
        int rings;              // For spheres
        int slices;             // For spheres, cylinders, capsules
        Vector3 startPos;       // For lines, rays, triangles
        Vector3 endPos;         // For lines, rays
        Vector3 v1, v2, v3;     // For triangles
        Vector3 rotationAxis;   // For circles
        float rotationAngle;    // For circles
        Color color;
        bool wireframe;         // Whether to draw wireframe version
    } primitive;

    RenderCommand(Entity* e, Position* p, Sprite* s, MeshComponent* m = nullptr, RenderType t = RenderType::SPRITE_2D)
        : entity(e), position(p), sprite(s), mesh(m), type(t), depth(0.0f) {
        primitive.type = PrimitiveType::CUBE;
        primitive.size = {1.0f, 1.0f, 1.0f};
        primitive.radius = 1.0f;
        primitive.topRadius = 1.0f;
        primitive.bottomRadius = 1.0f;
        primitive.height = 1.0f;
        primitive.rings = 16;
        primitive.slices = 16;
        primitive.startPos = {0.0f, 0.0f, 0.0f};
        primitive.endPos = {0.0f, 0.0f, 0.0f};
        primitive.v1 = {0.0f, 0.0f, 0.0f};
        primitive.v2 = {0.0f, 0.0f, 0.0f};
        primitive.v3 = {0.0f, 0.0f, 0.0f};
        primitive.rotationAxis = {0.0f, 1.0f, 0.0f};
        primitive.rotationAngle = 0.0f;
        primitive.color = WHITE;
        primitive.wireframe = false;
    }
};

class MeshSystem;

class Renderer {
public:
    Renderer();
    ~Renderer();

    // System access for resource operations
    void SetMeshSystem(MeshSystem* meshSystem) { meshSystem_ = meshSystem; }
    void SetAssetSystem(AssetSystem* assetSystem) { assetSystem_ = assetSystem; }

    // Main rendering methods
    void BeginFrame();
    void EndFrame();
    void Clear(Color color = BLACK);

    // Legacy sprite rendering (now uses dispatcher)
    void DrawSprite(const RenderCommand& command);

    // 2D Sprite rendering (billboards in 3D space)
    void DrawSprite2D(const RenderCommand& command);

    // 3D Primitive rendering
    void DrawPrimitive3D(const RenderCommand& command);

    // 3D Mesh/Model rendering
    void DrawMesh3D(const RenderCommand& command);

    // Main render command dispatcher
    void DrawRenderCommand(const RenderCommand& command);

    // Debug rendering
    void DrawDebugInfo();
    void DrawGrid(float spacing = 50.0f, Color color = LIGHTGRAY);

    // Raycasting for hit detection
    bool CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const;
    bool CastRayWorld(const Vector3& origin, const Vector3& direction, float maxDistance,
                     Vector3& hitPoint, Vector3& hitNormal) const;
    bool CastRayEntities(const Vector3& origin, const Vector3& direction, float maxDistance,
                        Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const;

    // Camera/viewport management
    void SetCameraPosition(float x, float y, float z);
    void SetCameraTarget(float x, float y, float z);
    void SetCameraRotation(float rotation);
    void SetCameraZoom(float zoom);
    void UpdateCameraToFollowPlayer(float playerX, float playerY, float playerZ);
    void UpdateCameraRotation(float mouseDeltaX, float mouseDeltaY, float deltaTime);
    void SetCameraAngles(float yaw, float pitch) { yaw_ = yaw; pitch_ = pitch; }
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    Vector3 GetCameraPosition() const { return camera_.position; }
    Vector3 GetCameraTarget() const { return camera_.target; }
    float GetCameraZoom() const { return camera_.fovy; }

    // Screen utilities
    int GetScreenWidth() const { return screenWidth_; }
    int GetScreenHeight() const { return screenHeight_; }
    Vector2 ScreenToWorld(Vector2 screenPos) const;
    Vector2 WorldToScreen(Vector2 worldPos) const;

    // Statistics access
    int GetSpritesRendered() const { return spritesRendered_; }
    int GetFramesRendered() const { return framesRendered_; }

    // World integration
    void SetBSPTree(BSPTree* bspTree) { bspTree_ = bspTree; }
    void SetRenderableEntities(const std::vector<Entity*>& entities) { renderableEntities_ = entities; }

private:
    Camera3D camera_;
    int screenWidth_;
    int screenHeight_;

    // System references
    MeshSystem* meshSystem_;
    AssetSystem* assetSystem_;

    // Camera rotation angles (standard FPS terminology)
    float yaw_;              // Horizontal rotation (left/right around Y axis)
    float pitch_;            // Vertical rotation (up/down from horizontal plane)
    float mouseSensitivity_;

    // World raycasting
    BSPTree* bspTree_;
    std::vector<Entity*> renderableEntities_;

    // Helper functions for camera calculations
    Vector3 SphericalToCartesian(float yaw, float pitch, float radius) const;
    void UpdateCameraFromAngles();

    // Render statistics
    int spritesRendered_;
    int framesRendered_;

    void UpdateScreenSize();
};
