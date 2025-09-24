#include <raylib.h>
#include <rlImGui.h>
#include <imgui.h>

#include "src/core/Application.h"

int main(int argc, char* argv[])
{
    // Initialize Raylib - normal resizable window (optimized for 13" MacBook)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Paint Strike Level Editor");
    SetTargetFPS(60);

    // Initialize rlImGui
    rlImGuiSetup(true);
    
    // Enable docking for editor-style interface (requires docking branch)
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    
    // Ensure proper fullscreen layout
    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0.0f;
    style.WindowRounding = 0.0f;

    // Customize colors for professional appearance
    // Button colors - solid blue theme
    ImGui::GetStyle().Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);        // Normal button - soft blue
    ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 0.9f, 1.0f); // Hovered - slightly brighter blue
    ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.35f, 0.75f, 1.0f); // Active - slightly darker blue

    // Text colors - white for buttons/tabs, black for specific labels
    ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);           // Regular text (buttons, tabs) - white
    ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);   // Disabled text - gray
    ImGui::GetStyle().Colors[ImGuiCol_TabActive] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);      // Active tab - blue
    ImGui::GetStyle().Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Inactive tab - dark gray
    ImGui::GetStyle().Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);          // Tab background - dark gray
    ImGui::GetStyle().Colors[ImGuiCol_TabHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);     // Tab hover - lighter gray
    ImGui::GetStyle().Colors[ImGuiCol_Header] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);          // Collapsing headers - dark gray
    ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);  // Header hover - slightly lighter
    ImGui::GetStyle().Colors[ImGuiCol_HeaderActive] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);   // Header active - blue

    // Window and frame colors for consistency
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);     // Light gray window background
    ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);         // Input field backgrounds
    ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); // Hovered input fields
    ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);   // Active input fields

    // Create and run editor application
    Application app;
    if (!app.Initialize(argc, argv)) {
        rlImGuiShutdown();
        CloseWindow();
        return 1;
    }

    // Main loop
    while (!WindowShouldClose())
    {
        // Update
        app.Update(GetFrameTime());

        // Render
        BeginDrawing();
        ClearBackground(DARKGRAY);

        rlImGuiBegin();
        app.Render();
        rlImGuiEnd();

        EndDrawing();
    }

    // Cleanup
    app.Shutdown();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
