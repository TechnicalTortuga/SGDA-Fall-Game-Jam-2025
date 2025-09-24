#include "Game.h"
#include "utils/Logger.h"
#include "networking/TransportLayer.h"

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

    // Example: Initialize networking (uncomment to enable)
    // For now, we'll run in offline mode
    // To enable networking, uncomment the following lines:
    /*
    if (game.GetEngine()->InitializeNetwork(NetworkMode::CLIENT)) {
        LOG_INFO("Networking initialized successfully");
        
        // Example: Join a lobby
        // game.GetEngine()->JoinLobby("test-lobby-123");
        
        // Example: Create a lobby (for host mode)
        // game.GetEngine()->InitializeNetwork(NetworkMode::HOST, 7777);
        // game.GetEngine()->CreateLobby("My Paint Wars Lobby");
    } else {
        LOG_WARNING("Failed to initialize networking - running in offline mode");
    }
    */

    // Run the game
    game.Run();

    // Shutdown networking
    game.GetEngine()->ShutdownNetwork();

    // Shutdown
    game.Shutdown();
    Logger::Shutdown();

    return 0;
}
