#include "EditorConfig.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>



EditorConfig::EditorConfig()
{
    configFilePath_ = GetConfigFilePath();
}

EditorConfig::~EditorConfig()
{
    // Config is saved in Application::Shutdown()
}

bool EditorConfig::Load()
{
    if (!std::filesystem::exists(configFilePath_)) {
        
        return LoadDefaults();
    }

    // TODO: Implement YAML loading for config
    
    return LoadDefaults(); // For now, just use defaults
}

bool EditorConfig::Save()
{
    // TODO: Implement YAML saving for config
    
    return true;
}

bool EditorConfig::LoadDefaults()
{
    // Set sensible defaults
    viewport.showGrid = true;
    viewport.gridSize = 64.0f;
    viewport.showCrosshairs = true;
    viewport.showWireframe = true;
    viewport.cameraSpeed = 5.0f;

    ui.toolbarWidth = 60.0f;
    ui.inspectorWidth = 300.0f;
    ui.assetBrowserHeight = 200.0f;
    ui.showStatusBar = true;

    return true;
}

std::string EditorConfig::GetConfigFilePath() const
{
    // Store config in user's home directory
    std::string homeDir = getenv("HOME") ? getenv("HOME") : ".";
    return homeDir + "/.paintstrike_editor/config.yaml";
}


