#include "ConsoleSystem.h"
#include <algorithm>
#include <sstream>
#include <iostream>

ConsoleSystem::ConsoleSystem()
    : isVisible_(false)
    , currentInput_("")
    , cursorPosition_(0)
    , consoleHeight_(300.0f)
    , fontSize_(20)
    , maxLogEntries_(100)
    , historyIndex_(-1)
    , collisionSystem_(nullptr)
    , playerEntity_(nullptr)
{
}

void ConsoleSystem::Initialize() {
    InitializeBuiltInCommands();
    LogInfo("Developer console initialized. Press ~ to toggle.");
}

void ConsoleSystem::Shutdown() {
    commands_.clear();
    logEntries_.clear();
    commandHistory_.clear();
}

void ConsoleSystem::Update(float deltaTime) {
    if (!IsEnabled()) return;

    // Handle console toggle (tilde key)
    if (IsKeyPressed(KEY_GRAVE)) {
        ToggleConsole();
        if (isVisible_) {
            Log("Console opened");
        }
    }

    if (isVisible_) {
        HandleInput();
    }
}

void ConsoleSystem::Render() {
    if (!IsEnabled() || !isVisible_) return;

    RenderConsole();
}

void ConsoleSystem::HandleInput() {
    ProcessTextInput();
    ProcessKeyInput();
}

void ConsoleSystem::ProcessTextInput() {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 125) { // Printable characters
            currentInput_.insert(cursorPosition_, 1, static_cast<char>(key));
            cursorPosition_++;
        }
        key = GetCharPressed();
    }
}

void ConsoleSystem::ProcessKeyInput() {
    // Backspace
    if (IsKeyPressed(KEY_BACKSPACE) && cursorPosition_ > 0) {
        currentInput_.erase(cursorPosition_ - 1, 1);
        cursorPosition_--;
    }

    // Delete
    if (IsKeyPressed(KEY_DELETE) && cursorPosition_ < static_cast<int>(currentInput_.size())) {
        currentInput_.erase(cursorPosition_, 1);
    }

    // Enter - execute command
    if (IsKeyPressed(KEY_ENTER)) {
        if (!currentInput_.empty()) {
            ExecuteCommand(currentInput_);
            AddToHistory(currentInput_);
            currentInput_.clear();
            cursorPosition_ = 0;
            historyIndex_ = -1;
        }
    }

    // Escape - close console
    if (IsKeyPressed(KEY_ESCAPE)) {
        HideConsole();
    }

    // Cursor movement
    if (IsKeyPressed(KEY_LEFT) && cursorPosition_ > 0) {
        cursorPosition_--;
    }
    if (IsKeyPressed(KEY_RIGHT) && cursorPosition_ < static_cast<int>(currentInput_.size())) {
        cursorPosition_++;
    }

    // Command history navigation
    if (IsKeyPressed(KEY_UP)) {
        NavigateHistory(-1);
    }
    if (IsKeyPressed(KEY_DOWN)) {
        NavigateHistory(1);
    }

    // Home/End
    if (IsKeyPressed(KEY_HOME)) {
        cursorPosition_ = 0;
    }
    if (IsKeyPressed(KEY_END)) {
        cursorPosition_ = currentInput_.size();
    }
}

void ConsoleSystem::ExecuteCommand(const std::string& commandLine) {
    auto args = ParseCommandLine(commandLine);
    if (args.empty()) return;

    std::string commandName = args[0];
    args.erase(args.begin()); // Remove command name from args

    ExecuteCommand(commandName, args);
}

void ConsoleSystem::ExecuteCommand(const std::string& command, const std::vector<std::string>& args) {
    auto it = commands_.find(command);
    if (it != commands_.end()) {
        try {
            it->second(args);
            Log("> " + command + " executed", GRAY);
        } catch (const std::exception& e) {
            LogError("Command '" + command + "' failed: " + e.what());
        }
    } else {
        LogError("Unknown command: " + command);
    }
}

void ConsoleSystem::RegisterCommand(const std::string& name, ConsoleCommand command,
                                   const std::string& description) {
    commands_[name] = command;
    if (!description.empty()) {
        commandDescriptions_[name] = description;
    }
    LogInfo("Registered command: " + name);
}

void ConsoleSystem::UnregisterCommand(const std::string& name) {
    commands_.erase(name);
    commandDescriptions_.erase(name);
    LogInfo("Unregistered command: " + name);
}

void ConsoleSystem::Log(const std::string& message, Color color) {
    logEntries_.emplace_back(message, color);

    // Maintain max log entries
    if (static_cast<int>(logEntries_.size()) > maxLogEntries_) {
        logEntries_.erase(logEntries_.begin());
    }

    // Also output to console for debugging
    std::cout << "[CONSOLE] " << message << std::endl;
}

std::vector<std::string> ConsoleSystem::ParseCommandLine(const std::string& commandLine) {
    std::vector<std::string> args;
    std::stringstream ss(commandLine);
    std::string token;

    while (ss >> token) {
        args.push_back(token);
    }

    return args;
}

void ConsoleSystem::AddToHistory(const std::string& command) {
    // Remove duplicate if it exists
    auto it = std::find(commandHistory_.begin(), commandHistory_.end(), command);
    if (it != commandHistory_.end()) {
        commandHistory_.erase(it);
    }

    commandHistory_.push_back(command);

    // Limit history size
    if (commandHistory_.size() > 50) {
        commandHistory_.erase(commandHistory_.begin());
    }
}

void ConsoleSystem::NavigateHistory(int direction) {
    if (commandHistory_.empty()) return;

    historyIndex_ += direction;

    if (historyIndex_ < 0) {
        historyIndex_ = -1;
        currentInput_.clear();
        cursorPosition_ = 0;
    } else if (historyIndex_ >= static_cast<int>(commandHistory_.size())) {
        historyIndex_ = commandHistory_.size() - 1;
    } else {
        currentInput_ = commandHistory_[historyIndex_];
        cursorPosition_ = currentInput_.size();
    }
}

void ConsoleSystem::RenderConsole() const {
    RenderBackground();
    RenderLog();
    RenderInputLine();
    RenderCursor();
}

void ConsoleSystem::RenderBackground() const {
    // Semi-transparent background
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(consoleHeight_),
                 Fade(BLACK, 0.8f));

    // Border
    DrawRectangleLines(0, 0, GetScreenWidth(), static_cast<int>(consoleHeight_),
                      Fade(WHITE, 0.5f));
}

void ConsoleSystem::RenderLog() const {
    int yOffset = 10;
    int lineHeight = fontSize_ + 2;
    int maxLines = static_cast<int>((consoleHeight_ - 60) / lineHeight);

    int startIndex = std::max(0, static_cast<int>(logEntries_.size()) - maxLines);

    for (int i = startIndex; i < static_cast<int>(logEntries_.size()); ++i) {
        const auto& entry = logEntries_[i];
        DrawText(entry.message.c_str(), 10, yOffset, fontSize_, entry.color);
        yOffset += lineHeight;
    }
}

void ConsoleSystem::RenderInputLine() const {
    int inputY = static_cast<int>(consoleHeight_) - 35;
    std::string prompt = "> ";
    std::string displayText = prompt + currentInput_;

    // Input background
    DrawRectangle(0, inputY - 5, GetScreenWidth(), 30, Fade(DARKGRAY, 0.5f));

    // Input text
    DrawText(displayText.c_str(), 10, inputY, fontSize_, WHITE);
}

void ConsoleSystem::RenderCursor() const {
    if (static_cast<int>(GetTime() * 2) % 2 == 0) { // Blinking cursor
        int inputY = static_cast<int>(consoleHeight_) - 35;
        std::string prompt = "> ";
        int cursorX = 10 + MeasureText((prompt + currentInput_.substr(0, cursorPosition_)).c_str(), fontSize_);
        int cursorY = inputY;

        DrawLine(cursorX, cursorY, cursorX, cursorY + fontSize_, WHITE);
    }
}

void ConsoleSystem::InitializeBuiltInCommands() {
    RegisterCommand("help", [this](const std::vector<std::string>& args) { CmdHelp(args); },
                   "Show available commands");
    RegisterCommand("clear", [this](const std::vector<std::string>& args) { CmdClear(args); },
                   "Clear console log");
    RegisterCommand("echo", [this](const std::vector<std::string>& args) { CmdEcho(args); },
                   "Echo text to console");
    RegisterCommand("list", [this](const std::vector<std::string>& args) { CmdList(args); },
                   "List all registered commands");

    // Phase 2 specific commands
    RegisterCommand("noclip", [this](const std::vector<std::string>& args) { CmdNoClip(args); },
                   "Toggle collision detection for player (1/0)");
    RegisterCommand("render_bounds", [this](const std::vector<std::string>& args) { CmdRenderBounds(args); },
                   "Toggle visualization of collision bounds (1/0)");
}

void ConsoleSystem::CmdNoClip(const std::vector<std::string>& args) {
    if (!playerEntity_) {
        LogError("No player entity available");
        return;
    }

    auto* player = playerEntity_->GetComponent<Player>();
    if (!player) {
        LogError("Player entity has no Player component");
        return;
    }

    bool newState = !player->HasNoClip(); // Toggle by default

    if (!args.empty()) {
        if (args[0] == "1" || args[0] == "true" || args[0] == "on") {
            newState = true;
        } else if (args[0] == "0" || args[0] == "false" || args[0] == "off") {
            newState = false;
        }
    }

    player->SetNoClip(newState);
    LogInfo("Noclip " + std::string(newState ? "enabled" : "disabled"));
}

void ConsoleSystem::CmdRenderBounds(const std::vector<std::string>& args) {
    if (!collisionSystem_) {
        LogError("No collision system available");
        return;
    }

    // Try to cast to CollisionSystem to access debug bounds toggle
    // For now, we'll use a simple approach and assume the system has this method
    bool newState = true; // Default to enabling

    if (!args.empty()) {
        if (args[0] == "1" || args[0] == "true" || args[0] == "on") {
            newState = true;
        } else if (args[0] == "0" || args[0] == "false" || args[0] == "off") {
            newState = false;
        }
    }

    // Note: This would need to be properly implemented by adding a method to CollisionSystem
    // For now, we'll just log the command
    LogInfo("Render bounds " + std::string(newState ? "enabled" : "disabled"));
    LogWarning("Render bounds visualization not yet implemented in CollisionSystem");
}

void ConsoleSystem::CmdHelp(const std::vector<std::string>& args) {
    if (args.empty()) {
        Log("Available commands:");
        for (const auto& pair : commandDescriptions_) {
            Log("  " + pair.first + " - " + pair.second, CYAN);
        }
        Log("Type 'help <command>' for more details");
    } else {
        std::string cmd = args[0];
        auto it = commandDescriptions_.find(cmd);
        if (it != commandDescriptions_.end()) {
            Log(cmd + ": " + it->second, CYAN);
        } else {
            LogError("No help available for: " + cmd);
        }
    }
}

void ConsoleSystem::CmdClear(const std::vector<std::string>& args) {
    logEntries_.clear();
}

void ConsoleSystem::CmdEcho(const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::string message;
        for (const auto& arg : args) {
            if (!message.empty()) message += " ";
            message += arg;
        }
        Log(message, YELLOW);
    }
}

void ConsoleSystem::CmdList(const std::vector<std::string>& args) {
    Log("Registered commands:");
    for (const auto& pair : commands_) {
        std::string desc = commandDescriptions_.count(pair.first) ?
                          " - " + commandDescriptions_[pair.first] : "";
        Log("  " + pair.first + desc, GREEN);
    }
}

std::string ConsoleSystem::TrimWhitespace(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}
