#include "Renderer.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Position.h"
#include "../ecs/Components/Sprite.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Components/TransformComponent.h"
#include "../ecs/Systems/MeshSystem.h"
#include "../ecs/Systems/AssetSystem.h"
#include "utils/Logger.h"
#include <algorithm>

Renderer::Renderer()
    : screenWidth_(800), screenHeight_(600),
      meshSystem_(nullptr), assetSystem_(nullptr),
      spritesRendered_(0), framesRendered_(0),
      yaw_(0.0f), pitch_(0.0f), mouseSensitivity_(0.15f),
      bspTree_(nullptr)
{
    UpdateScreenSize();

    // Initialize Raylib Camera3D with default values
    // Camera will be positioned properly when UpdateCameraToFollowPlayer is called
    camera_.position = {0.0f, 5.0f, 10.0f};   // Initial camera position
    camera_.target = {0.0f, 0.0f, 0.0f};      // Initial target (origin)
    camera_.up = {0.0f, 1.0f, 0.0f};          // Up vector
    camera_.fovy = 45.0f;                      // Field of view
    camera_.projection = CAMERA_PERSPECTIVE;   // Perspective projection

    LOG_INFO("Renderer initialized with Camera3D");
}

Renderer::~Renderer()
{
    LOG_INFO("Renderer destroyed");
}

void Renderer::BeginFrame()
{
    spritesRendered_ = 0;
    UpdateScreenSize();

    // Begin camera mode for 3D rendering
    BeginMode3D(camera_);
}

void Renderer::EndFrame()
{
    // End camera mode
    EndMode3D();

    framesRendered_++;
}

void Renderer::Clear(Color color)
{
    ClearBackground(color);
}

// Main render command dispatcher - routes to appropriate rendering method
void Renderer::DrawRenderCommand(const RenderCommand& command)
{
    if (!command.position) {
        LOG_WARNING("RenderCommand missing position");
        return;
    }

    // For mesh rendering, we don't need a sprite
    if (command.type != RenderType::MESH_3D && !command.sprite) {
        LOG_WARNING("RenderCommand missing sprite for non-mesh rendering");
        return;
    }

    switch (command.type) {
        case RenderType::SPRITE_2D:
            DrawSprite2D(command);
            break;
        case RenderType::PRIMITIVE_3D:
            DrawPrimitive3D(command);
            break;
        case RenderType::MESH_3D:
            DrawMesh3D(command);
            break;
        case RenderType::DEBUG:
            // Debug rendering handled separately
            break;
        default:
            LOG_WARNING("Unknown render type");
            break;
    }

    spritesRendered_++;
}

// Legacy method - now uses the dispatcher
void Renderer::DrawSprite(const RenderCommand& command)
{
    DrawRenderCommand(command);
}

// 2D Sprite rendering (billboards in 3D space)
void Renderer::DrawSprite2D(const RenderCommand& command)
{
    if (!command.sprite || !command.position) {
        return;
    }

    // Get world position in 3D space
    Vector3 worldPos = command.position->GetPosition();

    // Get rendering properties
    float scale = command.sprite->GetScale();
    Color tint = command.sprite->GetTint();

    if (command.sprite->IsTextureLoaded()) {
        // Draw texture-based sprite as billboard
        Texture2D texture = command.sprite->GetTexture();

        // Calculate billboard size based on texture and scale
        Vector2 size = {texture.width * scale, texture.height * scale};

        // Draw as billboard (always faces camera)
        DrawBillboard(camera_, texture, worldPos, size.x, tint);

        // Draw decal overlay if present
        Texture2D decal = command.sprite->GetDecalOverlay();
        if (decal.id != 0) {
            DrawBillboard(camera_, decal, worldPos, size.x, WHITE);
        }

        LOG_INFO("Rendered 2D sprite billboard at (" + std::to_string(worldPos.x) + ", " +
                 std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
    } else {
        LOG_WARNING("DrawSprite2D called on entity without texture");
    }
}

// 3D Primitive rendering (cubes, spheres, cylinders, etc.)
void Renderer::DrawPrimitive3D(const RenderCommand& command)
{
    if (!command.position) {
        return;
    }

    // Get world position in 3D space
    Vector3 worldPos = command.position->GetPosition();

    // Get primitive properties
    const auto& prim = command.primitive;
    Color color = prim.color;

    // Apply tint from sprite if available
    if (command.sprite) {
        Color tint = command.sprite->GetTint();
        color = Color{
            (unsigned char)((float)prim.color.r * (float)tint.r / 255.0f),
            (unsigned char)((float)prim.color.g * (float)tint.g / 255.0f),
            (unsigned char)((float)prim.color.b * (float)tint.b / 255.0f),
            (unsigned char)((float)prim.color.a * (float)tint.a / 255.0f)
        };
    }

    // Draw the appropriate primitive based on type
    switch (prim.type) {
        case PrimitiveType::CUBE: {
            if (prim.wireframe) {
                if (prim.size.x == prim.size.y && prim.size.y == prim.size.z) {
                    DrawCubeWires(worldPos, prim.size.x, prim.size.y, prim.size.z, color);
                } else {
                    DrawCubeWiresV(worldPos, prim.size, color);
                }
            } else {
                if (prim.size.x == prim.size.y && prim.size.y == prim.size.z) {
                    DrawCube(worldPos, prim.size.x, prim.size.y, prim.size.z, color);
                } else {
                    DrawCubeV(worldPos, prim.size, color);
                }
            }
            LOG_INFO("Drew 3D cube at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::SPHERE: {
            if (prim.wireframe) {
                DrawSphereWires(worldPos, prim.radius, prim.rings, prim.slices, color);
            } else {
                if (prim.rings == 16 && prim.slices == 16) {
                    DrawSphere(worldPos, prim.radius, color);
                } else {
                    DrawSphereEx(worldPos, prim.radius, prim.rings, prim.slices, color);
                }
            }
            LOG_INFO("Drew 3D sphere at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::CYLINDER: {
            if (prim.wireframe) {
                if (prim.topRadius == prim.bottomRadius) {
                    DrawCylinderWires(worldPos, prim.topRadius, prim.bottomRadius, prim.height, prim.slices, color);
                } else {
                    // DrawCylinderWiresEx(startPos, endPos, startRadius, endRadius, sides, color)
                    Vector3 endPos = Vector3Add(worldPos, {0.0f, prim.height, 0.0f});
                    DrawCylinderWiresEx(worldPos, endPos, prim.topRadius, prim.bottomRadius, prim.slices, color);
                }
            } else {
                if (prim.topRadius == prim.bottomRadius) {
                    DrawCylinder(worldPos, prim.topRadius, prim.bottomRadius, prim.height, prim.slices, color);
                } else {
                    // DrawCylinderEx(startPos, endPos, startRadius, endRadius, sides, color)
                    Vector3 endPos = Vector3Add(worldPos, {0.0f, prim.height, 0.0f});
                    DrawCylinderEx(worldPos, endPos, prim.topRadius, prim.bottomRadius, prim.slices, color);
                }
            }
            LOG_INFO("Drew 3D cylinder/cone at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::CAPSULE: {
            if (prim.wireframe) {
                DrawCapsuleWires(worldPos, prim.endPos, prim.radius, prim.slices, prim.rings, color);
            } else {
                DrawCapsule(worldPos, prim.endPos, prim.radius, prim.slices, prim.rings, color);
            }
            LOG_INFO("Drew 3D capsule at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::PLANE: {
            DrawPlane(worldPos, {prim.size.x, prim.size.z}, color);
            LOG_INFO("Drew 3D plane at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::TRIANGLE: {
            DrawTriangle3D(prim.v1, prim.v2, prim.v3, color);
            LOG_INFO("Drew 3D triangle at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::LINE: {
            DrawLine3D(prim.startPos, prim.endPos, color);
            LOG_INFO("Drew 3D line from (" + std::to_string(prim.startPos.x) + ", " +
                     std::to_string(prim.startPos.y) + ", " + std::to_string(prim.startPos.z) +
                     ") to (" + std::to_string(prim.endPos.x) + ", " +
                     std::to_string(prim.endPos.y) + ", " + std::to_string(prim.endPos.z) + ")");
            break;
        }

        case PrimitiveType::POINT: {
            DrawPoint3D(prim.startPos, color);
            LOG_INFO("Drew 3D point at (" + std::to_string(prim.startPos.x) + ", " +
                     std::to_string(prim.startPos.y) + ", " + std::to_string(prim.startPos.z) + ")");
            break;
        }

        case PrimitiveType::CIRCLE: {
            DrawCircle3D(worldPos, prim.radius, prim.rotationAxis, prim.rotationAngle, color);
            LOG_INFO("Drew 3D circle at (" + std::to_string(worldPos.x) + ", " +
                     std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
            break;
        }

        case PrimitiveType::RAY: {
            Ray ray = {prim.startPos, Vector3Normalize(Vector3Subtract(prim.endPos, prim.startPos))};
            DrawRay(ray, color);
            LOG_INFO("Drew 3D ray from (" + std::to_string(prim.startPos.x) + ", " +
                     std::to_string(prim.startPos.y) + ", " + std::to_string(prim.startPos.z) +
                     ") in direction (" + std::to_string(ray.direction.x) + ", " +
                     std::to_string(ray.direction.y) + ", " + std::to_string(ray.direction.z) + ")");
            break;
        }

        default:
            LOG_WARNING("Unknown primitive type in DrawPrimitive3D");
            break;
    }
}

// Mesh rendering
void Renderer::DrawMesh3D(const RenderCommand& command)
{
    if (!command.position || !command.mesh) {
        return;
    }

    Vector3 worldPos = command.position->GetPosition();
    const auto& mesh = *command.mesh;

    // Check for TransformComponent for rotation (simplified approach)
    auto transform = command.entity->GetComponent<TransformComponent>();
    bool hasRotation = false;
    Matrix rotationMatrix = MatrixIdentity();

    if (transform) {
        // Convert quaternion to rotation matrix for vertex transformation
        Vector3 rotationAxis;
        float rotationAngle;
        QuaternionToAxisAngle(transform->rotation, &rotationAxis, &rotationAngle);

        if (rotationAngle != 0.0f) {
            hasRotation = true;
            rotationMatrix = MatrixRotate(rotationAxis, rotationAngle);
        }
    }

    // Set up material/texture if available
    if (meshSystem_) {
        int materialId = meshSystem_->GetMaterial(command.entity);
        if (materialId >= 0) {
            // For now, use the legacy material ID system
            // TODO: Replace with proper AssetSystem integration
            LOG_DEBUG("Mesh has material ID " + std::to_string(materialId));
        }

        // Try to get texture from MeshSystem (legacy compatibility)
        auto texture = meshSystem_->GetTexture(command.entity);
        if (texture.id != 0) {
            // Bind the texture for this mesh
            rlSetTexture(texture.id);

            // Set texture parameters for better quality (similar to WorldRenderer)
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);

            LOG_DEBUG("Bound texture ID " + std::to_string(texture.id) + " for mesh rendering");
        } else {
            // No texture available, render with vertex colors only
            LOG_DEBUG("Rendering mesh with vertex colors (no texture)");
        }
    } else {
        LOG_WARNING("MeshSystem not available for texture/material access");
    }

    // Render triangles
    rlBegin(RL_TRIANGLES);
    for (const auto& triangle : mesh.triangles) {
        const auto& v1 = mesh.vertices[triangle.v1];
        const auto& v2 = mesh.vertices[triangle.v2];
        const auto& v3 = mesh.vertices[triangle.v3];

        // Apply rotation and translation to vertices
        Vector3 pos1 = hasRotation ? Vector3Transform(v1.position, rotationMatrix) : v1.position;
        Vector3 pos2 = hasRotation ? Vector3Transform(v2.position, rotationMatrix) : v2.position;
        Vector3 pos3 = hasRotation ? Vector3Transform(v3.position, rotationMatrix) : v3.position;

        // Vertex 1
        rlColor4ub(v1.color.r, v1.color.g, v1.color.b, v1.color.a);
        rlTexCoord2f(v1.texCoord.x, v1.texCoord.y);
        rlVertex3f(worldPos.x + pos1.x, worldPos.y + pos1.y, worldPos.z + pos1.z);

        // Vertex 2
        rlColor4ub(v2.color.r, v2.color.g, v2.color.b, v2.color.a);
        rlTexCoord2f(v2.texCoord.x, v2.texCoord.y);
        rlVertex3f(worldPos.x + pos2.x, worldPos.y + pos2.y, worldPos.z + pos2.z);

        // Vertex 3
        rlColor4ub(v3.color.r, v3.color.g, v3.color.b, v3.color.a);
        rlTexCoord2f(v3.texCoord.x, v3.texCoord.y);
        rlVertex3f(worldPos.x + pos3.x, worldPos.y + pos3.y, worldPos.z + pos3.z);
    }
    rlEnd();

    LOG_INFO("Drew 3D mesh at (" + std::to_string(worldPos.x) + ", " +
             std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ") with " +
             std::to_string(mesh.triangles.size()) + " triangles");
}

void Renderer::DrawDebugInfo()
{
    // Don't draw debug info during 3D rendering - it will be drawn after EndMode3D()
    // This function is called while still in 3D mode, so we'll skip drawing here
}

void Renderer::DrawGrid(float spacing, Color color)
{
    // Draw a simple 3D grid on the ground plane (y = 0)
    int gridSize = 20;

    for (int i = -gridSize; i <= gridSize; i++) {
        // Draw lines parallel to Z axis
        Vector3 start = {(float)i * spacing, 0.0f, (float)-gridSize * spacing};
        Vector3 end = {(float)i * spacing, 0.0f, (float)gridSize * spacing};
        DrawLine3D(start, end, color);

        // Draw lines parallel to X axis
        start = {(float)-gridSize * spacing, 0.0f, (float)i * spacing};
        end = {(float)gridSize * spacing, 0.0f, (float)i * spacing};
        DrawLine3D(start, end, color);
    }
}

void Renderer::SetCameraPosition(float x, float y, float z)
{
    camera_.position.x = x;
    camera_.position.y = y;
    camera_.position.z = z;
}

void Renderer::SetCameraTarget(float x, float y, float z)
{
    camera_.target.x = x;
    camera_.target.y = y;
    camera_.target.z = z;
}

void Renderer::SetCameraRotation(float rotation)
{
    // For 3D camera, we can rotate around the up axis
    // This is a simple implementation - more complex rotation would need quaternion math
    camera_.position.x = camera_.target.x + cos(rotation) * 10.0f;
    camera_.position.z = camera_.target.z + sin(rotation) * 10.0f;
}

void Renderer::SetCameraZoom(float zoom)
{
    camera_.fovy = (zoom > 5.0f) ? zoom : 5.0f; // Minimum FOV to prevent extreme zoom
    camera_.fovy = (zoom < 120.0f) ? zoom : 120.0f; // Maximum FOV
}

void Renderer::UpdateCameraToFollowPlayer(float playerX, float playerY, float playerZ)
{
    // Set camera position to player position (first-person view)
    const float eyeHeight = 1.5f;  // Eye level above player position
    
    camera_.position.x = playerX;
    camera_.position.y = playerY + eyeHeight;
    camera_.position.z = playerZ;

    // Calculate look direction using yaw and pitch
    Vector3 lookDirection = SphericalToCartesian(yaw_, pitch_, 1.0f);
    
    // Set camera target based on look direction
    camera_.target.x = camera_.position.x + lookDirection.x;
    camera_.target.y = camera_.position.y + lookDirection.y;
    camera_.target.z = camera_.position.z + lookDirection.z;

    LOG_INFO("Camera updated - Position: (" + std::to_string(camera_.position.x) + ", " +
             std::to_string(camera_.position.y) + ", " + std::to_string(camera_.position.z) +
             ") Target: (" + std::to_string(camera_.target.x) + ", " +
             std::to_string(camera_.target.y) + ", " + std::to_string(camera_.target.z) +
             ") Yaw: " + std::to_string(yaw_ * RAD2DEG) +
             "° Pitch: " + std::to_string(pitch_ * RAD2DEG) + "°");
}

void Renderer::UpdateCameraRotation(float mouseDeltaX, float mouseDeltaY, float deltaTime)
{
    LOG_DEBUG("UpdateCameraRotation called with mouseDelta: (" + std::to_string(mouseDeltaX) + ", " +
              std::to_string(mouseDeltaY) + ") deltaTime: " + std::to_string(deltaTime));

    // Apply delta time scaling for frame-rate independent movement
    // This ensures consistent camera speed regardless of frame rate
    float scaledDeltaX = mouseDeltaX * deltaTime * 60.0f; // Scale to 60 FPS baseline
    float scaledDeltaY = mouseDeltaY * deltaTime * 60.0f;

    // Update camera angles based on mouse input
    yaw_ += scaledDeltaX;    // Horizontal rotation (yaw) - already scaled by input system
    pitch_ -= scaledDeltaY;  // Vertical rotation (pitch) - inverted for natural up/down

    // Normalize yaw to [0, 2π] for consistency
    while (yaw_ > 2.0f * PI) yaw_ -= 2.0f * PI;
    while (yaw_ < 0.0f) yaw_ += 2.0f * PI;

    // Clamp pitch to prevent camera flipping (standard FPS behavior)
    // Allow looking up/down but prevent going completely upside down
    const float maxPitch = PI * 0.45f;  // About 81° up/down
    if (pitch_ > maxPitch) pitch_ = maxPitch;
    if (pitch_ < -maxPitch) pitch_ = -maxPitch;

    LOG_DEBUG("Camera angles updated - Yaw: " + std::to_string(yaw_ * RAD2DEG) +
              "° Pitch: " + std::to_string(pitch_ * RAD2DEG) + "° (scaled by deltaTime: " +
              std::to_string(deltaTime) + ")");
}

Vector2 Renderer::ScreenToWorld(Vector2 screenPos) const
{
    // For 3D camera, this is more complex. For now, return a simple approximation
    // In a real implementation, this would use ray casting from screen to world
    return {screenPos.x - screenWidth_ / 2.0f, screenPos.y - screenHeight_ / 2.0f};
}

Vector2 Renderer::WorldToScreen(Vector2 worldPos) const
{
    // For 3D camera, this is more complex. For now, return a simple approximation
    // In a real implementation, this would project 3D world position to 2D screen
    return {worldPos.x + screenWidth_ / 2.0f, worldPos.y + screenHeight_ / 2.0f};
}

void Renderer::UpdateScreenSize()
{
    screenWidth_ = GetScreenWidth();
    screenHeight_ = GetScreenHeight();
}

Vector3 Renderer::SphericalToCartesian(float yaw, float pitch, float radius) const
{
    // Convert yaw/pitch to cartesian coordinates
    // yaw: rotation around Y axis (0 = looking towards -Z, which is "forward" in our coordinate system)
    // pitch: rotation from horizontal plane (0 = horizontal, π/2 = up, -π/2 = down)
    
    Vector3 result;
    result.x = radius * cosf(pitch) * sinf(yaw);    // X component  
    result.y = radius * sinf(pitch);                // Y component (vertical)
    result.z = radius * cosf(pitch) * cosf(yaw);    // Z component
    
    // FIXED: Negate Z because we start looking towards negative Z (forward)
    result.z = -result.z;
    
    return result;
}

void Renderer::UpdateCameraFromAngles()
{
    // This function can be used to update camera when angles change
    // For now, it's called from UpdateCameraToFollowPlayer
    Vector3 lookDirection = SphericalToCartesian(yaw_, pitch_, 1.0f);

    camera_.target.x = camera_.position.x + lookDirection.x;
    camera_.target.y = camera_.position.y + lookDirection.y;
    camera_.target.z = camera_.position.z + lookDirection.z;
}

bool Renderer::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance,
                      Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const
{
    hitEntity = nullptr;
    float closestDistance = maxDistance;

    // Cast ray against BSP world first
    Vector3 worldHitPoint, worldHitNormal;
    if (CastRayWorld(origin, direction, maxDistance, worldHitPoint, worldHitNormal)) {
        Vector3 toHit = Vector3Subtract(worldHitPoint, origin);
        float distance = Vector3Length(toHit);
        if (distance < closestDistance) {
            closestDistance = distance;
            hitPoint = worldHitPoint;
            hitNormal = worldHitNormal;
        }
    }

    // Cast ray against entities
    Vector3 entityHitPoint, entityHitNormal;
    Entity* entityHit = nullptr;
    if (CastRayEntities(origin, direction, closestDistance, entityHitPoint, entityHitNormal, entityHit)) {
        hitPoint = entityHitPoint;
        hitNormal = entityHitNormal;
        hitEntity = entityHit;
    }

    return closestDistance < maxDistance;
}

bool Renderer::CastRayWorld(const Vector3& origin, const Vector3& direction, float maxDistance,
                           Vector3& hitPoint, Vector3& hitNormal) const
{
    if (!bspTree_) return false;

    Vector3 normalizedDir = Vector3Normalize(direction);
    float distance = bspTree_->CastRay(origin, normalizedDir, maxDistance);

    if (distance < maxDistance) {
        hitPoint = Vector3Add(origin, Vector3Scale(normalizedDir, distance));
        // For now, return a default normal (this could be improved with proper BSP ray casting)
        hitNormal = {0, 0, -1};
        return true;
    }

    return false;
}

bool Renderer::CastRayEntities(const Vector3& origin, const Vector3& direction, float maxDistance,
                              Vector3& hitPoint, Vector3& hitNormal, Entity*& hitEntity) const
{
    hitEntity = nullptr;
    float closestDistance = maxDistance;
    bool hit = false;

    Vector3 normalizedDir = Vector3Normalize(direction);

    for (Entity* entity : renderableEntities_) {
        if (!entity) continue;

        auto* position = entity->GetComponent<Position>();
        auto* collidable = entity->GetComponent<Collidable>();

        if (!position || !collidable) continue;

        Vector3 entityPos = position->GetPosition();
        AABB entityBounds = collidable->GetBounds();

        // Ray-AABB intersection test
        Vector3 invDir = {1.0f/normalizedDir.x, 1.0f/normalizedDir.y, 1.0f/normalizedDir.z};

        float tmin = 0.0f;
        float tmax = maxDistance;

        for (int i = 0; i < 3; ++i) {
            float originVal = (i == 0) ? origin.x : (i == 1) ? origin.y : origin.z;
            float dirVal = (i == 0) ? invDir.x : (i == 1) ? invDir.y : invDir.z;
            float minVal = (i == 0) ? entityBounds.min.x : (i == 1) ? entityBounds.min.y : entityBounds.min.z;
            float maxVal = (i == 0) ? entityBounds.max.x : (i == 1) ? entityBounds.max.y : entityBounds.max.z;

            float t1 = (minVal - originVal) * dirVal;
            float t2 = (maxVal - originVal) * dirVal;

            tmin = std::max(tmin, std::min(t1, t2));
            tmax = std::min(tmax, std::max(t1, t2));
        }

        if (tmax >= tmin && tmin < closestDistance && tmin >= 0) {
            Vector3 entityHitPoint = Vector3Add(origin, Vector3Scale(normalizedDir, tmin));
            if (tmin < closestDistance) {
                closestDistance = tmin;
                hitPoint = entityHitPoint;
                hitNormal = {0, 0, -1}; // Default normal, could be improved
                hitEntity = entity;
                hit = true;
            }
        }
    }

    return hit;
}
