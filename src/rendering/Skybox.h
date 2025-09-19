#pragma once
#include "raylib.h"
#include <string>

class Skybox {
public:
    Skybox();
    ~Skybox();

    bool LoadFromFile(const std::string& filePath);
    bool LoadTestSkybox();
    void Unload();
    void Render(const Camera3D& camera);
    bool IsLoaded() const;

private:
    TextureCubemap cubemap_;
    Shader shader_;
    Model model_;
    bool loaded_;

    void UnloadResources();
};
