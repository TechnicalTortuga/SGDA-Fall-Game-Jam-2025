#pragma once

#include <vector>
#include <memory>
#include <string>
#include "raylib.h"

// Forward declarations
class MainWindow;
enum class PrimitiveType { Cube, Cylinder, Sphere, Pyramid, Prism };

// Brush struct definition (shared between MainWindow and CommandManager)
struct Brush {
    PrimitiveType type;
    Vector3 position;
    Vector3 size;
    float rotation; // Simple Y rotation for 2D views
};

// Command pattern for undo/redo
class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string GetDescription() const = 0;
};

// Command Manager
class CommandManager {
public:
    CommandManager();
    ~CommandManager() = default;

    void ExecuteCommand(std::unique_ptr<Command> command);
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    void Clear();

private:
    std::vector<std::unique_ptr<Command>> history_;
    size_t currentIndex_;
};

// Specific command implementations
class CreateBrushCommand : public Command {
public:
    CreateBrushCommand(MainWindow* mainWindow, const struct Brush& brush);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    MainWindow* mainWindow_;
    struct Brush brush_;
    size_t brushIndex_;
};

class DeleteBrushCommand : public Command {
public:
    DeleteBrushCommand(MainWindow* mainWindow, size_t brushIndex);
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    MainWindow* mainWindow_;
    struct Brush deletedBrush_;
    size_t brushIndex_;
};
