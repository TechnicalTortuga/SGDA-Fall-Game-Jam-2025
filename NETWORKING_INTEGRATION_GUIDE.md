# 🌐 Networking Integration Guide

This guide explains how to use the integrated networking system in PaintSplash, which supports TCP WebSocket for lobby management and UDP Node.js server for game data.

## 📋 Overview

The networking system has been integrated into the existing ECS architecture and provides:

- **TCP WebSocket**: Lobby management, matchmaking, and initial connection
- **UDP Node.js Server**: High-performance game data transmission
- **ECS Integration**: Network components and systems that work seamlessly with existing game systems
- **Client/Host Modes**: Single executable that can run as client or host

## 🏗️ Architecture

### Network Components

#### NetworkComponent
```cpp
struct NetworkComponent : public Component {
    uint32_t networkId = 0;           // Unique across network
    uint32_t ownerClientId = 0;       // Client that owns this entity
    NetworkAuthority authority;       // Authority model
    float lastReplicationTime = 0.0f; // Last sync time
    uint32_t replicationPriority = 1; // Update priority
    bool useInterpolation = false;    // Client-side prediction
    // ... more fields
};
```

#### InterpolatedTransformComponent
```cpp
struct InterpolatedTransformComponent : public Component {
    Vector3 position = {0, 0, 0};
    Vector3 targetPosition = {0, 0, 0};
    Vector3 velocity = {0, 0, 0};
    float interpolationTime = 0.0f;
    bool isInterpolating = false;
    // ... more fields for prediction and correction
};
```

#### ReplicationComponent
```cpp
struct ReplicationComponent : public Component {
    std::vector<ReplicationRule> rules;
    uint32_t maxBandwidthPerSecond = 1024;
    float replicationDistance = 50.0f;
    bool useDeltaCompression = true;
    // ... more fields for optimization
};
```

### Network Systems

#### NetworkSystem
- Manages network connections and entity registration
- Handles lobby management via WebSocket
- Coordinates UDP handoff to Node.js server
- Provides authority management

#### ReplicationSystem
- Handles entity state synchronization
- Manages delta compression and bandwidth optimization
- Implements interest management (distance-based culling)
- Provides periodic snapshots for desync recovery

#### InterpolationSystem
- Client-side prediction and interpolation
- Server reconciliation and error correction
- Smooth movement transitions

## 🚀 Usage Examples

### 1. Basic Network Initialization

```cpp
#include "Game.h"
#include "networking/TransportLayer.h"

int main() {
    Game game;
    game.Initialize();
    
    // Initialize as client
    if (game.GetEngine()->InitializeNetwork(NetworkMode::CLIENT)) {
        LOG_INFO("Client networking initialized");
    }
    
    // Initialize as host
    if (game.GetEngine()->InitializeNetwork(NetworkMode::HOST, 7777)) {
        LOG_INFO("Host networking initialized on port 7777");
    }
    
    game.Run();
    game.GetEngine()->ShutdownNetwork();
    return 0;
}
```

### 2. Lobby Management

```cpp
// Join an existing lobby
if (game.GetEngine()->JoinLobby("lobby-abc123")) {
    LOG_INFO("Joined lobby successfully");
}

// Create a new lobby
if (game.GetEngine()->CreateLobby("My Paint Wars Game")) {
    LOG_INFO("Created lobby successfully");
}

// Request UDP handoff after lobby is ready
if (game.GetEngine()->RequestUDPHandoff("lobby-abc123")) {
    LOG_INFO("UDP handoff requested");
}
```

### 3. Making Entities Network-Aware

```cpp
// Create a networked player entity
Entity* player = engine->CreateEntity();

// Add network component
auto* networkComp = player->AddComponent<NetworkComponent>();
networkComp->authority = NetworkAuthority::CLIENT_PREDICTED;
networkComp->replicationPriority = 2; // High priority

// Add interpolated transform for smooth movement
auto* interpTransform = player->AddComponent<InterpolatedTransformComponent>();
interpTransform->useInterpolation = true;
interpTransform->interpolationSpeed = 10.0f;

// Add replication rules
auto* replicationComp = player->AddComponent<ReplicationComponent>();
ReplicationRule positionRule;
positionRule.propertyName = "position";
positionRule.mode = ReplicationMode::ON_CHANGE;
replicationComp->rules.push_back(positionRule);

// Register with network system
auto* networkSystem = engine->GetSystem<NetworkSystem>();
networkSystem->RegisterNetworkEntity(player);
```

### 4. Network Event Handling

```cpp
// Subscribe to network events
auto* eventManager = engine->GetEventManager();

eventManager->Subscribe<NetworkConnectedEvent>([](const NetworkConnectedEvent& event) {
    LOG_INFO("Client " + std::to_string(event.clientId) + " connected from " + event.clientAddress);
});

eventManager->Subscribe<LobbyJoinedEvent>([](const LobbyJoinedEvent& event) {
    LOG_INFO("Joined lobby: " + event.lobbyName + " with " + std::to_string(event.playerIds.size()) + " players");
});

eventManager->Subscribe<UDPHandoffCompletedEvent>([](const UDPHandoffCompletedEvent& event) {
    if (event.success) {
        LOG_INFO("UDP handoff completed for lobby: " + event.lobbyId);
    } else {
        LOG_ERROR("UDP handoff failed: " + event.errorMessage);
    }
});
```

### 5. Network Statistics

```cpp
// Monitor network performance
float latency = engine->GetNetworkLatency();
uint32_t packetsSent = engine->GetPacketsSent();
uint32_t packetsReceived = engine->GetPacketsReceived();

LOG_INFO("Network Stats - Latency: " + std::to_string(latency) + "ms, " +
         "Sent: " + std::to_string(packetsSent) + ", " +
         "Received: " + std::to_string(packetsReceived));
```

## 🔧 Configuration

### Network Settings

The networking system can be configured through the Engine API:

```cpp
// Set network mode
NetworkMode mode = NetworkMode::CLIENT; // or HOST, HANDSHAKE_ONLY

// Initialize with specific port (for host mode)
engine->InitializeNetwork(NetworkMode::HOST, 7777);

// WebSocket URL for lobby server
// This would typically be configured in a config file
std::string websocketUrl = "ws://your-lobby-server.com:8080";

// UDP Node.js server address
std::string udpServer = "your-game-server.com";
uint16_t udpPort = 7777;
```

### Component Configuration

```cpp
// Configure replication settings
auto* replicationComp = entity->GetComponent<ReplicationComponent>();
replicationComp->maxBandwidthPerSecond = 2048; // 2KB/s
replicationComp->replicationDistance = 100.0f; // 100 units
replicationComp->useDeltaCompression = true;
replicationComp->useQuantization = true;

// Configure interpolation settings
auto* interpComp = entity->GetComponent<InterpolatedTransformComponent>();
interpComp->interpolationSpeed = 15.0f; // Faster interpolation
interpComp->positionErrorThreshold = 0.05f; // 5cm threshold
```

## 🎮 Game-Specific Integration

### Paint System Integration

For the paint system, you'll want to create paint-specific network components:

```cpp
// Paint replication component (example)
struct PaintReplicationComponent : public Component {
    std::vector<Vector3> paintPositions;
    std::vector<Color> paintColors;
    std::vector<float> paintSizes;
    uint32_t owningTeamId = 0;
    float coverageArea = 0.0f;
    bool serverValidated = false;
};
```

### Team System Integration

```cpp
// Team network events
struct TeamScoreUpdateEvent : public Event {
    uint32_t teamId;
    uint32_t newScore;
    float territoryPercentage;
};

// Subscribe to team updates
eventManager->Subscribe<TeamScoreUpdateEvent>([](const TeamScoreUpdateEvent& event) {
    // Update UI with new team score
    UpdateTeamScoreboard(event.teamId, event.newScore, event.territoryPercentage);
});
```

## 🚨 Error Handling

```cpp
// Network error handling
eventManager->Subscribe<NetworkErrorEvent>([](const NetworkErrorEvent& event) {
    LOG_ERROR("Network Error [" + event.errorType + "]: " + event.errorMessage);
    
    if (event.errorType == "CONNECTION_LOST") {
        // Attempt reconnection
        engine->ShutdownNetwork();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        engine->InitializeNetwork(NetworkMode::CLIENT);
    }
});
```

## 📊 Performance Considerations

### Bandwidth Optimization

- Use delta compression for frequently updated entities
- Implement interest management to reduce unnecessary updates
- Quantize position/rotation data to reduce packet size
- Use different update frequencies for different entity types

### Latency Optimization

- Enable client-side prediction for player movement
- Use interpolation for smooth visual updates
- Implement server reconciliation for authoritative state
- Batch multiple updates into single packets when possible

## 🔍 Debugging

### Network Debug Information

```cpp
// Enable network debugging
auto* networkSystem = engine->GetSystem<NetworkSystem>();
if (networkSystem) {
    LOG_INFO("Network Mode: " + std::to_string(static_cast<int>(networkSystem->GetMode())));
    LOG_INFO("Connected: " + std::string(networkSystem->IsConnected() ? "Yes" : "No"));
    LOG_INFO("Local Client ID: " + std::to_string(networkSystem->GetLocalClientId()));
}
```

### Entity Network Status

```cpp
// Check entity network status
for (auto& entity : engine->GetEntities()) {
    auto* networkComp = entity.second->GetComponent<NetworkComponent>();
    if (networkComp) {
        LOG_INFO("Entity " + std::to_string(entity.first) + 
                " - Network ID: " + std::to_string(networkComp->networkId) +
                " - Owner: " + std::to_string(networkComp->ownerClientId));
    }
}
```

## 🚀 Next Steps

1. **Implement Transport Layer**: Complete the WebSocket and UDP socket implementations
2. **Add Paint-Specific Components**: Create components for paint replication
3. **Implement Team System**: Add team management and scoring
4. **Add Security**: Implement anti-cheat and validation systems
5. **Performance Testing**: Test with multiple clients and optimize bandwidth usage

## 📚 Related Files

- `src/networking/TransportLayer.h` - Core transport layer interface
- `src/ecs/Components/NetworkComponent.h` - Network component definitions
- `src/ecs/Systems/NetworkSystem.h` - Network system implementation
- `src/events/NetworkEvents.h` - Network event definitions
- `src/core/Engine.h` - Engine networking API

This integration provides a solid foundation for multiplayer functionality while maintaining compatibility with the existing single-player game architecture.
