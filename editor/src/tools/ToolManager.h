#pragma once



class Scene;
class InputManager;

class ToolManager {
public:
    ToolManager() = default;
    ~ToolManager() = default;

    bool Initialize(Scene* scene, InputManager* input) { return true; }
    void Shutdown() {}
    void Update(float deltaTime) {}
};


