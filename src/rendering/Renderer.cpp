#include "Renderer.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Position.h"
#include "../ecs/Components/Sprite.h"
#include "utils/Logger.h"
#include <algorithm>

Renderer::Renderer()
    : screenWidth_(800), screenHeight_(600),
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
    if (!command.sprite || !command.position) {
        LOG_WARNING("RenderCommand missing sprite or position");
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

// 3D Primitive rendering (cubes, spheres, etc.)
void Renderer::DrawPrimitive3D(const RenderCommand& command)
{
    if (!command.sprite || !command.position) {
        return;
    }

    // Get world position in 3D space
    Vector3 worldPos = command.position->GetPosition();

    // Get rendering properties
    float scale = command.sprite->GetScale();
    Color tint = command.sprite->GetTint();

    // For now, we only support cubes as primitives
    // Later we can extend this based on the primitive properties in RenderCommand
    if (!command.sprite->IsTextureLoaded()) {
        // Draw 3D cube primitive
        float size = 2.0f * scale;  // Reasonable 2-unit cube

        LOG_INFO("Drawing 3D cube at (" + std::to_string(worldPos.x) + ", " +
                 std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) +
                 ") with size " + std::to_string(size) + " color: R=" + std::to_string(tint.r) +
                 " G=" + std::to_string(tint.g) + " B=" + std::to_string(tint.b));

        // Draw filled cube
        DrawCube(worldPos, size, size, size, tint);
        // Draw wireframe outline for better visibility
        DrawCubeWires(worldPos, size, size, size, BLACK);

        LOG_INFO("3D cube drawn successfully");
    } else {
        LOG_WARNING("DrawPrimitive3D called on entity with texture");
    }
}

// 3D Mesh/Model rendering (placeholder for future implementation)
void Renderer::DrawMesh3D(const RenderCommand& command)
{
    LOG_WARNING("DrawMesh3D not yet implemented");
    // Future implementation will handle 3D models and meshes
    // This would load and render .obj files, etc.
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
