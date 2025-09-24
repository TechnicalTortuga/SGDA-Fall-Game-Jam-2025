#include "SelectionManager.h"
#include <algorithm>
#include <cmath>

SelectionManager::SelectionManager()
    : currentMode_(SelectionMode::OBJECT),
      isBoxSelecting_(false), boxSelectionStart_(0, 0), boxSelectionEnd_(0, 0),
      objectHighlightColor_(IM_COL32(255, 165, 0, 255)),    // Orange
      vertexHighlightColor_(IM_COL32(255, 255, 0, 255)),    // Yellow
      edgeHighlightColor_(IM_COL32(0, 255, 255, 255)),      // Cyan
      faceHighlightColor_(IM_COL32(255, 0, 255, 255)),      // Magenta
      boxSelectionColor_(IM_COL32(100, 149, 237, 128)),     // Semi-transparent blue
      highlightThickness_(2.0f)
{
}

void SelectionManager::SetSelectionMode(SelectionMode mode)
{
    if (currentMode_ != mode) {
        // Clear selection when switching modes (industry standard behavior)
        ClearCurrentMode();
        currentMode_ = mode;
    }
}

void SelectionManager::CycleSelectionMode()
{
    switch (currentMode_) {
        case SelectionMode::OBJECT:
            SetSelectionMode(SelectionMode::VERTEX);
            break;
        case SelectionMode::VERTEX:
            SetSelectionMode(SelectionMode::EDGE);
            break;
        case SelectionMode::EDGE:
            SetSelectionMode(SelectionMode::FACE);
            break;
        case SelectionMode::FACE:
            SetSelectionMode(SelectionMode::OBJECT);
            break;
    }
}

const char* SelectionManager::GetSelectionModeString() const
{
    switch (currentMode_) {
        case SelectionMode::OBJECT: return "OBJECT";
        case SelectionMode::VERTEX: return "VERTEX";
        case SelectionMode::EDGE:   return "EDGE";
        case SelectionMode::FACE:   return "FACE";
        default:                    return "UNKNOWN";
    }
}

void SelectionManager::SelectObject(ObjectID objectId, SelectionType type)
{
    if (currentMode_ != SelectionMode::OBJECT) return;
    ApplySelection(selectedObjects_, objectId, type);
}

void SelectionManager::SelectVertex(VertexID vertexId, SelectionType type)
{
    if (currentMode_ != SelectionMode::VERTEX) return;
    ApplySelection(selectedVertices_, vertexId, type);
}

void SelectionManager::SelectEdge(EdgeID edgeId, SelectionType type)
{
    if (currentMode_ != SelectionMode::EDGE) return;
    ApplySelection(selectedEdges_, edgeId, type);
}

void SelectionManager::SelectFace(FaceID faceId, SelectionType type)
{
    if (currentMode_ != SelectionMode::FACE) return;
    ApplySelection(selectedFaces_, faceId, type);
}

void SelectionManager::SelectObjects(const std::vector<ObjectID>& objectIds, SelectionType type)
{
    if (currentMode_ != SelectionMode::OBJECT) return;
    ApplyMultiSelection(selectedObjects_, objectIds, type);
}

void SelectionManager::SelectVertices(const std::vector<VertexID>& vertexIds, SelectionType type)
{
    if (currentMode_ != SelectionMode::VERTEX) return;
    ApplyMultiSelection(selectedVertices_, vertexIds, type);
}

void SelectionManager::SelectEdges(const std::vector<EdgeID>& edgeIds, SelectionType type)
{
    if (currentMode_ != SelectionMode::EDGE) return;
    ApplyMultiSelection(selectedEdges_, edgeIds, type);
}

void SelectionManager::SelectFaces(const std::vector<FaceID>& faceIds, SelectionType type)
{
    if (currentMode_ != SelectionMode::FACE) return;
    ApplyMultiSelection(selectedFaces_, faceIds, type);
}

void SelectionManager::BeginBoxSelection(ImVec2 startPos)
{
    isBoxSelecting_ = true;
    boxSelectionStart_ = startPos;
    boxSelectionEnd_ = startPos;
}

void SelectionManager::UpdateBoxSelection(ImVec2 currentPos)
{
    if (isBoxSelecting_) {
        boxSelectionEnd_ = currentPos;
    }
}

void SelectionManager::EndBoxSelection(SelectionType type)
{
    if (!isBoxSelecting_) return;
    
    isBoxSelecting_ = false;
    
    // TODO: Implement box selection logic based on current mode
    // This would require viewport transformation and collision detection
    // For now, we'll leave this as a placeholder
}

bool SelectionManager::IsObjectSelected(ObjectID objectId) const
{
    return selectedObjects_.find(objectId) != selectedObjects_.end();
}

bool SelectionManager::IsVertexSelected(VertexID vertexId) const
{
    return selectedVertices_.find(vertexId) != selectedVertices_.end();
}

bool SelectionManager::IsEdgeSelected(EdgeID edgeId) const
{
    return selectedEdges_.find(edgeId) != selectedEdges_.end();
}

bool SelectionManager::IsFaceSelected(FaceID faceId) const
{
    return selectedFaces_.find(faceId) != selectedFaces_.end();
}

size_t SelectionManager::GetSelectionCount() const
{
    switch (currentMode_) {
        case SelectionMode::OBJECT: return selectedObjects_.size();
        case SelectionMode::VERTEX: return selectedVertices_.size();
        case SelectionMode::EDGE:   return selectedEdges_.size();
        case SelectionMode::FACE:   return selectedFaces_.size();
        default:                    return 0;
    }
}

bool SelectionManager::HasSelection() const
{
    return GetSelectionCount() > 0;
}

Vector3 SelectionManager::GetSelectionCenter() const
{
    Vector3 center = {0.0f, 0.0f, 0.0f};
    size_t count = 0;

    switch (currentMode_) {
        case SelectionMode::OBJECT:
            if (GetObjectPosition) {
                for (ObjectID id : selectedObjects_) {
                    Vector3 pos = GetObjectPosition(id);
                    center.x += pos.x;
                    center.y += pos.y;
                    center.z += pos.z;
                    count++;
                }
            }
            break;
        case SelectionMode::VERTEX:
            if (GetVertexPosition) {
                for (VertexID id : selectedVertices_) {
                    Vector3 pos = GetVertexPosition(id);
                    center.x += pos.x;
                    center.y += pos.y;
                    center.z += pos.z;
                    count++;
                }
            }
            break;
        case SelectionMode::EDGE:
            if (GetEdgePosition) {
                for (EdgeID id : selectedEdges_) {
                    Vector3 pos = GetEdgePosition(id);
                    center.x += pos.x;
                    center.y += pos.y;
                    center.z += pos.z;
                    count++;
                }
            }
            break;
        case SelectionMode::FACE:
            if (GetFacePosition) {
                for (FaceID id : selectedFaces_) {
                    Vector3 pos = GetFacePosition(id);
                    center.x += pos.x;
                    center.y += pos.y;
                    center.z += pos.z;
                    count++;
                }
            }
            break;
    }

    if (count > 0) {
        center.x /= count;
        center.y /= count;
        center.z /= count;
    }

    return center;
}

Vector3 SelectionManager::GetSelectionBounds() const
{
    // TODO: Implement proper bounding box calculation
    // For now, return a default size
    return {1.0f, 1.0f, 1.0f};
}

void SelectionManager::ClearAll()
{
    selectedObjects_.clear();
    selectedVertices_.clear();
    selectedEdges_.clear();
    selectedFaces_.clear();
}

void SelectionManager::ClearObjects()
{
    selectedObjects_.clear();
}

void SelectionManager::ClearVertices()
{
    selectedVertices_.clear();
}

void SelectionManager::ClearEdges()
{
    selectedEdges_.clear();
}

void SelectionManager::ClearFaces()
{
    selectedFaces_.clear();
}

void SelectionManager::ClearCurrentMode()
{
    switch (currentMode_) {
        case SelectionMode::OBJECT: ClearObjects(); break;
        case SelectionMode::VERTEX: ClearVertices(); break;
        case SelectionMode::EDGE:   ClearEdges(); break;
        case SelectionMode::FACE:   ClearFaces(); break;
    }
}

void SelectionManager::HandleInput()
{
    // Handle Tab key for mode cycling
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        CycleSelectionMode();
    }

    // Handle Escape key to clear selection
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ClearCurrentMode();
    }

    // Handle Ctrl+A for select all (if callbacks are set)
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_A)) {
        // TODO: Implement select all for current mode
    }
}

SelectionManager::SelectionType SelectionManager::GetSelectionTypeFromInput() const
{
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
        return SelectionType::ADDITIVE;
    } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
        return SelectionType::SUBTRACTIVE;
    } else {
        return SelectionType::SINGLE;
    }
}

void SelectionManager::SelectAt(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                               float zoomLevel, ImVec2 panOffset, SelectionType type)
{
    switch (currentMode_) {
        case SelectionMode::OBJECT:
            if (GetObjectsAtPosition) {
                auto objects = GetObjectsAtPosition(screenPos, canvasPos, canvasSize, zoomLevel, panOffset);
                if (!objects.empty()) {
                    SelectObject(objects[0], type); // Select first object found
                }
            }
            break;
        case SelectionMode::VERTEX:
            if (GetVerticesAtPosition) {
                auto vertices = GetVerticesAtPosition(screenPos, canvasPos, canvasSize, zoomLevel, panOffset);
                if (!vertices.empty()) {
                    SelectVertex(vertices[0], type);
                }
            }
            break;
        case SelectionMode::EDGE:
            if (GetEdgesAtPosition) {
                auto edges = GetEdgesAtPosition(screenPos, canvasPos, canvasSize, zoomLevel, panOffset);
                if (!edges.empty()) {
                    SelectEdge(edges[0], type);
                }
            }
            break;
        case SelectionMode::FACE:
            if (GetFacesAtPosition) {
                auto faces = GetFacesAtPosition(screenPos, canvasPos, canvasSize, zoomLevel, panOffset);
                if (!faces.empty()) {
                    SelectFace(faces[0], type);
                }
            }
            break;
    }
}

void SelectionManager::RenderSelectionHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                                 float zoomLevel, ImVec2 panOffset)
{
    switch (currentMode_) {
        case SelectionMode::OBJECT:
            RenderObjectHighlights(drawList, canvasPos, canvasSize, zoomLevel, panOffset);
            break;
        case SelectionMode::VERTEX:
            RenderVertexHighlights(drawList, canvasPos, canvasSize, zoomLevel, panOffset);
            break;
        case SelectionMode::EDGE:
            RenderEdgeHighlights(drawList, canvasPos, canvasSize, zoomLevel, panOffset);
            break;
        case SelectionMode::FACE:
            RenderFaceHighlights(drawList, canvasPos, canvasSize, zoomLevel, panOffset);
            break;
    }
}

void SelectionManager::RenderBoxSelection(ImDrawList* drawList)
{
    if (!isBoxSelecting_) return;

    ImVec2 min = ImVec2(
        std::min(boxSelectionStart_.x, boxSelectionEnd_.x),
        std::min(boxSelectionStart_.y, boxSelectionEnd_.y)
    );
    ImVec2 max = ImVec2(
        std::max(boxSelectionStart_.x, boxSelectionEnd_.x),
        std::max(boxSelectionStart_.y, boxSelectionEnd_.y)
    );

    // Draw selection rectangle
    drawList->AddRect(min, max, boxSelectionColor_, 0.0f, 0, 1.0f);
    drawList->AddRectFilled(min, max, IM_COL32(100, 149, 237, 32)); // Very transparent fill
}

// Template specialization for multi-selection
template<typename T>
void SelectionManager::ApplyMultiSelection(std::set<T>& targetSet, const std::vector<T>& ids, SelectionType type)
{
    if (type == SelectionType::SINGLE) {
        targetSet.clear();
    }

    for (T id : ids) {
        ApplySelection(targetSet, id, type == SelectionType::SINGLE ? SelectionType::ADDITIVE : type);
    }
}

template<typename T>
void SelectionManager::ApplySelection(std::set<T>& targetSet, T id, SelectionType type)
{
    switch (type) {
        case SelectionType::SINGLE:
            targetSet.clear();
            targetSet.insert(id);
            break;
        case SelectionType::ADDITIVE:
            targetSet.insert(id);
            break;
        case SelectionType::SUBTRACTIVE:
            targetSet.erase(id);
            break;
    }
}

void SelectionManager::RenderObjectHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                             float zoomLevel, ImVec2 panOffset)
{
    // TODO: Implement object highlighting
    // This would require callbacks to get object visual data
}

void SelectionManager::RenderVertexHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                             float zoomLevel, ImVec2 panOffset)
{
    // TODO: Implement vertex highlighting
    // This would require callbacks to get vertex positions and render them as circles
}

void SelectionManager::RenderEdgeHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                           float zoomLevel, ImVec2 panOffset)
{
    // TODO: Implement edge highlighting
    // This would require callbacks to get edge endpoints and render them as lines
}

void SelectionManager::RenderFaceHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                           float zoomLevel, ImVec2 panOffset)
{
    // TODO: Implement face highlighting
    // This would require callbacks to get face vertices and render them as filled polygons
}