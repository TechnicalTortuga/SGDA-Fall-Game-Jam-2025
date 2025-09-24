#include "GridManager.h"
#include <algorithm>
#include <cmath>
#include <vector>

GridManager::GridManager()
    : currentGridIndex_(DEFAULT_GRID_INDEX), snappingEnabled_(true)
{
}

void GridManager::IncreaseGridSize()
{
    if (currentGridIndex_ < static_cast<int>(GRID_SIZES.size()) - 1) {
        currentGridIndex_++;
    }
}

void GridManager::DecreaseGridSize()
{
    if (currentGridIndex_ > 0) {
        currentGridIndex_--;
    }
}

void GridManager::SetGridSize(int size)
{
    // Find the closest grid size
    for (int i = 0; i < static_cast<int>(GRID_SIZES.size()); ++i) {
        if (GRID_SIZES[i] == size) {
            currentGridIndex_ = i;
            return;
        }
    }
}

Vector3 GridManager::SnapToGrid(Vector3 position) const
{
    if (!snappingEnabled_) return position;
    
    float gridSize = static_cast<float>(GetCurrentGridSize());
    return Vector3{
        floorf(position.x / gridSize + 0.5f) * gridSize,
        floorf(position.y / gridSize + 0.5f) * gridSize,
        floorf(position.z / gridSize + 0.5f) * gridSize
    };
}

Vector2 GridManager::SnapToGrid(Vector2 position) const
{
    if (!snappingEnabled_) return position;
    
    float gridSize = static_cast<float>(GetCurrentGridSize());
    return Vector2{
        floorf(position.x / gridSize + 0.5f) * gridSize,
        floorf(position.y / gridSize + 0.5f) * gridSize
    };
}

ImVec2 GridManager::SnapToGrid(ImVec2 position) const
{
    if (!snappingEnabled_) return position;
    
    float gridSize = static_cast<float>(GetCurrentGridSize());
    return ImVec2(
        floorf(position.x / gridSize + 0.5f) * gridSize,
        floorf(position.y / gridSize + 0.5f) * gridSize
    );
}

void GridManager::DrawGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                          float zoomLevel, ImVec2 panOffset, ViewportType viewportType, bool enabled) const
{
    if (!enabled) return;
    
    // Get all visible grid levels for current zoom
    auto gridLevels = GetVisibleGridLevels(zoomLevel);
    
    // Draw each grid level from largest to smallest (back to front)
    for (auto it = gridLevels.rbegin(); it != gridLevels.rend(); ++it) {
        DrawGridLevel(drawList, canvasPos, canvasEnd, zoomLevel, panOffset, viewportType, *it);
    }

    // Draw axis reference lines
    DrawAxisLines(drawList, canvasPos, canvasEnd, zoomLevel, panOffset, viewportType);
}

bool GridManager::IsGridSizeVisible(int gridSize, float zoomLevel) const
{
    float gridPixels = static_cast<float>(gridSize) * zoomLevel;
    return gridPixels >= MIN_GRID_PIXELS;
}

int GridManager::GetDisplayGridSize() const
{
    return GetCurrentGridSize();
}

int GridManager::GetGridSizeAtIndex(int index)
{
    if (index >= 0 && index < static_cast<int>(GRID_SIZES.size())) {
        return GRID_SIZES[index];
    }
    return GRID_SIZES[DEFAULT_GRID_INDEX];
}

void GridManager::DrawGridLevel(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                               float zoomLevel, ImVec2 panOffset, ViewportType viewportType, const GridLevel& level) const
{
    ImVec2 canvasSize = ImVec2(canvasEnd.x - canvasPos.x, canvasEnd.y - canvasPos.y);
    
    // Adjust color alpha based on grid level
    ImU32 color = level.color;
    float alpha = level.alpha;
    float gridPixels = static_cast<float>(level.size) * zoomLevel;
    
    // Fade out grid as it gets too dense
    if (gridPixels < MIN_GRID_PIXELS * 2.0f) {
        alpha *= (gridPixels - MIN_GRID_PIXELS) / MIN_GRID_PIXELS;
        alpha = std::max(0.0f, std::min(1.0f, alpha));
    }
    
    // Apply alpha to color
    ImU32 alphaValue = static_cast<ImU32>(alpha * 255.0f);
    color = (color & 0x00FFFFFF) | (alphaValue << 24);
    
    if (viewportType == ViewportType::PERSPECTIVE_3D) {
        // For 3D perspective, don't draw grid (handled by skybox/axis lines)
        return;
    }

    // For orthographic views, draw grid lines representing 3D grid intersections
    // Calculate visible world bounds (what's actually visible on screen)
    ImVec2 worldCenter = ImVec2(-panOffset.x, -panOffset.y);
    float worldLeft = worldCenter.x - (canvasSize.x * 0.5f) / zoomLevel;
    float worldRight = worldCenter.x + (canvasSize.x * 0.5f) / zoomLevel;
    float worldTop = worldCenter.y - (canvasSize.y * 0.5f) / zoomLevel;
    float worldBottom = worldCenter.y + (canvasSize.y * 0.5f) / zoomLevel;

    // Extend bounds by grid size to ensure grid lines at edges are drawn
    float gridSize = static_cast<float>(level.size);
    worldLeft = std::floor(worldLeft / gridSize) * gridSize - gridSize;
    worldRight = std::ceil(worldRight / gridSize) * gridSize + gridSize;
    worldTop = std::floor(worldTop / gridSize) * gridSize - gridSize;
    worldBottom = std::ceil(worldBottom / gridSize) * gridSize + gridSize;

    // Draw grid lines based on viewport type
    if (viewportType == ViewportType::TOP_XY) {
        // Top view: X/Y plane - draw vertical and horizontal lines

        // Vertical lines (constant X) - iterate through X positions
        for (float x = worldLeft; x <= worldRight; x += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(x, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(x, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }

        // Horizontal lines (constant Y) - iterate through Y positions
        for (float y = worldTop; y <= worldBottom; y += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, y), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, y), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }
    }
    else if (viewportType == ViewportType::FRONT_XZ) {
        // Front view: X/Z plane - X horizontal, Z vertical

        // Horizontal lines (constant X) - iterate through X positions
        for (float x = worldLeft; x <= worldRight; x += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(x, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(x, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }

        // Vertical lines (constant Z) - iterate through Z positions
        for (float z = worldTop; z <= worldBottom; z += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, z), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, z), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }
    }
    else if (viewportType == ViewportType::SIDE_YZ) {
        // Side view: Y/Z plane - Y horizontal, Z vertical

        // Horizontal lines (constant Y) - iterate through Y positions
        for (float y = worldLeft; y <= worldRight; y += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(y, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(y, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }

        // Vertical lines (constant Z) - iterate through Z positions
        for (float z = worldTop; z <= worldBottom; z += gridSize) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, z), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, z), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, color, level.thickness);
        }
    }
}

ImVec2 GridManager::WorldToScreen(ImVec2 worldPos, ImVec2 canvasPos, ImVec2 canvasSize,
                                  float zoomLevel, ImVec2 panOffset) const
{
    // Convert world coordinates to screen coordinates
    // Center of canvas is (0,0) in world space when panOffset is (0,0)
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

    // Apply pan and zoom transformations
    float screenX = center.x + (worldPos.x - panOffset.x) * zoomLevel;
    float screenY = center.y + (worldPos.y - panOffset.y) * zoomLevel;

    return ImVec2(screenX, screenY);
}

void GridManager::DrawAxisLines(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                               float zoomLevel, ImVec2 panOffset, ViewportType viewportType) const
{
    ImVec2 canvasSize = ImVec2(canvasEnd.x - canvasPos.x, canvasEnd.y - canvasPos.y);

    if (viewportType == ViewportType::PERSPECTIVE_3D) {
        // For 3D perspective, axis lines are drawn separately
        return;
    }

    // Draw thicker axis reference lines at X=0, Y=0, Z=0
    const float axisThickness = 3.0f;

    // Calculate visible world bounds
    ImVec2 worldCenter = ImVec2(-panOffset.x, -panOffset.y);
    float worldLeft = worldCenter.x - (canvasSize.x * 0.5f) / zoomLevel;
    float worldRight = worldCenter.x + (canvasSize.x * 0.5f) / zoomLevel;
    float worldTop = worldCenter.y - (canvasSize.y * 0.5f) / zoomLevel;
    float worldBottom = worldCenter.y + (canvasSize.y * 0.5f) / zoomLevel;

    if (viewportType == ViewportType::TOP_XY) {
        // Top view: X/Y plane - X=0 (red vertical line), Y=0 (green horizontal line)
        if (0.0f >= worldLeft && 0.0f <= worldRight) {
            ImVec2 start = WorldToScreen(ImVec2(0.0f, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(0.0f, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(255, 0, 0, 255), axisThickness); // Red for X-axis
        }

        if (0.0f >= worldTop && 0.0f <= worldBottom) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(0, 255, 0, 255), axisThickness); // Green for Y-axis
        }
    }
    else if (viewportType == ViewportType::FRONT_XZ) {
        // Front view: X/Z plane - X=0 (red horizontal line), Z=0 (blue vertical line)
        if (0.0f >= worldLeft && 0.0f <= worldRight) {
            ImVec2 start = WorldToScreen(ImVec2(0.0f, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(0.0f, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(255, 0, 0, 255), axisThickness); // Red for X-axis
        }

        if (0.0f >= worldTop && 0.0f <= worldBottom) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(0, 0, 255, 255), axisThickness); // Blue for Z-axis
        }
    }
    else if (viewportType == ViewportType::SIDE_YZ) {
        // Side view: Y/Z plane - Y=0 (green horizontal line), Z=0 (blue vertical line)
        if (0.0f >= worldLeft && 0.0f <= worldRight) {
            ImVec2 start = WorldToScreen(ImVec2(0.0f, worldTop), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(0.0f, worldBottom), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(0, 255, 0, 255), axisThickness); // Green for Y-axis
        }

        if (0.0f >= worldTop && 0.0f <= worldBottom) {
            ImVec2 start = WorldToScreen(ImVec2(worldLeft, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            ImVec2 end = WorldToScreen(ImVec2(worldRight, 0.0f), canvasPos, canvasSize, zoomLevel, panOffset);
            drawList->AddLine(start, end, IM_COL32(0, 0, 255, 255), axisThickness); // Blue for Z-axis
        }
    }
}

ImVec2 GridManager::Project3DTo2D(Vector3 worldPos, ViewportType viewportType) const
{
    switch (viewportType) {
        case ViewportType::TOP_XY:
            return ImVec2(worldPos.x, worldPos.y);
        case ViewportType::FRONT_XZ:
            return ImVec2(worldPos.x, worldPos.z);
        case ViewportType::SIDE_YZ:
            return ImVec2(worldPos.y, worldPos.z);
        case ViewportType::PERSPECTIVE_3D:
        default:
            return ImVec2(worldPos.x, worldPos.y); // Default fallback
    }
}

Vector3 GridManager::Project2DTo3D(ImVec2 screenPos, float depth, ViewportType viewportType) const
{
    switch (viewportType) {
        case ViewportType::TOP_XY:
            return Vector3{screenPos.x, screenPos.y, depth};
        case ViewportType::FRONT_XZ:
            return Vector3{screenPos.x, depth, screenPos.y};
        case ViewportType::SIDE_YZ:
            return Vector3{depth, screenPos.x, screenPos.y};
        case ViewportType::PERSPECTIVE_3D:
        default:
            return Vector3{screenPos.x, screenPos.y, depth}; // Default fallback
    }
}

std::vector<GridManager::GridLevel> GridManager::GetVisibleGridLevels(float zoomLevel) const
{
    std::vector<GridLevel> levels;
    
    // Current grid size (primary grid)
    int primaryGrid = GetCurrentGridSize();
    if (IsGridSizeVisible(primaryGrid, zoomLevel)) {
        levels.push_back({
            primaryGrid,
            1.5f,                               // Thickness
            IM_COL32(140, 140, 140, 255),      // Color (medium gray)
            1.0f                                // Alpha
        });
    }
    
    // Secondary grid (4x larger)
    int secondaryGrid = primaryGrid * 4;
    if (secondaryGrid <= GRID_SIZES.back() && IsGridSizeVisible(secondaryGrid, zoomLevel)) {
        levels.push_back({
            secondaryGrid,
            2.0f,                               // Thickness
            IM_COL32(100, 100, 100, 255),      // Color (darker gray)
            0.8f                                // Alpha
        });
    }
    
    // Major grid (16x larger than primary, or next major power of 2)
    int majorGrid = primaryGrid * 16;
    if (majorGrid <= GRID_SIZES.back() && IsGridSizeVisible(majorGrid, zoomLevel)) {
        levels.push_back({
            majorGrid,
            2.5f,                               // Thickness
            IM_COL32(80, 80, 80, 255),         // Color (dark gray)
            0.9f                                // Alpha
        });
    }
    
    // Fine grid (1/4 size) - only show when zoomed in enough
    int fineGrid = primaryGrid / 4;
    if (fineGrid >= GRID_SIZES.front() && IsGridSizeVisible(fineGrid, zoomLevel)) {
        float fineGridPixels = static_cast<float>(fineGrid) * zoomLevel;
        if (fineGridPixels >= MIN_GRID_PIXELS * 2.0f) { // Only show if clearly visible
            levels.push_back({
                fineGrid,
                0.5f,                               // Thickness
                IM_COL32(160, 160, 160, 255),      // Color (light gray)
                0.6f                                // Alpha
            });
        }
    }
    
    return levels;
}