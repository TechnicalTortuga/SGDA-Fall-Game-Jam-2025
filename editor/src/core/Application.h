#pragma once

class MainWindow;

class Application {
public:
    Application();
    ~Application();

    bool Initialize(int argc, char* argv[]);
    void Shutdown();
    void Update(float deltaTime);
    void Render();
    bool ShouldExit() const { return shouldExit_; }

private:
    bool shouldExit_;
    MainWindow* mainWindow_;
};
