#pragma once

#include "../System.h"
#include "../../rendering/Renderer.h"
#include "../Components/Velocity.h"
#include <vector>

class RenderSystem : public System {
public:
    RenderSystem();
    ~RenderSystem();

    void Initialize() override;
    void Update(float deltaTime) override;  // Called during update phase - does nothing for rendering
    void Render() override;  // Called during render phase - does the actual rendering

    // Renderer access
    Renderer* GetRenderer() { return &renderer_; }
    const Renderer* GetRenderer() const { return &renderer_; }

    // Rendering options
    void SetDebugRendering(bool enabled) { debugRendering_ = enabled; }
    bool IsDebugRenderingEnabled() const { return debugRendering_; }

    // Debug info
    void SetPlayerPosition(float x, float y, float z) {
        playerPosX_ = x;
        playerPosY_ = y;
        playerPosZ_ = z;
    }

    void SetGridEnabled(bool enabled) { gridEnabled_ = enabled; }
    bool IsGridEnabled() const { return gridEnabled_; }

private:
    Renderer renderer_;
    bool debugRendering_;
    bool gridEnabled_;

    // Debug player position tracking
    float playerPosX_;
    float playerPosY_;
    float playerPosZ_;

    std::vector<RenderCommand> renderCommands_;

    void CollectRenderCommands();
    void SortRenderCommands();
    void ExecuteRenderCommands();
};
