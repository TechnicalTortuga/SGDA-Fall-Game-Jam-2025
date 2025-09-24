#pragma once

#include <imgui.h>
#include "raylib.h"
#include <vector>
#include <memory>
#include "../viewport/GridManager.h"
#include "../viewport/CameraManager.h"
#include "../selection/SelectionManager.h"
#include "../selection/GizmoManager.h"
#include "CommandManager.h"
#include "../scene/BrushManager.h"

class Application;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Initialize(Application* app);
    void Shutdown();
    void Render();

private:
    Application* app_;
    GridManager gridManager_;  // Robust grid system
    CameraManager cameraManager_;  // Professional camera system
    SelectionManager selectionManager_;  // Multi-mode selection system
    GizmoManager gizmoManager_;  // 3D manipulation gizmos

    void RenderMenuBar();
    void RenderToolbar();
    void RenderViewports();
    void HandleViewportInteraction(int viewportIndex);
    void HandlePerspectiveCameraControls(int viewportIndex, bool isHovered);
    void RenderSceneViewport(int viewportIndex, bool showGrid, float zoomLevel, ImVec2 panOffset);
    void RenderScene3D(ImVec2 canvasPos, ImVec2 canvasSize);
    void InitializeViewportTextures();
    void RenderPerspectiveView();
    void RenderTopView(); 
    void RenderFrontView();
    void RenderSideView();
    ImVec2 WorldToScreen(ImVec2 worldPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset);
    void HandleGridInput(); // Handle bracket keys for grid scaling
    void HandleSelectionInput(); // Handle Tab key and selection operations
    void HandleGizmoInput(); // Handle G/R/S keys and gizmo operations
    void SetupSelectionCallbacks(); // Setup callbacks for SelectionManager
    void RenderSkybox();
    void RenderAxisLines();
    void UpdatePerspectiveCamera(float deltaTime);
    void RenderInspector();
    void RenderAssetBrowser();
    void RenderStatusBar();
    void RenderContextMenu();
    void CreateObjectAtContextMenu(PrimitiveType type);

    // Selection/picking helper methods
    std::vector<SelectionManager::ObjectID> GetObjectsAtPosition(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset);
    Vector3 GetObjectPosition(SelectionManager::ObjectID objectId);
    Vector3 GetBrushPosition(uint32_t brushIndex);

    // Brush/primitive functions
    void StartBrushCreation(PrimitiveType type);
    void UpdateBrushCreation(ImVec2 currentMousePos, int viewportIndex);
    void FinishBrushCreation();
    void CancelBrushCreation();
    void RenderBrushes(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd, float zoomLevel, ImVec2 panOffset, int viewportIndex);
    ImVec2 ScreenToWorld(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset);
    void SetMouseCursor(ImGuiMouseCursor cursor);

    // Undo/Redo system
    void ExecuteCommand(std::unique_ptr<Command> command);
    void Undo();
    void Redo();
    bool CanUndo() const { return commandManager_.CanUndo(); }
    bool CanRedo() const { return commandManager_.CanRedo(); }

public:
    // Brush management (delegates to BrushManager)
    size_t AddBrush(const Brush& brush);
    void RemoveBrush(size_t index);
    const Brush& GetBrush(size_t index) const;
    size_t GetBrushCount() const;

private:

    // Managers
    CommandManager commandManager_;
    BrushManager brushManager_;

    // Cached camera reference for rendering (avoid repeated GetRaylibCamera() calls)
    Camera3D* cachedCamera_;

    // Viewport RenderTextures for hybrid approach
    RenderTexture2D perspectiveTexture_;  // 3D perspective view
    RenderTexture2D topTexture_;          // Top view (X/Y plane)
    RenderTexture2D frontTexture_;        // Front view (X/Z plane)
    RenderTexture2D sideTexture_;         // Side view (Y/Z plane)
    bool viewportTexturesInitialized_;


    // Brush creation state
    bool isCreatingBrush_;      // Currently creating a brush
    PrimitiveType creatingType_; // Type of brush being created
    ImVec2 creationStartPos_;   // Screen position where creation started
    int creationViewport_;      // Which viewport creation started in

    // Context menu state
    bool showContextMenu_;      // Whether to show context menu
    ImVec2 contextMenuPos_;     // Position to show context menu
    int contextMenuViewport_;   // Which viewport context menu is for

    // Mouse interaction state
    bool isDragging_[4];  // Track if each viewport is being dragged
    ImVec2 lastMousePos_[4];  // Last mouse position for each viewport during drag

    // Viewport state
    float zoomLevels_[4];  // Zoom level for each viewport (1.0 = default)
    ImVec2 panOffsets_[4]; // Pan offset for each viewport in world units

    // UI state
    bool showGrids_;       // Whether to show grids
    bool showSkybox_;      // Whether to show skybox in perspective view
    int activeViewportIndex_; // Which viewport is currently active/focused (-1 = none)
};
