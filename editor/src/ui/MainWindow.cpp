#include "MainWindow.h"
#include "../core/Application.h"
#include "../core/Logger.h"
#include <imgui.h>
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>


MainWindow::MainWindow()
    : app_(nullptr), commandManager_(), cachedCamera_(nullptr),
      gridManager_(), cameraManager_(), selectionManager_(), gizmoManager_(),
      viewportTexturesInitialized_(false),
      showGrids_(true), showSkybox_(false), activeViewportIndex_(-1),
      isCreatingBrush_(false), creatingType_(PrimitiveType::Cube), creationStartPos_(0, 0), creationViewport_(-1),
      showContextMenu_(false), contextMenuPos_(0, 0), contextMenuViewport_(-1)
{
    // Initialize zoom levels - reasonable starting point
    // Lower zoom = more zoomed out, higher zoom = more zoomed in
    zoomLevels_[0] = 0.5f; // Top view - reasonable zoom level
    zoomLevels_[1] = 0.5f; // Front view - reasonable zoom level
    zoomLevels_[2] = 0.5f; // Perspective view - reasonable zoom level
    zoomLevels_[3] = 0.5f; // Side view - reasonable zoom level

    // Initialize pan offsets (centered at origin)
    panOffsets_[0] = ImVec2(0, 0); // Top view
    panOffsets_[1] = ImVec2(0, 0); // Front view
    panOffsets_[2] = ImVec2(0, 0); // Perspective view
    panOffsets_[3] = ImVec2(0, 0); // Side view

    // Initialize mouse interaction state
    isDragging_[0] = false; // Top view
    isDragging_[1] = false; // Front view
    isDragging_[2] = false; // Perspective view
    isDragging_[3] = false; // Side view

    lastMousePos_[0] = ImVec2(0, 0); // Top view
    lastMousePos_[1] = ImVec2(0, 0); // Front view
    lastMousePos_[2] = ImVec2(0, 0); // Perspective view
    lastMousePos_[3] = ImVec2(0, 0); // Side view
}

MainWindow::~MainWindow()
{
    if (cachedCamera_) {
        delete cachedCamera_;
        cachedCamera_ = nullptr;
    }
    
    // Clean up RenderTextures
    if (viewportTexturesInitialized_) {
        UnloadRenderTexture(perspectiveTexture_);
        UnloadRenderTexture(topTexture_);
        UnloadRenderTexture(frontTexture_);
        UnloadRenderTexture(sideTexture_);
        viewportTexturesInitialized_ = false;
    }
    
    Shutdown();
}

bool MainWindow::Initialize(Application* app)
{
    app_ = app;
    
    // Setup selection callbacks
    SetupSelectionCallbacks();
    
    // Initialize gizmo settings
    gizmoManager_.SetSnapToGrid(true);
    gizmoManager_.SetGridSize(static_cast<float>(gridManager_.GetCurrentGridSize()));
    
    // Initialize viewport RenderTextures
    InitializeViewportTextures();
    
    return true;
}

void MainWindow::InitializeViewportTextures()
{
    if (viewportTexturesInitialized_) {
        return; // Already initialized
    }
    
    // Create RenderTextures for each viewport (start with reasonable sizes)
    int textureWidth = 512;
    int textureHeight = 512;
    
    perspectiveTexture_ = LoadRenderTexture(textureWidth, textureHeight);
    topTexture_ = LoadRenderTexture(textureWidth, textureHeight);
    frontTexture_ = LoadRenderTexture(textureWidth, textureHeight);
    sideTexture_ = LoadRenderTexture(textureWidth, textureHeight);
    
    viewportTexturesInitialized_ = true;
}

void MainWindow::Shutdown()
{
    
}

void MainWindow::Render()
{
    // Handle global input for brush creation
    if (isCreatingBrush_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        CancelBrushCreation();
    }

    // Handle undo/redo keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Z) && (ImGui::GetIO().KeyMods & ImGuiKey_ModCtrl)) {
        if (ImGui::GetIO().KeyMods & ImGuiKey_ModShift) {
            // Ctrl+Shift+Z is redo on some systems
            Redo();
        } else {
            // Ctrl+Z is undo
            Undo();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y) && (ImGui::GetIO().KeyMods & ImGuiKey_ModCtrl)) {
        // Ctrl+Y is redo
        Redo();
    }

    // Handle grid input
    HandleGridInput();
    
    // Handle selection input
    HandleSelectionInput();
    
    // Handle gizmo input
    HandleGizmoInput();

    // Render context menu if needed
    RenderContextMenu();

    // Create main editor window that contains all panels in fixed layout
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("Editor", nullptr, window_flags)) {
        ImGui::PopStyleVar(2);

        // Render menu bar inside the main window
        RenderMenuBar();

        // Get available space for layout
        ImVec2 availableSpace = ImGui::GetContentRegionAvail();
        float toolbarWidth = 256.0f;
        float inspectorWidth = 320.0f;
        float assetBrowserHeight = 200.0f;

        // Calculate panel positions and sizes
        float viewportX = toolbarWidth;
        float viewportWidth = availableSpace.x - toolbarWidth - inspectorWidth;
        float viewportHeight = availableSpace.y - assetBrowserHeight;

        // Toolbar (left side)
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::BeginChild("ToolbarPanel", ImVec2(toolbarWidth, viewportHeight), true);
        RenderToolbar();
        ImGui::EndChild();

        // Viewport (center)
        ImGui::SetCursorPos(ImVec2(viewportX, 0));
        ImGui::BeginChild("ViewportPanel", ImVec2(viewportWidth, viewportHeight), true);
        RenderViewports();
        ImGui::EndChild();

        // Inspector (right side)
        ImGui::SetCursorPos(ImVec2(viewportX + viewportWidth, 0));
        ImGui::BeginChild("InspectorPanel", ImVec2(inspectorWidth, viewportHeight), true);
        RenderInspector();
        ImGui::EndChild();

        // Asset Browser (bottom, spans full width)
        ImGui::SetCursorPos(ImVec2(0, viewportHeight));
        ImGui::BeginChild("AssetBrowserPanel", ImVec2(availableSpace.x, assetBrowserHeight), true);
        RenderAssetBrowser();
        ImGui::EndChild();
    } else {
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

void MainWindow::RenderMenuBar()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                // TODO: New project
            }
            if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                // TODO: Open project
            }
            if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                // TODO: Save project
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // TODO: Exit application
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
                Redo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Toggle Grid", nullptr, showGrids_)) {
                showGrids_ = !showGrids_;
            }
            if (ImGui::MenuItem("Grid Snapping", nullptr, gridManager_.IsSnappingEnabled())) {
                gridManager_.SetSnappingEnabled(!gridManager_.IsSnappingEnabled());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Mouse Look Mode", "Z", cameraManager_.IsMouseLookMode())) {
                cameraManager_.ToggleMouseLookMode();
            }
            if (ImGui::MenuItem("Reset Camera", "Middle Mouse 2x")) {
                cameraManager_.ResetToDefault();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Skybox (3D View)", nullptr, showSkybox_)) {
                showSkybox_ = !showSkybox_;
            }
            if (ImGui::MenuItem("Toggle Wireframe")) {
                // TODO: Toggle wireframe
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Compile BSP", "F9")) {
                // TODO: Compile BSP
            }
            if (ImGui::MenuItem("Play Level", "F5")) {
                // TODO: Play level
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void MainWindow::RenderToolbar()
{
    // Tools Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("TOOLS");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Select", ImVec2(180, 30))) {
        // TODO: Select tool
    }
    if (ImGui::Button("Move", ImVec2(180, 30))) {
        // TODO: Move tool
    }
    if (ImGui::Button("Rotate", ImVec2(180, 30))) {
        // TODO: Rotate tool
    }
    if (ImGui::Button("Scale", ImVec2(180, 30))) {
        // TODO: Scale tool
    }

    ImGui::Separator();

    // Brushes Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("BRUSHES");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Cube", ImVec2(180, 30))) {
        StartBrushCreation(PrimitiveType::Cube);
        SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (ImGui::Button("Cylinder", ImVec2(180, 30))) {
        StartBrushCreation(PrimitiveType::Cylinder);
        SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (ImGui::Button("Sphere", ImVec2(180, 30))) {
        StartBrushCreation(PrimitiveType::Sphere);
        SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (ImGui::Button("Pyramid", ImVec2(180, 30))) {
        StartBrushCreation(PrimitiveType::Pyramid);
        SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (ImGui::Button("Prism", ImVec2(180, 30))) {
        StartBrushCreation(PrimitiveType::Prism);
        SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImGui::Separator();

    // Game Objects Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("GAME OBJECTS");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Model", ImVec2(180, 30))) {
        // TODO: Create model object
    }
    if (ImGui::Button("Sprite", ImVec2(180, 30))) {
        // TODO: Create sprite object
    }
    if (ImGui::Button("Composite", ImVec2(180, 30))) {
        // TODO: Create composite object
    }
    if (ImGui::Button("Player Spawn", ImVec2(180, 30))) {
        // TODO: Create player spawn
    }

    ImGui::Separator();

    // Lights Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("LIGHTS");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Point Light", ImVec2(180, 30))) {
        // TODO: Create point light
    }
    if (ImGui::Button("Spot Light", ImVec2(180, 30))) {
        // TODO: Create spot light
    }
    if (ImGui::Button("Directional", ImVec2(180, 30))) {
        // TODO: Create directional light
    }

    ImGui::Separator();

    // Audio Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("AUDIO");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Sound Emitter", ImVec2(180, 30))) {
        // TODO: Create sound emitter
    }
    if (ImGui::Button("Ambient Zone", ImVec2(180, 30))) {
        // TODO: Create ambient audio zone
    }

    ImGui::Separator();

    // Triggers Section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("TRIGGERS");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::Button("Trigger Box", ImVec2(180, 30))) {
        // TODO: Create trigger box
    }
    if (ImGui::Button("Trigger Sphere", ImVec2(180, 30))) {
        // TODO: Create trigger sphere
    }
}

void MainWindow::RenderViewports()
{
    if (!viewportTexturesInitialized_) {
        ImGui::Text("Initializing viewports...");
        return;
    }
    
    // First, render all scenes to their respective RenderTextures
    RenderPerspectiveView();
    RenderTopView();
    RenderFrontView();
    RenderSideView();
    
    // Get available space for displaying the viewports
    ImVec2 availableSpace = ImGui::GetContentRegionAvail();
    
    // Calculate sizes for 2x2 grid layout
    float viewportWidth = (availableSpace.x - 10.0f) * 0.5f;  // Account for spacing
    float viewportHeight = (availableSpace.y - 30.0f) * 0.5f; // Account for spacing and text
    
    // Create 2x2 grid layout using ImGui
    if (ImGui::BeginTable("ViewportGrid", 2, ImGuiTableFlags_Borders)) {
        // Set up equal columns
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, viewportWidth);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed, viewportWidth);
        
        // Top row
        ImGui::TableNextRow();
        
        // Top-Left: Perspective View (3D)
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("PerspectiveViewport", ImVec2(viewportWidth, viewportHeight), true);
        {
            ImVec2 imageSize = ImVec2(viewportWidth - 10, viewportHeight - 25); // Leave space for text
            ImGui::Image((ImTextureID)(intptr_t)perspectiveTexture_.texture.id, imageSize);

            Vector3 camPos = cameraManager_.GetPosition();
            const char* camMode = cameraManager_.IsMouseLookMode() ? "MOUSELOOK (Z)" : "NAVIGATION";
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
            ImGui::Text("PERSPECTIVE (3D) | %s | Pos: (%.1f, %.1f, %.1f)", camMode, camPos.x, camPos.y, camPos.z);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        
        // Top-Right: Front View (X/Z)
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("FrontViewport", ImVec2(viewportWidth, viewportHeight), true);
        {
            ImVec2 imageSize = ImVec2(viewportWidth - 10, viewportHeight - 25);
            ImGui::Image((ImTextureID)(intptr_t)frontTexture_.texture.id, imageSize);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
            ImGui::Text("FRONT VIEW (X/Z) | Grid: %s (%d) | Zoom: %.0f%%",
                       showGrids_ ? "On" : "Off", gridManager_.GetCurrentGridSize(), zoomLevels_[1] * 100.0f);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        
        // Bottom row
        ImGui::TableNextRow();
        
        // Bottom-Left: Top View (X/Y)
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("TopViewport", ImVec2(viewportWidth, viewportHeight), true);
        {
            ImVec2 imageSize = ImVec2(viewportWidth - 10, viewportHeight - 25);
            ImGui::Image((ImTextureID)(intptr_t)topTexture_.texture.id, imageSize);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
            ImGui::Text("TOP VIEW (X/Y) | Grid: %s (%d) | Zoom: %.0f%%",
                       showGrids_ ? "On" : "Off", gridManager_.GetCurrentGridSize(), zoomLevels_[0] * 100.0f);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        
        // Bottom-Right: Side View (Y/Z)
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("SideViewport", ImVec2(viewportWidth, viewportHeight), true);
        {
            ImVec2 imageSize = ImVec2(viewportWidth - 10, viewportHeight - 25);
            ImGui::Image((ImTextureID)(intptr_t)sideTexture_.texture.id, imageSize);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
            ImGui::Text("SIDE VIEW (Y/Z) | Grid: %s (%d) | Zoom: %.0f%%",
                       showGrids_ ? "On" : "Off", gridManager_.GetCurrentGridSize(), zoomLevels_[3] * 100.0f);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        
        ImGui::EndTable();
    }
}

void MainWindow::HandleViewportInteraction(int viewportIndex)
{
    // Only process interaction if mouse is over this viewport
    if (!ImGui::IsWindowHovered()) {
        // Reset dragging state when mouse leaves window
        isDragging_[viewportIndex] = false;
        return;
    }

    // Set this viewport as active when clicked
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        activeViewportIndex_ = viewportIndex;
    }

    // Handle mouse wheel for zooming
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        zoomLevels_[viewportIndex] *= (wheel > 0.0f) ? 1.1f : 0.9f;
        zoomLevels_[viewportIndex] = zoomLevels_[viewportIndex] < 0.01f ? 0.01f : (zoomLevels_[viewportIndex] > 100.0f ? 100.0f : zoomLevels_[viewportIndex]);
    }

    // Special handling for perspective view (index 2)
    if (viewportIndex == 2) {
        HandlePerspectiveCameraControls(viewportIndex, ImGui::IsWindowHovered());
        return;
    }

    // Handle middle mouse button for panning (for ortho views)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        if (!isDragging_[viewportIndex]) {
            // Start dragging
            isDragging_[viewportIndex] = true;
            lastMousePos_[viewportIndex] = ImGui::GetMousePos();
        } else {
            // Continue dragging
            ImVec2 currentMousePos = ImGui::GetMousePos();
            ImVec2 delta = ImVec2(currentMousePos.x - lastMousePos_[viewportIndex].x,
                                  currentMousePos.y - lastMousePos_[viewportIndex].y);

            // Apply pan (invert both X and Y for natural feel)
            panOffsets_[viewportIndex].x -= delta.x / zoomLevels_[viewportIndex];
            panOffsets_[viewportIndex].y -= delta.y / zoomLevels_[viewportIndex];

            // Clamp pan offsets to reasonable bounds to prevent infinite panning
            const float maxPan = 5000.0f; // Allow panning up to 5000 units in any direction
            panOffsets_[viewportIndex].x = std::max(-maxPan, std::min(maxPan, panOffsets_[viewportIndex].x));
            panOffsets_[viewportIndex].y = std::max(-maxPan, std::min(maxPan, panOffsets_[viewportIndex].y));

            lastMousePos_[viewportIndex] = currentMousePos;
        }
    } else {
        isDragging_[viewportIndex] = false;
    }

    // Handle gizmo interaction first (highest priority)
    if (selectionManager_.HasSelection() && gizmoManager_.GetGizmoMode() != GizmoManager::GizmoMode::NONE) {
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y -= 20.0f; // Reserve space for text
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        
        Camera3D camera = cameraManager_.GetRaylibCamera(); // For perspective view
        bool gizmoHandled = gizmoManager_.HandleMouseInput(ImGui::GetMousePos(), canvasPos, canvasSize, zoomLevels_[viewportIndex], panOffsets_[viewportIndex], camera);
        
        if (gizmoHandled) {
            // Update gizmo position if there's an active manipulation
            if (gizmoManager_.IsGizmoActive() && selectionManager_.HasSelection()) {
                Vector3 selectionCenter = selectionManager_.GetSelectionCenter();
                Vector3 delta = gizmoManager_.GetCurrentDelta();
                gizmoManager_.SetGizmoPosition(Vector3{selectionCenter.x + delta.x, selectionCenter.y + delta.y, selectionCenter.z + delta.z});
                
                // TODO: Apply transformation to selected objects
            }
            return; // Gizmo consumed the input
        }
    }

    // Handle brush creation
    if (isCreatingBrush_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Start creating brush at click position
        creationStartPos_ = ImGui::GetMousePos();
        creationViewport_ = viewportIndex;
    } else if (isCreatingBrush_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) && creationViewport_ == viewportIndex) {
        // Update brush creation while dragging
        UpdateBrushCreation(ImGui::GetMousePos(), viewportIndex);
    } else if (isCreatingBrush_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        // Finish brush creation
        FinishBrushCreation();
    } else if (!isCreatingBrush_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Handle selection
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y -= 20.0f; // Reserve space for text
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        
        SelectionManager::SelectionType selectionType = selectionManager_.GetSelectionTypeFromInput();
        selectionManager_.SelectAt(ImGui::GetMousePos(), canvasPos, canvasSize, zoomLevels_[viewportIndex], panOffsets_[viewportIndex], selectionType);
        
        // Update gizmo position to selection center
        if (selectionManager_.HasSelection()) {
            Vector3 selectionCenter = selectionManager_.GetSelectionCenter();
            gizmoManager_.SetGizmoPosition(selectionCenter);
        }
    }

    // Handle right-click to show context menu
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImVec2 clickPos = ImGui::GetMousePos();
        // Check if click is within viewport bounds
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y -= 20.0f; // Reserve space for text
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasEnd = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

        if (clickPos.x >= canvasPos.x && clickPos.x <= canvasEnd.x &&
            clickPos.y >= canvasPos.y && clickPos.y <= canvasEnd.y) {
            // Store click position and viewport for creation
            contextMenuPos_ = clickPos;
            contextMenuViewport_ = viewportIndex;
            ImGui::OpenPopup("CreateMenu");
        }
    }

    // Handle double-click to reset view (middle mouse button)
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle)) {
        zoomLevels_[viewportIndex] = 1.0f;
        panOffsets_[viewportIndex] = ImVec2(0, 0);
    }
}

void MainWindow::HandlePerspectiveCameraControls(int viewportIndex, bool isHovered)
{
    // Handle mouse wheel zoom for perspective view
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f && isHovered) {
        // For perspective camera, zoom by moving closer/farther
        float zoomFactor = (wheel > 0.0f) ? 0.9f : 1.1f;
        cameraManager_.Zoom(zoomFactor);
    }

    // Handle middle mouse button for 3D orbiting
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && isHovered) {
        if (!isDragging_[viewportIndex]) {
            // Start orbiting
            isDragging_[viewportIndex] = true;
            lastMousePos_[viewportIndex] = ImGui::GetMousePos();
        } else {
            // Continue orbiting
            ImVec2 currentMousePos = ImGui::GetMousePos();
            ImVec2 delta = ImVec2(currentMousePos.x - lastMousePos_[viewportIndex].x,
                                  currentMousePos.y - lastMousePos_[viewportIndex].y);

            // Apply orbital rotation (middle mouse orbits around target)
            cameraManager_.Orbit(delta.x * 0.01f, delta.y * 0.01f);

            lastMousePos_[viewportIndex] = currentMousePos;
        }
    } else {
        isDragging_[viewportIndex] = false;
    }

    // Update camera manager with delta time and hover state
    // WASD and navigation input work when perspective viewport is active
    float deltaTime = GetFrameTime();
    bool isPerspectiveActive = (activeViewportIndex_ == 2);

    // Enable input when perspective viewport is active and hovered
    cameraManager_.Update(deltaTime, isPerspectiveActive && isHovered);

    // If perspective viewport becomes active, switch to navigation mode
    if (isPerspectiveActive && cameraManager_.IsMouseLookMode()) {
        cameraManager_.SetMouseLookMode(false);
    }
}

void MainWindow::RenderSkybox()
{
    if (!showSkybox_) return;

    // For now, just draw a simple gradient skybox effect
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 20.0f; // Reserve space for the text below
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasEnd = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    // Draw skybox gradient (blue to light blue)
    drawList->AddRectFilledMultiColor(canvasPos, canvasEnd,
        IM_COL32(135, 206, 235, 255),  // Sky blue top-left
        IM_COL32(173, 216, 230, 255),  // Light blue top-right
        IM_COL32(100, 149, 237, 255),  // Cornflower blue bottom-left
        IM_COL32(70, 130, 180, 255));  // Steel blue bottom-right
}

void MainWindow::RenderAxisLines()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 20.0f; // Reserve space for the text below
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasEnd = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    // Get the camera for perspective projection
    if (!cachedCamera_) {
        cachedCamera_ = new Camera3D(cameraManager_.GetRaylibCamera());
    } else {
        *cachedCamera_ = cameraManager_.GetRaylibCamera();
    }
    Camera3D camera = *cachedCamera_;

    // Define axis points in world space (origin and axis endpoints)
    Vector3 origin = {0.0f, 0.0f, 0.0f};
    Vector3 xAxisEnd = {2.0f, 0.0f, 0.0f};
    Vector3 yAxisEnd = {0.0f, 2.0f, 0.0f};
    Vector3 zAxisEnd = {0.0f, 0.0f, 2.0f};

    // Project 3D points to 2D screen space using raylib
    Vector2 originScreen = GetWorldToScreen(origin, camera);
    Vector2 xEndScreen = GetWorldToScreen(xAxisEnd, camera);
    Vector2 yEndScreen = GetWorldToScreen(yAxisEnd, camera);
    Vector2 zEndScreen = GetWorldToScreen(zAxisEnd, camera);

    // Convert raylib Vector2 to ImVec2 and adjust for canvas position
    ImVec2 originIm = ImVec2(canvasPos.x + originScreen.x, canvasPos.y + originScreen.y);
    ImVec2 xEndIm = ImVec2(canvasPos.x + xEndScreen.x, canvasPos.y + xEndScreen.y);
    ImVec2 yEndIm = ImVec2(canvasPos.x + yEndScreen.x, canvasPos.y + yEndScreen.y);
    ImVec2 zEndIm = ImVec2(canvasPos.x + zEndScreen.x, canvasPos.y + zEndScreen.y);

    // Draw axis lines (only if they're visible in the viewport)
    const float axisThickness = 3.0f;

    // Check if points are within viewport bounds
    auto isInViewport = [&](ImVec2 point) {
        return point.x >= canvasPos.x && point.x <= canvasEnd.x &&
               point.y >= canvasPos.y && point.y <= canvasEnd.y;
    };

    if (isInViewport(originIm) || isInViewport(xEndIm)) {
        drawList->AddLine(originIm, xEndIm, IM_COL32(255, 0, 0, 255), axisThickness); // X axis (Red)
    }
    if (isInViewport(originIm) || isInViewport(yEndIm)) {
        drawList->AddLine(originIm, yEndIm, IM_COL32(0, 255, 0, 255), axisThickness); // Y axis (Green)
    }
    if (isInViewport(originIm) || isInViewport(zEndIm)) {
        drawList->AddLine(originIm, zEndIm, IM_COL32(0, 0, 255, 255), axisThickness); // Z axis (Blue)
    }
}

void MainWindow::RenderSceneViewport(int viewportIndex, bool showGrid, float zoomLevel, ImVec2 panOffset)
{
    // Get the available space for the scene canvas (leave room for text at bottom)
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 20.0f; // Reserve space for the text below

    // Create a scene canvas
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasEnd = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    // Draw scene background - skybox for perspective view, solid dark for others
    if (viewportIndex == 2 && showSkybox_) {
        RenderSkybox();
    } else {
        ImU32 bgColor = IM_COL32(30, 30, 30, 255); // Dark gray background
        drawList->AddRectFilled(canvasPos, canvasEnd, bgColor);
    }

    // Draw grid if enabled (viewport-specific grid)
    if (showGrid) {
        // Map viewport index to GridManager::ViewportType
        GridManager::ViewportType viewportType;
        switch (viewportIndex) {
            case 0: viewportType = GridManager::ViewportType::TOP_XY; break;    // Bottom-left
            case 1: viewportType = GridManager::ViewportType::FRONT_XZ; break;  // Top-right
            case 2: viewportType = GridManager::ViewportType::PERSPECTIVE_3D; break; // Top-left
            case 3: viewportType = GridManager::ViewportType::SIDE_YZ; break;   // Bottom-right
            default: viewportType = GridManager::ViewportType::TOP_XY; break;
        }

        gridManager_.DrawGrid(drawList, canvasPos, canvasEnd, zoomLevel, panOffset, viewportType, true);
    }

    // Draw axis lines for perspective view
    if (viewportIndex == 2) {
        RenderAxisLines();
    }

    if (viewportIndex == 2) {
        // Perspective 3D view - use Raylib 3D rendering
        RenderScene3D(canvasPos, canvasSize);
    } else {
        // Orthographic 2D views - use ImGui drawing
        RenderBrushes(drawList, canvasPos, canvasEnd, zoomLevel, panOffset, viewportIndex);

        // Draw selection highlights
        selectionManager_.RenderSelectionHighlights(drawList, canvasPos, canvasSize, zoomLevel, panOffset);
    }

    // Draw scene origin (0,0) in world space
    ImVec2 worldOrigin = WorldToScreen(ImVec2(0, 0), canvasPos, canvasSize, zoomLevel, panOffset);
    if (worldOrigin.x >= canvasPos.x && worldOrigin.x <= canvasEnd.x &&
        worldOrigin.y >= canvasPos.y && worldOrigin.y <= canvasEnd.y) {
        float originRadius = 3.0f / zoomLevel;
        originRadius = originRadius < 1.0f ? 1.0f : (originRadius > 5.0f ? 5.0f : originRadius); // Clamp to keep origin visible
        drawList->AddCircle(worldOrigin, originRadius, IM_COL32(255, 255, 255, 255), 8);
    }

    // Reserve space for the canvas
    ImGui::Dummy(canvasSize);
}

ImVec2 MainWindow::WorldToScreen(ImVec2 worldPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset)
{
    // Convert world coordinates to screen coordinates
    // Center of canvas is (0,0) in world space when panOffset is (0,0)
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

    // Apply pan and zoom transformations
    float screenX = center.x + (worldPos.x - panOffset.x) * zoomLevel;
    float screenY = center.y + (worldPos.y - panOffset.y) * zoomLevel;

    return ImVec2(screenX, screenY);
}







void MainWindow::RenderInspector()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("INSPECTOR");
    ImGui::PopStyleColor(); // Restore white text

    if (ImGui::CollapsingHeader("Transform")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Position");
        ImGui::PopStyleColor();
        float pos[3] = {0, 0, 0};
        ImGui::DragFloat3("##pos", pos, 0.1f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Rotation");
        ImGui::PopStyleColor();
        float rot[3] = {0, 0, 0};
        ImGui::DragFloat3("##rot", rot, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Scale");
        ImGui::PopStyleColor();
        float scale[3] = {1, 1, 1};
        ImGui::DragFloat3("##scale", scale, 0.1f);
    }

    if (ImGui::CollapsingHeader("Material")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Diffuse Color");
        ImGui::PopStyleColor();
        float color[4] = {1, 1, 1, 1};
        ImGui::ColorEdit4("##diffuse", color);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Texture");
        ImGui::PopStyleColor();
        ImGui::Button("Select Texture...", ImVec2(-1, 25)); // Button text is white by default now
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("(No texture selected)");
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("Physics")) {
        bool isStatic = true;
        ImGui::Checkbox("Static", &isStatic);

        if (!isStatic) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
            ImGui::Text("Mass");
            ImGui::PopStyleColor();
            float mass = 1.0f;
            ImGui::DragFloat("##mass", &mass, 0.1f, 0.1f, 1000.0f);
        }
    }

    if (ImGui::CollapsingHeader("Paint Mode")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
        ImGui::Text("Texture Atlas");
        ImGui::Text("(Available in Paint Mode)");
        ImGui::PopStyleColor();
        ImGui::Button("Switch to Paint Mode", ImVec2(-1, 25)); // Button text is white by default now
    }
}

void MainWindow::RenderAssetBrowser()
{
    if (ImGui::BeginTabBar("AssetTabs")) {
        bool texturesOpen = ImGui::BeginTabItem("Textures");

        if (texturesOpen) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
            ImGui::Text("Available Textures:");
            ImGui::PopStyleColor();
            for (int i = 0; i < 5; ++i) {
                ImGui::Button(("Texture_0" + std::to_string(i) + ".png").c_str(), ImVec2(100, 60));
                if ((i + 1) % 4 != 0) ImGui::SameLine();
            }
            ImGui::EndTabItem();
        }

        bool modelsOpen = ImGui::BeginTabItem("Models");

        if (modelsOpen) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
            ImGui::Text("Available Models:");
            ImGui::PopStyleColor();
            ImGui::Button("Cube.obj", ImVec2(120, 25));
            ImGui::Button("Cylinder.obj", ImVec2(120, 25));
            ImGui::Button("Sphere.obj", ImVec2(120, 25));
            ImGui::Button("Player.mdl", ImVec2(120, 25));
            ImGui::EndTabItem();
        }

        bool materialsOpen = ImGui::BeginTabItem("Materials");

        if (materialsOpen) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
            ImGui::Text("Available Materials:");
            ImGui::PopStyleColor();
            ImGui::Button("Default", ImVec2(120, 25));
            ImGui::Button("Metal", ImVec2(120, 25));
            ImGui::Button("Wood", ImVec2(120, 25));
            ImGui::Button("Concrete", ImVec2(120, 25));
            ImGui::EndTabItem();
        }

        bool prefabsOpen = ImGui::BeginTabItem("Prefabs");

        if (prefabsOpen) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
            ImGui::Text("Available Prefabs:");
            ImGui::PopStyleColor();
            ImGui::Button("Player Spawn", ImVec2(120, 25));
            ImGui::Button("Light Source", ImVec2(120, 25));
            ImGui::Button("Door", ImVec2(120, 25));
            ImGui::Button("Button", ImVec2(120, 25));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text for labels
    ImGui::Text("Drag items into viewports to place them in the scene");
    ImGui::PopStyleColor();
}

void MainWindow::RenderStatusBar()
{
    // Status bar is integrated into the main window layout
    // For now, we'll skip a separate status bar since ImGui handles docking
}

void MainWindow::RenderContextMenu()
{
    if (ImGui::BeginPopup("CreateMenu")) {
        // Brushes submenu
        if (ImGui::BeginMenu("Brushes")) {
            if (ImGui::MenuItem("Cube")) {
                CreateObjectAtContextMenu(PrimitiveType::Cube);
            }
            if (ImGui::MenuItem("Cylinder")) {
                CreateObjectAtContextMenu(PrimitiveType::Cylinder);
            }
            if (ImGui::MenuItem("Sphere")) {
                CreateObjectAtContextMenu(PrimitiveType::Sphere);
            }
            if (ImGui::MenuItem("Pyramid")) {
                CreateObjectAtContextMenu(PrimitiveType::Pyramid);
            }
            if (ImGui::MenuItem("Prism")) {
                CreateObjectAtContextMenu(PrimitiveType::Prism);
            }
            ImGui::EndMenu();
        }

        // Game Objects submenu
        if (ImGui::BeginMenu("Game Objects")) {
            if (ImGui::MenuItem("Model")) {
                // TODO: Create model object
            }
            if (ImGui::MenuItem("Sprite")) {
                // TODO: Create sprite object
            }
            if (ImGui::MenuItem("Composite")) {
                // TODO: Create composite object
            }
            if (ImGui::MenuItem("Player Spawn")) {
                // TODO: Create player spawn
            }
            ImGui::EndMenu();
        }

        // Lights submenu
        if (ImGui::BeginMenu("Lights")) {
            if (ImGui::MenuItem("Point Light")) {
                // TODO: Create point light
            }
            if (ImGui::MenuItem("Spot Light")) {
                // TODO: Create spot light
            }
            if (ImGui::MenuItem("Directional Light")) {
                // TODO: Create directional light
            }
            ImGui::EndMenu();
        }

        // Audio submenu
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("Sound Emitter")) {
                // TODO: Create sound emitter
            }
            if (ImGui::MenuItem("Ambient Zone")) {
                // TODO: Create ambient audio zone
            }
            ImGui::EndMenu();
        }

        // Triggers submenu
        if (ImGui::BeginMenu("Triggers")) {
            if (ImGui::MenuItem("Trigger Box")) {
                // TODO: Create trigger box
            }
            if (ImGui::MenuItem("Trigger Sphere")) {
                // TODO: Create trigger sphere
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
}

void MainWindow::CreateObjectAtContextMenu(PrimitiveType type)
{
    if (contextMenuViewport_ < 0 || contextMenuViewport_ >= 4) return;

    // Use the stored context menu position
    ImVec2 clickPos = contextMenuPos_;

    // Convert screen position to world position for the correct viewport
    // We need to get the canvas info for the viewport that was right-clicked
    // This is tricky since we're in a popup context. For now, use a simplified approach
    // by assuming standard viewport coordinates

    // Create at the stored position using the viewport's transform
    // This is a simplified implementation - in a full editor we'd need more sophisticated
    // viewport coordinate tracking

    // Get canvas info for the viewport that was right-clicked
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 20.0f; // Reserve space for text
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Convert click position to world coordinates for this viewport
    ImVec2 worldPos = ScreenToWorld(clickPos, canvasPos, canvasSize,
                                    zoomLevels_[contextMenuViewport_], panOffsets_[contextMenuViewport_]);

    // Snap to grid
    ImVec2 snappedPos = gridManager_.SnapToGrid(worldPos);

    Brush newBrush = {
        type,
        {snappedPos.x, snappedPos.y, 0.0f}, // Position (snapped to grid)
        {1.0f, 1.0f, 1.0f},                // Size (1x1x1)
        0.0f                               // Rotation
    };

    // Execute command for undo/redo support
    ExecuteCommand(std::make_unique<CreateBrushCommand>(this, newBrush));
}

// Brush/primitive functions
void MainWindow::StartBrushCreation(PrimitiveType type)
{
    isCreatingBrush_ = true;
    creatingType_ = type;
}

void MainWindow::UpdateBrushCreation(ImVec2 currentMousePos, int viewportIndex)
{
    // For now, just track the creation - actual brush will be created on mouse release
    // Could add visual feedback here for brush size during drag
}

void MainWindow::FinishBrushCreation()
{
    if (creationViewport_ >= 0 && creationViewport_ < 4) {
        // Convert screen positions to world positions
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y -= 20.0f; // Reserve space for text
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        ImVec2 startWorld = ScreenToWorld(creationStartPos_, canvasPos, canvasSize,
                                         zoomLevels_[creationViewport_], panOffsets_[creationViewport_]);
        ImVec2 currentWorld = ScreenToWorld(ImGui::GetMousePos(), canvasPos, canvasSize,
                                           zoomLevels_[creationViewport_], panOffsets_[creationViewport_]);

        // Snap positions to grid
        startWorld = gridManager_.SnapToGrid(startWorld);
        currentWorld = gridManager_.SnapToGrid(currentWorld);

        // Calculate size (minimum 1x1 unit)
        Vector3 size = {
            std::max(std::abs(currentWorld.x - startWorld.x), 1.0f),
            std::max(std::abs(currentWorld.y - startWorld.y), 1.0f),
            1.0f // Default depth
        };

        // Calculate position (top-left corner at start position for orthographic views)
        // For orthographic views, position the brush with its corner at the start point
        Vector3 position = {
            std::min(startWorld.x, currentWorld.x),
            std::min(startWorld.y, currentWorld.y),
            0.0f
        };

        // Create the brush using command system for undo/redo support
        Brush newBrush = {
            creatingType_,
            position,
            size,
            0.0f // Rotation
        };

        ExecuteCommand(std::make_unique<CreateBrushCommand>(this, newBrush));
    }

    CancelBrushCreation();
}

void MainWindow::CancelBrushCreation()
{
    isCreatingBrush_ = false;
    creationStartPos_ = ImVec2(0, 0);
    creationViewport_ = -1;
    SetMouseCursor(ImGuiMouseCursor_Arrow);
}

void MainWindow::RenderBrushes(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasEnd,
                               float zoomLevel, ImVec2 panOffset, int viewportIndex)
{
    ImVec2 canvasSize = ImVec2(canvasEnd.x - canvasPos.x, canvasEnd.y - canvasPos.y);

    // Map viewport index to GridManager::ViewportType
    GridManager::ViewportType viewportType;
    switch (viewportIndex) {
        case 0: viewportType = GridManager::ViewportType::TOP_XY; break;    // Bottom-left
        case 1: viewportType = GridManager::ViewportType::FRONT_XZ; break;  // Top-right
        case 2: viewportType = GridManager::ViewportType::PERSPECTIVE_3D; break; // Top-left (don't render brushes in 3D view)
        case 3: viewportType = GridManager::ViewportType::SIDE_YZ; break;   // Bottom-right
        default: viewportType = GridManager::ViewportType::TOP_XY; break;
    }

    // Don't render brushes in perspective view (they would need 3D projection)
    if (viewportType == GridManager::ViewportType::PERSPECTIVE_3D) {
        return;
    }

    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        
        // Project 3D position to 2D screen coordinates based on viewport type
        ImVec2 projectedPos = gridManager_.Project3DTo2D(brush.position, viewportType);

        // Convert projected world position to screen position
        ImVec2 screenPos = WorldToScreen(projectedPos, canvasPos, canvasSize, zoomLevel, panOffset);

        // Calculate screen size based on projected dimensions
        float screenWidth, screenHeight;
        switch (viewportType) {
            case GridManager::ViewportType::TOP_XY:
                screenWidth = brush.size.x * zoomLevel;
                screenHeight = brush.size.y * zoomLevel;
                break;
            case GridManager::ViewportType::FRONT_XZ:
                screenWidth = brush.size.x * zoomLevel;
                screenHeight = brush.size.z * zoomLevel;
                break;
            case GridManager::ViewportType::SIDE_YZ:
                screenWidth = brush.size.y * zoomLevel;
                screenHeight = brush.size.z * zoomLevel;
                break;
            default:
                screenWidth = brush.size.x * zoomLevel;
                screenHeight = brush.size.y * zoomLevel;
                break;
        }

        // Calculate bounding rectangle
        ImVec2 minPos = ImVec2(screenPos.x - screenWidth * 0.5f, screenPos.y - screenHeight * 0.5f);
        ImVec2 maxPos = ImVec2(screenPos.x + screenWidth * 0.5f, screenPos.y + screenHeight * 0.5f);

        // Only draw if visible in viewport
        if (maxPos.x >= canvasPos.x && minPos.x <= canvasEnd.x &&
            maxPos.y >= canvasPos.y && minPos.y <= canvasEnd.y) {
            
            ImU32 outlineColor = IM_COL32(100, 149, 237, 255);
            ImU32 fillColor = IM_COL32(100, 149, 237, 50);
            
            // Draw different shapes based on primitive type in 2D projection
            switch (brush.type) {
                case PrimitiveType::Cube:
                    drawList->AddRect(minPos, maxPos, outlineColor, 0.0f, 0, 2.0f);
                    drawList->AddRectFilled(minPos, maxPos, fillColor);
                    break;
                    
                case PrimitiveType::Cylinder:
                    // Draw circle for cylinder (uses max dimension as radius)
                    {
                        float radius = std::max(screenWidth, screenHeight) * 0.5f;
                        drawList->AddCircle(screenPos, radius, outlineColor, 16, 2.0f);
                        drawList->AddCircleFilled(screenPos, radius, fillColor);
                    }
                    break;
                    
                case PrimitiveType::Sphere:
                    // Draw circle for sphere (uses x dimension as radius)
                    {
                        float radius = screenWidth * 0.5f;
                        drawList->AddCircle(screenPos, radius, outlineColor, 16, 2.0f);
                        drawList->AddCircleFilled(screenPos, radius, fillColor);
                    }
                    break;
                    
                case PrimitiveType::Pyramid:
                    // Draw triangle pointing upward
                    {
                        ImVec2 p1 = ImVec2(screenPos.x, minPos.y); // Top point
                        ImVec2 p2 = ImVec2(minPos.x, maxPos.y);    // Bottom left
                        ImVec2 p3 = ImVec2(maxPos.x, maxPos.y);    // Bottom right
                        drawList->AddTriangle(p1, p2, p3, outlineColor, 2.0f);
                        drawList->AddTriangleFilled(p1, p2, p3, fillColor);
                    }
                    break;
                    
                case PrimitiveType::Prism:
                    // Draw hexagon for prism
                    {
                        float radius = std::min(screenWidth, screenHeight) * 0.5f;
                        ImVec2 center = screenPos;
                        std::vector<ImVec2> points;
                        for (int j = 0; j < 6; ++j) {
                            float angle = j * (2.0f * 3.14159f / 6.0f);
                            points.push_back(ImVec2(center.x + radius * cosf(angle), 
                                                  center.y + radius * sinf(angle)));
                        }
                        // Draw outline
                        for (int j = 0; j < 6; ++j) {
                            drawList->AddLine(points[j], points[(j + 1) % 6], outlineColor, 2.0f);
                        }
                        // Draw filled (approximate with triangles)
                        for (int j = 1; j < 5; ++j) {
                            drawList->AddTriangleFilled(points[0], points[j], points[j + 1], fillColor);
                        }
                    }
                    break;
            }
        }
    }
}

ImVec2 MainWindow::ScreenToWorld(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize,
                                float zoomLevel, ImVec2 panOffset)
{
    // Convert screen coordinates to world coordinates
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

    // Apply inverse pan and zoom transformations
    float worldX = (screenPos.x - center.x) / zoomLevel + panOffset.x;
    float worldY = (screenPos.y - center.y) / zoomLevel + panOffset.y;

    return ImVec2(worldX, worldY);
}

void MainWindow::RenderScene3D(ImVec2 canvasPos, ImVec2 canvasSize)
{
    // Set up the scissor rectangle to only render within this viewport
    rlDrawRenderBatchActive();
    rlViewport((int)canvasPos.x, (int)(GetScreenHeight() - canvasPos.y - canvasSize.y),
               (int)canvasSize.x, (int)canvasSize.y);
    rlScissor((int)canvasPos.x, (int)(GetScreenHeight() - canvasPos.y - canvasSize.y),
              (int)canvasSize.x, (int)canvasSize.y);

    // Begin 3D mode with the current camera
    Camera3D camera = cameraManager_.GetRaylibCamera();
    BeginMode3D(camera);

    // Draw skybox if enabled
    if (showSkybox_) {
        RenderSkybox();
    }

    // Draw brushes as 3D primitives
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        Color fillColor = {100, 149, 237, 100}; // Semi-transparent cornflower blue
        Color wireColor = BLUE;
        
        switch (brush.type) {
            case PrimitiveType::Cube:
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
                
            case PrimitiveType::Cylinder:
                DrawCylinderWires(brush.position, brush.size.x, brush.size.x, brush.size.y, 16, wireColor);
                DrawCylinder(brush.position, brush.size.x, brush.size.x, brush.size.y, 16, fillColor);
                break;
                
            case PrimitiveType::Sphere:
                DrawSphereWires(brush.position, brush.size.x, 16, 16, wireColor);
                DrawSphere(brush.position, brush.size.x, fillColor);
                break;
                
            case PrimitiveType::Pyramid:
                // Draw pyramid as custom mesh approximation
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
                
            case PrimitiveType::Prism:
                // Draw prism as scaled cube for now
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
        }
    }

    // Draw axis lines in 3D space
    DrawLine3D({0, 0, 0}, {10, 0, 0}, RED);   // X axis
    DrawLine3D({0, 0, 0}, {0, 10, 0}, GREEN); // Y axis
    DrawLine3D({0, 0, 0}, {0, 0, 10}, BLUE);  // Z axis

    // Draw gizmos if selection exists
    if (selectionManager_.HasSelection()) {
        // TODO: Implement 3D gizmo rendering
        // For now, we'll use the 2D gizmo system for the 3D view
        if (!cachedCamera_) {
            cachedCamera_ = new Camera3D(cameraManager_.GetRaylibCamera());
        } else {
            *cachedCamera_ = cameraManager_.GetRaylibCamera();
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        gizmoManager_.RenderGizmo(drawList, canvasPos, canvasSize, 1.0f, {0, 0}, *cachedCamera_);
    }

    EndMode3D();

    // Reset scissor and viewport
    rlScissor(0, 0, GetScreenWidth(), GetScreenHeight());
    rlViewport(0, 0, GetScreenWidth(), GetScreenHeight());

    // Return to ImGui drawing mode
    rlDrawRenderBatchActive();
}

void MainWindow::SetMouseCursor(ImGuiMouseCursor cursor)
{
    ImGui::SetMouseCursor(cursor);
}

void MainWindow::RenderPerspectiveView()
{
    // Render the 3D perspective view to its RenderTexture
    BeginTextureMode(perspectiveTexture_);

    ClearBackground(GRAY);
    
    // Get the 3D camera from camera manager
    Camera3D camera = cameraManager_.GetRaylibCamera();
    BeginMode3D(camera);
    
    // Draw skybox if enabled
    if (showSkybox_) {
        // Simple gradient skybox
        DrawPlane({0, 0, 0}, {100, 100}, SKYBLUE);
    }
    
    // Draw all brushes as 3D primitives
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        Color fillColor = {100, 149, 237, 100}; // Semi-transparent cornflower blue
        Color wireColor = BLUE;
        
        switch (brush.type) {
            case PrimitiveType::Cube:
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
                
            case PrimitiveType::Cylinder:
                DrawCylinderWires(brush.position, brush.size.x, brush.size.x, brush.size.y, 16, wireColor);
                DrawCylinder(brush.position, brush.size.x, brush.size.x, brush.size.y, 16, fillColor);
                break;
                
            case PrimitiveType::Sphere:
                DrawSphereWires(brush.position, brush.size.x, 16, 16, wireColor);
                DrawSphere(brush.position, brush.size.x, fillColor);
                break;
                
            case PrimitiveType::Pyramid:
                // Draw pyramid as custom mesh approximation
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
                
            case PrimitiveType::Prism:
                // Draw prism as scaled cube for now
                DrawCubeWires(brush.position, brush.size.x, brush.size.y, brush.size.z, wireColor);
                DrawCube(brush.position, brush.size.x, brush.size.y, brush.size.z, fillColor);
                break;
        }
    }
    
    // Draw axis lines in 3D space
    DrawLine3D({0, 0, 0}, {10, 0, 0}, RED);   // X axis
    DrawLine3D({0, 0, 0}, {0, 10, 0}, GREEN); // Y axis
    DrawLine3D({0, 0, 0}, {0, 0, 10}, BLUE);  // Z axis
    
    EndMode3D();
    EndTextureMode();
}

void MainWindow::RenderTopView()
{
    // Render the top view (X/Y plane) to its RenderTexture
    BeginTextureMode(topTexture_);

    ClearBackground(GRAY);

    // Set up orthographic camera looking down the Z axis
    // Use zoom level to control the orthographic view size - higher zoom = smaller view area
    float orthoSize = 45.0f / zoomLevels_[0];  // Index 0 = top view

    Camera3D orthoCamera = { 0 };
    orthoCamera.position = {0.0f, 0.0f, 50.0f};  // Looking down from above
    orthoCamera.target = {0.0f, 0.0f, 0.0f};     // Looking at origin
    orthoCamera.up = {0.0f, 1.0f, 0.0f};         // Y is up
    orthoCamera.fovy = orthoSize;
    orthoCamera.projection = CAMERA_ORTHOGRAPHIC;
    
    BeginMode3D(orthoCamera);
    
    // Draw grid if enabled
    if (showGrids_) {
        DrawGrid(20, 1.0f);
    }
    
    // Draw all brushes projected to top view
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        
        // Project to X/Y plane (top view)
        Vector3 projectedPos = {brush.position.x, brush.position.y, 0.0f};
        Vector3 projectedSize = {brush.size.x, brush.size.y, 0.1f}; // Thin in Z
        
        DrawCubeWires(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, BLUE);
        DrawCube(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, {100, 149, 237, 100});
    }
    
    // Draw axis indicators
    DrawLine3D({0, 0, 0}, {5, 0, 0}, RED);   // X axis
    DrawLine3D({0, 0, 0}, {0, 5, 0}, GREEN); // Y axis
    
    EndMode3D();
    EndTextureMode();
}

void MainWindow::RenderFrontView()
{
    // Render the front view (X/Z plane) to its RenderTexture
    BeginTextureMode(frontTexture_);

    ClearBackground(GRAY);

    // Set up orthographic camera looking along the Y axis
    // Use zoom level to control the orthographic view size - higher zoom = smaller view area
    float orthoSize = 45.0f / zoomLevels_[1];  // Index 1 = front view

    Camera3D orthoCamera = { 0 };
    orthoCamera.position = {0.0f, -50.0f, 0.0f}; // Looking from negative Y
    orthoCamera.target = {0.0f, 0.0f, 0.0f};     // Looking at origin
    orthoCamera.up = {0.0f, 0.0f, 1.0f};         // Z is up
    orthoCamera.fovy = orthoSize;
    orthoCamera.projection = CAMERA_ORTHOGRAPHIC;
    
    BeginMode3D(orthoCamera);
    
    // Draw grid if enabled
    if (showGrids_) {
        DrawGrid(20, 1.0f);
    }
    
    // Draw all brushes projected to front view
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        
        // Project to X/Z plane (front view)
        Vector3 projectedPos = {brush.position.x, 0.0f, brush.position.z};
        Vector3 projectedSize = {brush.size.x, 0.1f, brush.size.z}; // Thin in Y
        
        DrawCubeWires(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, BLUE);
        DrawCube(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, {100, 149, 237, 100});
    }
    
    // Draw axis indicators
    DrawLine3D({0, 0, 0}, {5, 0, 0}, RED);   // X axis
    DrawLine3D({0, 0, 0}, {0, 0, 5}, BLUE);  // Z axis
    
    EndMode3D();
    EndTextureMode();
}

void MainWindow::RenderSideView()
{
    // Render the side view (Y/Z plane) to its RenderTexture
    BeginTextureMode(sideTexture_);

    ClearBackground(GRAY);
    
    // Set up orthographic camera looking along the X axis
    // Use zoom level to control the orthographic view size - higher zoom = smaller view area
    float orthoSize = 45.0f / zoomLevels_[3];  // Index 3 = side view

    Camera3D orthoCamera = { 0 };
    orthoCamera.position = {50.0f, 0.0f, 0.0f};  // Looking from positive X
    orthoCamera.target = {0.0f, 0.0f, 0.0f};     // Looking at origin
    orthoCamera.up = {0.0f, 0.0f, 1.0f};         // Z is up
    orthoCamera.fovy = orthoSize;
    orthoCamera.projection = CAMERA_ORTHOGRAPHIC;
    
    BeginMode3D(orthoCamera);
    
    // Draw grid if enabled
    if (showGrids_) {
        DrawGrid(20, 1.0f);
    }
    
    // Draw all brushes projected to side view
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        
        // Project to Y/Z plane (side view)
        Vector3 projectedPos = {0.0f, brush.position.y, brush.position.z};
        Vector3 projectedSize = {0.1f, brush.size.y, brush.size.z}; // Thin in X
        
        DrawCubeWires(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, BLUE);
        DrawCube(projectedPos, projectedSize.x, projectedSize.y, projectedSize.z, {100, 149, 237, 100});
    }
    
    // Draw axis indicators
    DrawLine3D({0, 0, 0}, {0, 5, 0}, GREEN); // Y axis
    DrawLine3D({0, 0, 0}, {0, 0, 5}, BLUE);  // Z axis
    
    EndMode3D();
    EndTextureMode();
}


void MainWindow::HandleGridInput()
{
    // Handle bracket keys for grid scaling (Source SDK Hammer standard)
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        gridManager_.DecreaseGridSize();
        // Update gizmo grid size
        gizmoManager_.SetGridSize(static_cast<float>(gridManager_.GetCurrentGridSize()));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        gridManager_.IncreaseGridSize();
        // Update gizmo grid size
        gizmoManager_.SetGridSize(static_cast<float>(gridManager_.GetCurrentGridSize()));
    }
}

void MainWindow::HandleSelectionInput()
{
    // Delegate to SelectionManager
    selectionManager_.HandleInput();
}

void MainWindow::HandleGizmoInput()
{
    // Delegate to GizmoManager
    gizmoManager_.HandleInput();
}

void MainWindow::SetupSelectionCallbacks()
{
    // Setup object picking callback
    selectionManager_.GetObjectsAtPosition = [this](ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset) {
        return GetObjectsAtPosition(screenPos, canvasPos, canvasSize, zoomLevel, panOffset);
    };

    // Setup object position callback
    selectionManager_.GetObjectPosition = [this](SelectionManager::ObjectID objectId) {
        return GetObjectPosition(objectId);
    };

    // TODO: Setup vertex/edge/face callbacks when mesh editing is implemented
}

std::vector<SelectionManager::ObjectID> MainWindow::GetObjectsAtPosition(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, float zoomLevel, ImVec2 panOffset)
{
    std::vector<SelectionManager::ObjectID> result;
    
    // Convert screen position to world position
    ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    float worldX = (screenPos.x - center.x) / zoomLevel + panOffset.x;
    float worldY = (screenPos.y - center.y) / zoomLevel + panOffset.y;
    
    // Check each brush for intersection
    for (size_t i = 0; i < GetBrushCount(); ++i) {
        const auto& brush = GetBrush(i);
        
        // Simple bounding box check
        float halfWidth = brush.size.x * 0.5f;
        float halfHeight = brush.size.y * 0.5f;
        
        if (worldX >= brush.position.x - halfWidth && worldX <= brush.position.x + halfWidth &&
            worldY >= brush.position.y - halfHeight && worldY <= brush.position.y + halfHeight) {
            result.push_back(static_cast<SelectionManager::ObjectID>(i));
        }
    }
    
    return result;
}

Vector3 MainWindow::GetObjectPosition(SelectionManager::ObjectID objectId)
{
    if (objectId < GetBrushCount()) {
        return GetBrush(objectId).position;
    }
    return {0, 0, 0};
}

Vector3 MainWindow::GetBrushPosition(uint32_t brushIndex)
{
    if (brushIndex < GetBrushCount()) {
        return GetBrush(brushIndex).position;
    }
    return {0, 0, 0};
}

// Undo/Redo system implementation
void MainWindow::ExecuteCommand(std::unique_ptr<Command> command)
{
    commandManager_.ExecuteCommand(std::move(command));
}

void MainWindow::Undo()
{
    commandManager_.Undo();
}

void MainWindow::Redo()
{
    commandManager_.Redo();
}

// Brush management methods (delegate to BrushManager)
size_t MainWindow::AddBrush(const Brush& brush)
{
    return brushManager_.CreateBrush(brush);
}

void MainWindow::RemoveBrush(size_t index)
{
    brushManager_.RemoveBrush(index);
}

const Brush& MainWindow::GetBrush(size_t index) const
{
    return brushManager_.GetBrush(index);
}

size_t MainWindow::GetBrushCount() const
{
    return brushManager_.GetBrushCount();
}
