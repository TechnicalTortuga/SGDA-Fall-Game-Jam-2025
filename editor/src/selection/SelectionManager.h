#pragma once

#include <raylib.h>
#include <imgui.h>
#include <set>
#include <vector>
#include <functional>

// Forward declarations
struct EditableMesh;

class SelectionManager {
public:
    enum class SelectionMode {
        OBJECT,    // Select entire objects/brushes
        VERTEX,    // Select individual vertices
        EDGE,      // Select edges between vertices
        FACE       // Select individual faces
    };

    enum class SelectionType {
        SINGLE,      // Replace current selection
        ADDITIVE,    // Add to selection (Shift+Click)
        SUBTRACTIVE  // Remove from selection (Ctrl+Click)
    };

    // ID types for different selection targets
    using ObjectID = uint32_t;
    using VertexID = uint32_t;
    using EdgeID = uint32_t;
    using FaceID = uint32_t;

    SelectionManager();
    ~SelectionManager() = default;

    // Mode management
    void SetSelectionMode(SelectionMode mode);
    SelectionMode GetSelectionMode() const { return currentMode_; }
    void CycleSelectionMode(); // Tab key functionality
    const char* GetSelectionModeString() const;

    // Selection operations
    void SelectObject(ObjectID objectId, SelectionType type = SelectionType::SINGLE);
    void SelectVertex(VertexID vertexId, SelectionType type = SelectionType::SINGLE);
    void SelectEdge(EdgeID edgeId, SelectionType type = SelectionType::SINGLE);
    void SelectFace(FaceID faceId, SelectionType type = SelectionType::SINGLE);

    // Multi-selection operations
    void SelectObjects(const std::vector<ObjectID>& objectIds, SelectionType type = SelectionType::SINGLE);
    void SelectVertices(const std::vector<VertexID>& vertexIds, SelectionType type = SelectionType::SINGLE);
    void SelectEdges(const std::vector<EdgeID>& edgeIds, SelectionType type = SelectionType::SINGLE);
    void SelectFaces(const std::vector<FaceID>& faceIds, SelectionType type = SelectionType::SINGLE);

    // Box selection
    void BeginBoxSelection(ImVec2 startPos);
    void UpdateBoxSelection(ImVec2 currentPos);
    void EndBoxSelection(SelectionType type = SelectionType::SINGLE);
    bool IsBoxSelecting() const { return isBoxSelecting_; }
    ImVec2 GetBoxSelectionStart() const { return boxSelectionStart_; }
    ImVec2 GetBoxSelectionEnd() const { return boxSelectionEnd_; }

    // Selection queries
    bool IsObjectSelected(ObjectID objectId) const;
    bool IsVertexSelected(VertexID vertexId) const;
    bool IsEdgeSelected(EdgeID edgeId) const;
    bool IsFaceSelected(FaceID faceId) const;

    // Get selections
    const std::set<ObjectID>& GetSelectedObjects() const { return selectedObjects_; }
    const std::set<VertexID>& GetSelectedVertices() const { return selectedVertices_; }
    const std::set<EdgeID>& GetSelectedEdges() const { return selectedEdges_; }
    const std::set<FaceID>& GetSelectedFaces() const { return selectedFaces_; }

    // Selection info
    size_t GetSelectionCount() const;
    bool HasSelection() const;
    Vector3 GetSelectionCenter() const;
    Vector3 GetSelectionBounds() const;

    // Clear operations
    void ClearAll();
    void ClearObjects();
    void ClearVertices();
    void ClearEdges();
    void ClearFaces();
    void ClearCurrentMode();

    // Input handling
    void HandleInput();
    SelectionType GetSelectionTypeFromInput() const;

    // Picking operations (screen to world selection)
    void SelectAt(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, 
                  float zoomLevel, ImVec2 panOffset, SelectionType type = SelectionType::SINGLE);
    
    // Callbacks for getting selectable elements (set by MainWindow)
    std::function<std::vector<ObjectID>(ImVec2, ImVec2, ImVec2, float, ImVec2)> GetObjectsAtPosition;
    std::function<std::vector<VertexID>(ImVec2, ImVec2, ImVec2, float, ImVec2)> GetVerticesAtPosition;
    std::function<std::vector<EdgeID>(ImVec2, ImVec2, ImVec2, float, ImVec2)> GetEdgesAtPosition;
    std::function<std::vector<FaceID>(ImVec2, ImVec2, ImVec2, float, ImVec2)> GetFacesAtPosition;

    // Callbacks for getting element positions (for center calculation)
    std::function<Vector3(ObjectID)> GetObjectPosition;
    std::function<Vector3(VertexID)> GetVertexPosition;
    std::function<Vector3(EdgeID)> GetEdgePosition;
    std::function<Vector3(FaceID)> GetFacePosition;

    // Visual feedback
    void RenderSelectionHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                                   float zoomLevel, ImVec2 panOffset);
    void RenderBoxSelection(ImDrawList* drawList);

private:
    SelectionMode currentMode_;

    // Selection storage
    std::set<ObjectID> selectedObjects_;
    std::set<VertexID> selectedVertices_;
    std::set<EdgeID> selectedEdges_;
    std::set<FaceID> selectedFaces_;

    // Box selection state
    bool isBoxSelecting_;
    ImVec2 boxSelectionStart_;
    ImVec2 boxSelectionEnd_;

    // Visual settings
    ImU32 objectHighlightColor_;
    ImU32 vertexHighlightColor_;
    ImU32 edgeHighlightColor_;
    ImU32 faceHighlightColor_;
    ImU32 boxSelectionColor_;
    float highlightThickness_;

    // Helper methods
    template<typename T>
    void ApplySelection(std::set<T>& targetSet, T id, SelectionType type);

    template<typename T>
    void ApplyMultiSelection(std::set<T>& targetSet, const std::vector<T>& ids, SelectionType type);

    void RenderObjectHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                               float zoomLevel, ImVec2 panOffset);
    void RenderVertexHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                               float zoomLevel, ImVec2 panOffset);
    void RenderEdgeHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                             float zoomLevel, ImVec2 panOffset);
    void RenderFaceHighlights(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize, 
                             float zoomLevel, ImVec2 panOffset);
};