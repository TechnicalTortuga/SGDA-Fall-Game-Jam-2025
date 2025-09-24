#pragma once



class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    bool Initialize() { return true; }
    void Shutdown() {}
    void Update(float deltaTime) {}
};


