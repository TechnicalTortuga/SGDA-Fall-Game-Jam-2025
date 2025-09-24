#include "GizmoManager.h"
#include <algorithm>
#include <cmath>

GizmoManager::GizmoManager()
    : currentMode_(GizmoMode::TRANSLATE), gizmoPosition_({0, 0, 0}), gizmoScale_(1.0f),
      useLocalCoordinates_(false), snapToGrid_(false), gridSize_(1.0f),
      gizmoVisualSize_(DEFAULT_GIZMO_SIZE), axisThickness_(DEFAULT_AXIS_THICKNESS),
      handleSize_(DEFAULT_HANDLE_SIZE), planeSize_(DEFAULT_PLANE_SIZE),
      xAxisColor_(IM_COL32(255, 0, 0, 255)),      // Red
      yAxisColor_(IM_COL32(0, 255, 0, 255)),      // Green
      zAxisColor_(IM_COL32(0, 0, 255, 255)),      // Blue
      xyPlaneColor_(IM_COL32(255, 255, 0, 128)),  // Yellow (semi-transparent)
      xzPlaneColor_(IM_COL32(255, 0, 255, 128)),  // Magenta (semi-transparent)
      yzPlaneColor_(IM_COL32(0, 255, 255, 128)),  // Cyan (semi-transparent)
      centerColor_(IM_COL32(200, 200, 200, 255)), // Light gray
      highlightColor_(IM_COL32(255, 255, 255, 255)) // White
{
}

void GizmoManager::SetGizmoMode(GizmoMode mode)
{
    if (currentMode_ != mode) {
        // Reset any active manipulation when switching modes
        state_.isActive = false;
        state_.activeAxis = GizmoAxis::NONE;
        currentMode_ = mode;
    }
}

void GizmoManager::CycleGizmoMode()
{
    switch (currentMode_) {
        case GizmoMode::NONE:
            SetGizmoMode(GizmoMode::TRANSLATE);
            break;
        case GizmoMode::TRANSLATE:
            SetGizmoMode(GizmoMode::ROTATE);
            break;
        case GizmoMode::ROTATE:
            SetGizmoMode(GizmoMode::SCALE);
            break;
        case GizmoMode::SCALE:
            SetGizmoMode(GizmoMode::TRANSLATE);
            break;
    }
}

const char* GizmoManager::GetGizmoModeString() const
{
    switch (currentMode_) {
        case GizmoMode::NONE:      return "NONE";
        case GizmoMode::TRANSLATE: return "TRANSLATE (G)";
        case GizmoMode::ROTATE:    return "ROTATE (R)";
        case GizmoMode::SCALE:     return "SCALE (S)";
        default:                   return "UNKNOWN";
    }
}

void GizmoManager::HandleInput()
{
    // Handle G/R/S keys for mode switching (industry standard)
    if (ImGui::IsKeyPressed(ImGuiKey_G)) {
        SetGizmoMode(GizmoMode::TRANSLATE);
    } else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        SetGizmoMode(GizmoMode::ROTATE);
    } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        SetGizmoMode(GizmoMode::SCALE);
    }

    // Handle Escape to cancel active manipulation
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && state_.isActive) {
        state_.isActive = false;
        state_.activeAxis = GizmoAxis::NONE;
    }

    // Handle coordinate system toggle (local/global)
    if (ImGui::IsKeyPressed(ImGuiKey_X)) {
        useLocalCoordinates_ = !useLocalCoordinates_;
    }
}

bool GizmoManager::HandleMouseInput(ImVec2 mousePos, ImVec2 canvasPos, ImVec2 canvasSize, 
                                   float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    if (currentMode_ == GizmoMode::NONE) return false;

    // Handle mouse down - start manipulation
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        GizmoAxis pickedAxis = PickGizmoComponent(mousePos, canvasPos, canvasSize, zoomLevel, panOffset, camera);
        if (pickedAxis != GizmoAxis::NONE) {
            state_.isActive = true;
            state_.activeAxis = pickedAxis;
            state_.startPosition = gizmoPosition_;
            state_.startValue = gizmoPosition_; // For translation
            state_.startMousePos = mousePos;
            state_.currentDelta = {0, 0, 0};
            return true; // Consumed input
        }
    }

    // Handle mouse drag - update manipulation
    if (state_.isActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mouseDelta = ImVec2(mousePos.x - state_.startMousePos.x, mousePos.y - state_.startMousePos.y);
        
        switch (currentMode_) {
            case GizmoMode::TRANSLATE:
                state_.currentDelta = CalculateTranslationDelta(mouseDelta, state_.activeAxis, camera);
                break;
            case GizmoMode::ROTATE:
                state_.currentDelta = CalculateRotationDelta(mouseDelta, state_.activeAxis, camera);
                break;
            case GizmoMode::SCALE:
                state_.currentDelta = CalculateScaleDelta(mouseDelta, state_.activeAxis, camera);
                break;
            default:
                break;
        }

        // Apply snapping if enabled
        if (snapToGrid_) {
            state_.currentDelta = SnapVector(state_.currentDelta, gridSize_);
        }

        return true; // Consumed input
    }

    // Handle mouse up - end manipulation
    if (state_.isActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        state_.isActive = false;
        state_.activeAxis = GizmoAxis::NONE;
        return true; // Consumed input
    }

    return false; // Did not consume input
}

void GizmoManager::RenderGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                              float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    if (currentMode_ == GizmoMode::NONE) return;

    switch (currentMode_) {
        case GizmoMode::TRANSLATE:
            RenderTranslationGizmo(drawList, canvasPos, canvasSize, zoomLevel, panOffset, camera);
            break;
        case GizmoMode::ROTATE:
            RenderRotationGizmo(drawList, canvasPos, canvasSize, zoomLevel, panOffset, camera);
            break;
        case GizmoMode::SCALE:
            RenderScaleGizmo(drawList, canvasPos, canvasSize, zoomLevel, panOffset, camera);
            break;
        default:
            break;
    }
}

void GizmoManager::RenderTranslationGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                         float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    Vector3 center = gizmoPosition_;
    float size = gizmoVisualSize_ * gizmoScale_;

    // Calculate axis endpoints
    Vector3 xEnd = {center.x + size, center.y, center.z};
    Vector3 yEnd = {center.x, center.y + size, center.z};
    Vector3 zEnd = {center.x, center.y, center.z + size};

    // Highlight active axis
    ImU32 xColor = (state_.activeAxis == GizmoAxis::X || state_.activeAxis == GizmoAxis::XY || state_.activeAxis == GizmoAxis::XZ) ? highlightColor_ : xAxisColor_;
    ImU32 yColor = (state_.activeAxis == GizmoAxis::Y || state_.activeAxis == GizmoAxis::XY || state_.activeAxis == GizmoAxis::YZ) ? highlightColor_ : yAxisColor_;
    ImU32 zColor = (state_.activeAxis == GizmoAxis::Z || state_.activeAxis == GizmoAxis::XZ || state_.activeAxis == GizmoAxis::YZ) ? highlightColor_ : zAxisColor_;

    // Render axes
    RenderAxis(drawList, center, xEnd, xColor, axisThickness_, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    RenderAxis(drawList, center, yEnd, yColor, axisThickness_, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    RenderAxis(drawList, center, zEnd, zColor, axisThickness_, canvasPos, canvasSize, zoomLevel, panOffset, camera);

    // Render axis handles (arrow heads)
    Vector3 xDir = {1, 0, 0};
    Vector3 yDir = {0, 1, 0};
    Vector3 zDir = {0, 0, 1};

    RenderAxisHandle(drawList, xEnd, xDir, xColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    RenderAxisHandle(drawList, yEnd, yDir, yColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    RenderAxisHandle(drawList, zEnd, zDir, zColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);

    // Render plane handles (for 2-axis movement)
    if (planeSize_ > 0) {
        Vector3 xyOffset = {size * planeSize_, size * planeSize_, 0};
        Vector3 xzOffset = {size * planeSize_, 0, size * planeSize_};
        Vector3 yzOffset = {0, size * planeSize_, size * planeSize_};

        ImU32 xyColor = (state_.activeAxis == GizmoAxis::XY) ? highlightColor_ : xyPlaneColor_;
        ImU32 xzColor = (state_.activeAxis == GizmoAxis::XZ) ? highlightColor_ : xzPlaneColor_;
        ImU32 yzColor = (state_.activeAxis == GizmoAxis::YZ) ? highlightColor_ : yzPlaneColor_;

        RenderPlaneHandle(drawList, Vector3{center.x + xyOffset.x, center.y + xyOffset.y, center.z}, 
                         Vector3{0, 0, 1}, xyColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);
        RenderPlaneHandle(drawList, Vector3{center.x + xzOffset.x, center.y, center.z + xzOffset.z}, 
                         Vector3{0, 1, 0}, xzColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);
        RenderPlaneHandle(drawList, Vector3{center.x, center.y + yzOffset.y, center.z + yzOffset.z}, 
                         Vector3{1, 0, 0}, yzColor, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    }

    // Render center handle
    ImVec2 centerScreen = WorldToScreen(center, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float centerRadius = handleSize_ * size * zoomLevel * 0.5f;
    ImU32 centerColorFinal = (state_.activeAxis == GizmoAxis::XYZ) ? highlightColor_ : centerColor_;
    drawList->AddCircleFilled(centerScreen, centerRadius, centerColorFinal);
}

void GizmoManager::RenderRotationGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                      float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    // TODO: Implement rotation gizmo rendering
    // For now, draw a simple indicator
    ImVec2 centerScreen = WorldToScreen(gizmoPosition_, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float radius = gizmoVisualSize_ * gizmoScale_ * zoomLevel;
    drawList->AddCircle(centerScreen, radius, IM_COL32(255, 255, 0, 255), 32, 2.0f);
}

void GizmoManager::RenderScaleGizmo(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                   float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    // TODO: Implement scale gizmo rendering
    // For now, draw a simple indicator
    ImVec2 centerScreen = WorldToScreen(gizmoPosition_, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float size = gizmoVisualSize_ * gizmoScale_ * zoomLevel * 0.5f;
    drawList->AddRect(ImVec2(centerScreen.x - size, centerScreen.y - size), 
                     ImVec2(centerScreen.x + size, centerScreen.y + size), 
                     IM_COL32(255, 0, 255, 255), 0.0f, 0, 2.0f);
}

void GizmoManager::RenderAxis(ImDrawList* drawList, Vector3 start, Vector3 end, ImU32 color, 
                             float thickness, ImVec2 canvasPos, ImVec2 canvasSize, 
                             float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    ImVec2 startScreen = WorldToScreen(start, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    ImVec2 endScreen = WorldToScreen(end, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    drawList->AddLine(startScreen, endScreen, color, thickness);
}

void GizmoManager::RenderAxisHandle(ImDrawList* drawList, Vector3 position, Vector3 direction, 
                                   ImU32 color, ImVec2 canvasPos, ImVec2 canvasSize, 
                                   float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    ImVec2 posScreen = WorldToScreen(position, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float handleRadius = handleSize_ * gizmoVisualSize_ * gizmoScale_ * zoomLevel;
    
    // Draw simple circle handle for now
    drawList->AddCircleFilled(posScreen, handleRadius, color);
    drawList->AddCircle(posScreen, handleRadius, IM_COL32(0, 0, 0, 255), 16, 1.0f); // Black outline
}

void GizmoManager::RenderPlaneHandle(ImDrawList* drawList, Vector3 center, Vector3 normal, 
                                    ImU32 color, ImVec2 canvasPos, ImVec2 canvasSize, 
                                    float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    ImVec2 centerScreen = WorldToScreen(center, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float planeHandleSize = planeSize_ * gizmoVisualSize_ * gizmoScale_ * zoomLevel;
    
    // Draw simple square handle for now
    drawList->AddRectFilled(ImVec2(centerScreen.x - planeHandleSize, centerScreen.y - planeHandleSize),
                           ImVec2(centerScreen.x + planeHandleSize, centerScreen.y + planeHandleSize),
                           color);
}

GizmoManager::GizmoAxis GizmoManager::PickGizmoComponent(ImVec2 mousePos, ImVec2 canvasPos, ImVec2 canvasSize, 
                                                        float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    Vector3 center = gizmoPosition_;
    float size = gizmoVisualSize_ * gizmoScale_;

    // Check handles first (highest priority)
    ImVec2 centerScreen = WorldToScreen(center, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    float centerRadius = handleSize_ * size * zoomLevel * 0.5f;
    if (IsPointInCircle(mousePos, centerScreen, centerRadius)) {
        return GizmoAxis::XYZ;
    }

    // Check axis handles
    Vector3 xEnd = {center.x + size, center.y, center.z};
    Vector3 yEnd = {center.x, center.y + size, center.z};
    Vector3 zEnd = {center.x, center.y, center.z + size};

    ImVec2 xEndScreen = WorldToScreen(xEnd, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    ImVec2 yEndScreen = WorldToScreen(yEnd, canvasPos, canvasSize, zoomLevel, panOffset, camera);
    ImVec2 zEndScreen = WorldToScreen(zEnd, canvasPos, canvasSize, zoomLevel, panOffset, camera);

    float handleRadius = handleSize_ * size * zoomLevel;

    if (IsPointInCircle(mousePos, xEndScreen, handleRadius)) return GizmoAxis::X;
    if (IsPointInCircle(mousePos, yEndScreen, handleRadius)) return GizmoAxis::Y;
    if (IsPointInCircle(mousePos, zEndScreen, handleRadius)) return GizmoAxis::Z;

    // Check axes
    if (IsPointNearLine(mousePos, centerScreen, xEndScreen, PICK_THRESHOLD)) return GizmoAxis::X;
    if (IsPointNearLine(mousePos, centerScreen, yEndScreen, PICK_THRESHOLD)) return GizmoAxis::Y;
    if (IsPointNearLine(mousePos, centerScreen, zEndScreen, PICK_THRESHOLD)) return GizmoAxis::Z;

    return GizmoAxis::NONE;
}

bool GizmoManager::IsPointNearLine(ImVec2 point, ImVec2 lineStart, ImVec2 lineEnd, float threshold)
{
    // Calculate distance from point to line segment
    float lineLength = sqrtf((lineEnd.x - lineStart.x) * (lineEnd.x - lineStart.x) + 
                            (lineEnd.y - lineStart.y) * (lineEnd.y - lineStart.y));
    if (lineLength < 0.001f) return false;

    float t = ((point.x - lineStart.x) * (lineEnd.x - lineStart.x) + 
               (point.y - lineStart.y) * (lineEnd.y - lineStart.y)) / (lineLength * lineLength);
    t = std::max(0.0f, std::min(1.0f, t));

    ImVec2 projection = ImVec2(lineStart.x + t * (lineEnd.x - lineStart.x),
                              lineStart.y + t * (lineEnd.y - lineStart.y));
    
    float distance = sqrtf((point.x - projection.x) * (point.x - projection.x) + 
                          (point.y - projection.y) * (point.y - projection.y));
    
    return distance <= threshold;
}

bool GizmoManager::IsPointInCircle(ImVec2 point, ImVec2 center, float radius)
{
    float distance = sqrtf((point.x - center.x) * (point.x - center.x) + 
                          (point.y - center.y) * (point.y - center.y));
    return distance <= radius;
}

ImVec2 GizmoManager::WorldToScreen(Vector3 worldPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                                  float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    // For orthographic views, use simple 2D projection
    // For perspective view, would need proper 3D to 2D projection
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    
    // Simple 2D projection (assuming top-down view for now)
    float screenX = center.x + (worldPos.x - panOffset.x) * zoomLevel;
    float screenY = center.y + (worldPos.y - panOffset.y) * zoomLevel;
    
    return ImVec2(screenX, screenY);
}

Vector3 GizmoManager::ScreenToWorld(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                                   float zoomLevel, ImVec2 panOffset, Camera3D camera)
{
    // Simple inverse of WorldToScreen
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    
    float worldX = (screenPos.x - center.x) / zoomLevel + panOffset.x;
    float worldY = (screenPos.y - center.y) / zoomLevel + panOffset.y;
    
    return Vector3{worldX, worldY, 0.0f}; // Z stays the same for now
}

Vector3 GizmoManager::CalculateTranslationDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera)
{
    Vector3 delta = {0, 0, 0};
    float sensitivity = 0.01f;

    switch (axis) {
        case GizmoAxis::X:
            delta.x = mouseDelta.x * sensitivity;
            break;
        case GizmoAxis::Y:
            delta.y = -mouseDelta.y * sensitivity; // Invert Y for natural feel
            break;
        case GizmoAxis::Z:
            delta.z = mouseDelta.x * sensitivity; // Map X mouse movement to Z
            break;
        case GizmoAxis::XY:
            delta.x = mouseDelta.x * sensitivity;
            delta.y = -mouseDelta.y * sensitivity;
            break;
        case GizmoAxis::XZ:
            delta.x = mouseDelta.x * sensitivity;
            delta.z = mouseDelta.y * sensitivity;
            break;
        case GizmoAxis::YZ:
            delta.y = -mouseDelta.y * sensitivity;
            delta.z = mouseDelta.x * sensitivity;
            break;
        case GizmoAxis::XYZ:
            // Free movement - for now, just XY
            delta.x = mouseDelta.x * sensitivity;
            delta.y = -mouseDelta.y * sensitivity;
            break;
        default:
            break;
    }

    return delta;
}

Vector3 GizmoManager::CalculateRotationDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera)
{
    // TODO: Implement rotation calculation
    return {0, 0, 0};
}

Vector3 GizmoManager::CalculateScaleDelta(ImVec2 mouseDelta, GizmoAxis axis, Camera3D camera)
{
    // TODO: Implement scale calculation
    return {0, 0, 0};
}

float GizmoManager::SnapValue(float value, float snapSize)
{
    if (snapSize <= 0.0f) return value;
    return roundf(value / snapSize) * snapSize;
}

Vector3 GizmoManager::SnapVector(Vector3 value, float snapSize)
{
    return Vector3{
        SnapValue(value.x, snapSize),
        SnapValue(value.y, snapSize),
        SnapValue(value.z, snapSize)
    };
}