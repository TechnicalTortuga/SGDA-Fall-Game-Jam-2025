#include "Application.h"
#include "../ui/MainWindow.h"
#include "Logger.h"

#include <raylib.h>
#include <imgui.h>

Application::Application()
    : shouldExit_(false), mainWindow_(nullptr)
{
}

Application::~Application()
{
    Shutdown();
}

bool Application::Initialize(int argc, char* argv[])
{
    LOG_INFO("Initializing Paint Strike Level Editor");

    // Initialize main window
    mainWindow_ = new MainWindow();
    if (!mainWindow_->Initialize(this)) {
        LOG_ERROR("Failed to initialize main window");
        return false;
    }

    LOG_INFO("Paint Strike Level Editor initialized successfully");
    return true;
}

void Application::Update(float deltaTime)
{
    // TODO: Update editor systems
}

void Application::Render()
{
    // Create dockspace that allows content to show through
    // This is the proper way to create editor-style layouts with ImGui docking branch
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, NULL, ImGuiDockNodeFlags_PassthruCentralNode);

    // Main window handles all ImGui rendering within the docking context
    if (mainWindow_) {
        mainWindow_->Render();
    }
}

void Application::Shutdown()
{
    if (mainWindow_) {
        mainWindow_->Shutdown();
        delete mainWindow_;
        mainWindow_ = nullptr;
    }

    LOG_INFO("Paint Strike Level Editor shutdown complete");
}
