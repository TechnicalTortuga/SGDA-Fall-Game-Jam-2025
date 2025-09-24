#pragma once



class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    bool Initialize() { return true; }
    void Shutdown() {}
    void Update(float deltaTime) {}
};


