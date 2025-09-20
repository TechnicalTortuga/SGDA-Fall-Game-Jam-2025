#pragma once

#include "../ecs/System.h"
#include "../ecs/Entity.h"
#include "../ecs/Components/Player.h"
#include "../ecs/Systems/CollisionSystem.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "raylib.h"

// Color constants for console output
const Color CYAN = {0, 255, 255, 255};

// Console command function signature
using ConsoleCommand = std::function<void(const std::vector<std::string>&)>;

// Console log entry
struct ConsoleLogEntry {
    std::string message;
    Color color;
    float timestamp;

    ConsoleLogEntry(const std::string& msg, Color c = WHITE)
        : message(msg), color(c), timestamp(static_cast<float>(GetTime())) {}
};

/**
 * In-game developer console system for debugging and commands
 */
class ConsoleSystem : public System {
public:
    ConsoleSystem();
    ~ConsoleSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Render() override;
    void Initialize() override;
    void Shutdown() override;

    // Console control
    void ToggleConsole() { isVisible_ = !isVisible_; }
    bool IsVisible() const { return isVisible_; }
    void ShowConsole() { isVisible_ = true; }
    void HideConsole() { isVisible_ = false; }

    // Command registration
    void RegisterCommand(const std::string& name, ConsoleCommand command,
                        const std::string& description = "");
    void UnregisterCommand(const std::string& name);

    // Command execution
    void ExecuteCommand(const std::string& commandLine);
    void ExecuteCommand(const std::string& command, const std::vector<std::string>& args);

    // Logging
    void Log(const std::string& message, Color color = WHITE);
    void LogInfo(const std::string& message) { Log(message, GREEN); }
    void LogWarning(const std::string& message) { Log(message, YELLOW); }
    void LogError(const std::string& message) { Log(message, RED); }

    // Console state
    const std::string& GetCurrentInput() const { return currentInput_; }
    const std::vector<ConsoleLogEntry>& GetLogEntries() const { return logEntries_; }
    const std::vector<std::string>& GetCommandHistory() const { return commandHistory_; }

    // Configuration
    void SetMaxLogEntries(int maxEntries) { maxLogEntries_ = maxEntries; }
    void SetConsoleHeight(float height) { consoleHeight_ = height; }
    void SetFontSize(int size) { fontSize_ = size; }

    // External system references for commands
    void SetCollisionSystem(System* collisionSystem) { collisionSystem_ = collisionSystem; }
    void SetPlayerEntity(Entity* player) { playerEntity_ = player; }

private:
    // Console state
    bool isVisible_;
    std::string currentInput_;
    int cursorPosition_;
    float consoleHeight_;
    int fontSize_;
    int maxLogEntries_;

    // Console data
    std::vector<ConsoleLogEntry> logEntries_;
    std::vector<std::string> commandHistory_;
    int historyIndex_;
    std::unordered_map<std::string, ConsoleCommand> commands_;
    std::unordered_map<std::string, std::string> commandDescriptions_;

    // External system references
    System* collisionSystem_;
    Entity* playerEntity_;

    // Input handling
    void HandleInput();
    void ProcessKeyInput();
    void ProcessTextInput();

    // Command parsing
    std::vector<std::string> ParseCommandLine(const std::string& commandLine);
    std::string TrimWhitespace(const std::string& str) const;

    // Rendering
    void RenderConsole() const;
    void RenderBackground() const;
    void RenderLog() const;
    void RenderInputLine() const;
    void RenderCursor() const;

    // History management
    void AddToHistory(const std::string& command);
    void NavigateHistory(int direction);

    // Built-in commands
    void InitializeBuiltInCommands();
    void CmdHelp(const std::vector<std::string>& args);
    void CmdClear(const std::vector<std::string>& args);
    void CmdEcho(const std::vector<std::string>& args);
    void CmdList(const std::vector<std::string>& args);
    void CmdNoClip(const std::vector<std::string>& args);
    void CmdRenderBounds(const std::vector<std::string>& args);

    // Utility
    std::string GetTimestampString() const;
    Color GetColorForLogLevel(const std::string& level) const;
};
