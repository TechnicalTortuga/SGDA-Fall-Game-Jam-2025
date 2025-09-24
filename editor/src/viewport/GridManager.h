#pragma once

#include <raylib.h>
#include <imgui.h>
#include <array>

class GridManager {
public:
    enum class ViewportType {
        PERSPECTIVE_3D,    // 3D perspective view (top-left)
        TOP_XY,           // Top view: X/Y plane (bottom-left)
        FRONT_XZ,         // Front view: X/Z plane (top-right)
        SIDE_YZ           // Side view: Y/Z plane (bottom-right)
    };

    GridManager();
    ~GridManager() = default;

    // Grid size management
    void IncreaseGridSize();
    void DecreaseGridSize();
    void SetGridSize(int size);
    int GetCurrentGridSize() const { return GRID_SIZES[currentGridIndex_]; }
    int GetCurrentGridIndex() const { return currentGridIndex_; }

    // Snapping operations
    Vector3 SnapToGrid(Vector3 position) const;
    Vector2 SnapToGrid(Vector2 position) const;
    ImVec2 SnapToGrid(ImVec2 position) const;

    // Grid rendering
    void DrawGrid(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                  float zoomLevel, ImVec2 panOffset, ViewportType viewportType, bool enabled = true) const;

    // Grid size for display (in Hammer units)
    int GetDisplayGridSize() const;
    
    // Check if a grid size would be visible at the current zoom
    bool IsGridSizeVisible(int gridSize, float zoomLevel) const;

    // Grid settings
    void SetSnappingEnabled(bool enabled) { snappingEnabled_ = enabled; }
    bool IsSnappingEnabled() const { return snappingEnabled_; }

    // Get grid size at specific index
    static int GetGridSizeAtIndex(int index);
    static int GetGridSizeCount() { return GRID_SIZES.size(); }

private:
    // Source SDK Hammer standard grid sizes (powers of 2)
    static constexpr std::array<int, 8> GRID_SIZES = {1, 2, 4, 8, 16, 32, 64, 128};
    static constexpr int DEFAULT_GRID_INDEX = 6; // 64 units (Hammer standard)
    static constexpr float MIN_GRID_PIXELS = 8.0f; // Minimum pixels for grid visibility

    int currentGridIndex_;
    bool snappingEnabled_;

    // Grid rendering helpers
    struct GridLevel {
        int size;           // Grid size in world units
        float thickness;    // Line thickness
        ImU32 color;        // Line color
        float alpha;        // Alpha multiplier
    };

    void DrawGridLevel(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                       float zoomLevel, ImVec2 panOffset, ViewportType viewportType, const GridLevel& level) const;

    void DrawAxisLines(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                       float zoomLevel, ImVec2 panOffset, ViewportType viewportType) const;

    std::vector<GridLevel> GetVisibleGridLevels(float zoomLevel) const;

    // Coordinate transformation helper
    ImVec2 WorldToScreen(ImVec2 worldPos, ImVec2 canvasPos, ImVec2 canvasSize,
                         float zoomLevel, ImVec2 panOffset) const;

    // Helper functions for 3D to 2D projection
public:
    ImVec2 Project3DTo2D(Vector3 worldPos, ViewportType viewportType) const;
    Vector3 Project2DTo3D(ImVec2 screenPos, float depth, ViewportType viewportType) const;
};