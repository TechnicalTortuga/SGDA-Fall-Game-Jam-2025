#pragma once

#include <raylib.h>
#include <imgui.h>
#include <vector>

class GizmoManager {
public:
    enum class GizmoMode {
        NONE,        // No gizmo active
        TRANSLATE,   // Move objects (G key)
        ROTATE,      // Rotate objects (R key)
        SCALE        // Scale objects (S key)
    };

    enum class GizmoAxis {
        NONE,
        X,           // Red axis
        Y,           // Green axis
        Z,           // Blue axis
        XY,          // Yellow plane
        XZ,          // Magenta plane
        YZ,          // Cyan plane
        XYZ          // Center cube (all axes)
    };

    struct GizmoState {
        bool isActive = false;
        GizmoAxis activeAxis = GizmoAxis::NONE;
        Vector3 startPosition = {0, 0, 0};
        Vector3 startValue = {0, 0, 0};     // Position, rotation, or scale
        ImVec2 startMousePos = {0, 0};
        Vector3 currentDelta = {0, 0, 0};
    };

    GizmoManager();
    ~GizmoManager() = default;

    // Mode management
    void SetGizmoMode(GizmoMode mode);
    GizmoMode GetGizmoMode() const { return currentMode_; }
    void CycleGizmoMode(); // Cycle through modes
    const char* GetGizmoModeString() const;

    // Gizmo operations
    void SetGizmoPosition(Vector3 position) { gizmoPosition_ = position; }
    Vector3 GetGizmoPosition() const { return gizmoPosition_; }
    void SetGizmoScale(float scale) { gizmoScale_ = scale; }
    float GetGizmoScale() const { return gizmoScale_; }

    // Input handling
    void HandleInput();
    bool HandleMouseInput(ImVec2 mousePos, ImVec2 canvasPos, ImVec2 canvasSize, 
                         float zoomLevel, ImVec2 panOffset, Camera3D camera);

    // Interaction
    bool IsGizmoActive() const { return state_.isActive; }
    GizmoAxis GetActiveAxis() const { return state_.activeAxis; }
    Vector3 GetCurrentDelta() const { return state_.currentDelta; }

    // Rendering
    void RenderGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                     float zoomLevel, ImVec2 panOffset, Camera3D camera);

    // Coordinate system
    void SetLocalCoordinates(bool local) { useLocalCoordinates_ = local; }
    bool IsUsingLocalCoordinates() const { return useLocalCoordinates_; }

    // Snapping
    void SetSnapToGrid(bool snap) { snapToGrid_ = snap; }
    bool IsSnapToGrid() const { return snapToGrid_; }
    void SetGridSize(float size) { gridSize_ = size; }

    // Visual settings
    void SetGizmoSize(float size) { gizmoVisualSize_ = size; }
    void SetAxisThickness(float thickness) { axisThickness_ = thickness; }

private:
    GizmoMode currentMode_;
    Vector3 gizmoPosition_;
    float gizmoScale_;
    GizmoState state_;

    // Settings
    bool useLocalCoordinates_;
    bool snapToGrid_;
    float gridSize_;
    float gizmoVisualSize_;
    float axisThickness_;
    float handleSize_;
    float planeSize_;

    // Colors
    ImU32 xAxisColor_;      // Red
    ImU32 yAxisColor_;      // Green
    ImU32 zAxisColor_;      // Blue
    ImU32 xyPlaneColor_;    // Yellow
    ImU32 xzPlaneColor_;    // Magenta
    ImU32 yzPlaneColor_;    // Cyan
    ImU32 centerColor_;     // White/Gray
    ImU32 highlightColor_;  // Bright highlight

    // Helper methods
    void RenderTranslationGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                               float zoomLevel, ImVec2 panOffset, Camera3D camera);
    void RenderRotationGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                            float zoomLevel, ImVec2 panOffset, Camera3D camera);
    void RenderScaleGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                         float zoomLevel, ImVec2 panOffset, Camera3D camera);

    // Axis rendering
    void RenderAxis(ImDrawList* drawList, Vector3 start, Vector3 end, ImU32 color, 
                   float thickness, ImVec2 canvasPos, ImVec2 canvasSize, 
                   float zoomLevel, ImVec2 panOffset, Camera3D camera);
    void RenderAxisHandle(ImDrawList* drawList, Vector3 position, Vector3 direction, 
                         ImU32 color, ImVec2 canvasPos, ImVec2 canvasSize, 
                         float zoomLevel, ImVec2 panOffset, Camera3D camera);
    void RenderPlaneHandle(ImDrawList* drawList, Vector3 center, Vector3 normal, 
                          ImU32 color, ImVec2 canvasPos, ImVec2 canvasSize, 
                          float zoomLevel, ImVec2 panOffset, Camera3D camera);

    // Picking
    GizmoAxis PickGizmoComponent(ImVec2 mousePos, ImVec2 canvasPos, ImVec2 canvasSize, 
                                float zoomLevel, ImVec2 panOffset, Camera3D camera);
    bool IsPointNearLine(ImVec2 point, ImVec2 lineStart, ImVec2 lineEnd, float threshold);
    bool IsPointInCircle(ImVec2 point, ImVec2 center, float radius);

    // Coordinate transformation
    ImVec2 WorldToScreen(Vector3 worldPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                        float zoomLevel, ImVec2 panOffset, Camera3D camera);
    Vector3 ScreenToWorld(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                         float zoomLevel, ImVec2 panOffset, Camera3D camera);

    // Manipulation calculations
    Vector3 CalculateTranslationDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera);
    Vector3 CalculateRotationDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera);
    Vector3 CalculateScaleDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera);

    // Snapping
    float SnapValue(float value, float snapSize);
    Vector3 SnapVector(Vector3 value, float snapSize);

    // Constants
    static constexpr float DEFAULT_GIZMO_SIZE = 1.0f;
    static constexpr float DEFAULT_AXIS_THICKNESS = 3.0f;
    static constexpr float DEFAULT_HANDLE_SIZE = 0.15f;
    static constexpr float DEFAULT_PLANE_SIZE = 0.4f;
    static constexpr float PICK_THRESHOLD = 8.0f; // Pixels
};