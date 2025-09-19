#include "RenderSystem.h"
#include "../Entity.h"
#include "../../ecs/Components/Position.h"
#include "../../ecs/Components/Sprite.h"
#include "../../utils/Logger.h"

RenderSystem::RenderSystem()
    : debugRendering_(true), gridEnabled_(true),  // Enable grid to see if 3D rendering works
      playerPosX_(0.0f), playerPosY_(0.0f), playerPosZ_(0.0f)
{
    LOG_INFO("RenderSystem constructed; signature set during Initialize()");
}

RenderSystem::~RenderSystem()
{
    LOG_INFO("RenderSystem destroyed");
}

void RenderSystem::Initialize()
{
    LOG_INFO("RenderSystem::Initialize - setting up Position+Sprite signature");

    // Set signature to only get entities with Position and Sprite components
    // This is done here instead of constructor so entities can be registered first
    SetSignature<Position, Sprite>();

    LOG_INFO("RenderSystem signature set (requires Position and Sprite)");
    LOG_INFO("RenderSystem initialized and ready for entity registration");
}

void RenderSystem::Update(float deltaTime)
{
    // Update phase - just collect and sort render commands, but don't render yet
    CollectRenderCommands();
    SortRenderCommands();
}

void RenderSystem::Render()
{
    LOG_DEBUG("RenderSystem::Render called");
    // Render phase - execute the actual rendering
    ExecuteRenderCommands();
    LOG_DEBUG("RenderSystem::Render completed");
}

void RenderSystem::CollectRenderCommands()
{
    renderCommands_.clear();

    // Get entities that match our Position+Sprite signature
    const auto& entities = GetEntities();

    LOG_DEBUG("RenderSystem: processing " + std::to_string(entities.size()) + " entities with Position+Sprite signature");
    if (entities.empty()) {
        LOG_ERROR("No entities match RenderSystem signature. Ensure entities register with required components.");
        return;
    }

    int processedCount = 0;
    int skippedCount = 0;

    for (Entity* entity : entities) {
        if (!entity) {
            LOG_WARNING("Null entity in RenderSystem");
            skippedCount++;
            continue;
        }

        if (!entity->IsActive()) {
            LOG_DEBUG("Skipping inactive entity " + std::to_string(entity->GetId()));
            skippedCount++;
            continue;
        }

        // At this point, entities should already have Position and Sprite components
        // (that's what the signature matching ensures)
        auto position = entity->GetComponent<Position>();
        auto sprite = entity->GetComponent<Sprite>();

        LOG_DEBUG("Processing entity " + std::to_string(entity->GetId()) +
                " - Position: (" + std::to_string(position->GetX()) + ", " +
                std::to_string(position->GetY()) + ", " + std::to_string(position->GetZ()) + ")");

        // Determine render type based on sprite properties
        RenderType renderType = RenderType::SPRITE_2D;
        if (!sprite->IsTextureLoaded()) {
            // No texture means it's a primitive (like a cube)
            renderType = RenderType::PRIMITIVE_3D;
        }

        RenderCommand command(entity, position, sprite, renderType);
        command.depth = position->GetZ();
        renderCommands_.push_back(command);

        LOG_DEBUG("Added entity " + std::to_string(entity->GetId()) +
                 " to render commands - Type: " + (renderType == RenderType::SPRITE_2D ? "2D Sprite" : "3D Primitive"));

        processedCount++;
    }

    LOG_DEBUG("RenderSystem summary:");
    LOG_DEBUG("  - Total entities: " + std::to_string(entities.size()));
    LOG_DEBUG("  - Processed: " + std::to_string(processedCount));
    LOG_DEBUG("  - Skipped: " + std::to_string(skippedCount));
    LOG_DEBUG("  - Final render commands: " + std::to_string(renderCommands_.size()));
}

void RenderSystem::SortRenderCommands()
{
    // Sort by depth (lower depth = rendered first)
    std::sort(renderCommands_.begin(), renderCommands_.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.depth < b.depth;
        });
}

void RenderSystem::ExecuteRenderCommands()
{
    LOG_DEBUG("RenderSystem: executing " + std::to_string(renderCommands_.size()) + " render commands");
    if (renderCommands_.empty()) {
        LOG_WARNING("No render commands to execute. Check entity registration.");
    }

    renderer_.BeginFrame();

    // Draw grid if enabled
    if (gridEnabled_) {
        renderer_.DrawGrid(50.0f, Fade(LIGHTGRAY, 0.3f));
    }

    // Draw all render commands using the dispatcher
    for (const auto& command : renderCommands_) {
        LOG_DEBUG("Rendering entity " + std::to_string(command.entity->GetId()) +
                 " at (" + std::to_string(command.position->GetX()) + ", " +
                 std::to_string(command.position->GetY()) + ", " + std::to_string(command.position->GetZ()) +
                 ") Type: " + (command.type == RenderType::SPRITE_2D ? "2D Sprite" : "3D Primitive"));
        renderer_.DrawRenderCommand(command);
    }

    renderer_.EndFrame();

    // Draw minimal debug info (optional overlay)
    if (debugRendering_) {
        DrawText(TextFormat("Meshes: %d", renderer_.GetSpritesRendered()), 10, 10, 16, YELLOW);
        auto camPos = renderer_.GetCameraPosition();
        DrawText(TextFormat("Cam: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z), 10, 30, 16, WHITE);
    }
}
