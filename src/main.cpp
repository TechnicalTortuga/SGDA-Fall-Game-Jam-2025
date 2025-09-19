#include "Game.h"
#include "utils/Logger.h"

int main()
{
    // Initialize logging
    Logger::Init();

    LOG_INFO("Starting PaintSplash v0.1.0");

    // Create and initialize game
    Game game;
    if (!game.Initialize()) {
        LOG_ERROR("Failed to initialize game");
        Logger::Shutdown();
        return 1;
    }

    // Run the game
    game.Run();

    // Shutdown
    game.Shutdown();
    Logger::Shutdown();

    return 0;
}
