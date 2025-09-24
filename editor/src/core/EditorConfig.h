#pragma once

#include <string>
#include <vector>



class EditorConfig {
public:
    EditorConfig();
    ~EditorConfig();

    bool Load();
    bool Save();

    // Configuration properties
    struct ViewportSettings {
        bool showGrid = true;
        float gridSize = 64.0f;
        bool showCrosshairs = true;
        bool showWireframe = true;
        float cameraSpeed = 5.0f;
    };

    struct UISettings {
        float toolbarWidth = 60.0f;
        float inspectorWidth = 300.0f;
        float assetBrowserHeight = 200.0f;
        bool showStatusBar = true;
    };

    ViewportSettings viewport;
    UISettings ui;

    std::string lastProjectPath;
    std::vector<std::string> recentProjects;

private:
    std::string configFilePath_;

    bool LoadDefaults();
    std::string GetConfigFilePath() const;
};


