//
// Created by sergio on 9/23/25.
//

#ifndef TRYTWOFORGAMEJAM_TRANSPORTLAYER_H
#define TRYTWOFORGAMEJAM_TRANSPORTLAYER_H
#pragma once
#include <memory>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

enum class NetworkMode {
    CLIENT,          // Pure client
    HOST,            // Integrated server (client + server logic)
    HANDSHAKE_ONLY   // Lightweight handshake bridge
};

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    FAILED
};

struct NetworkMessage {
    uint32_t messageType;
    uint32_t senderId;
    uint32_t targetId;  // 0 for broadcast
    std::vector<uint8_t> payload;
    uint32_t sequenceNumber;
    bool reliable;
    uint64_t timestamp; // For latency measurement
};

class TransportLayer {
public:
    // Mode-aware initialization for TCP WebSocket + UDP Node.js architecture
    bool Initialize(NetworkMode mode, uint16_t port = 0);
    void Shutdown();

    // WebSocket connection for lobby/matchmaking
    bool ConnectToWebSocket(const std::string& websocketUrl);
    void DisconnectFromWebSocket();
    
    // UDP connection for game data (after lobby handoff)
    bool ConnectToUDPNode(const std::string& nodeServer, uint16_t port);
    void DisconnectFromUDPNode();

    // Messaging
    void SendMessage(const NetworkMessage& message);
    void BroadcastMessage(const NetworkMessage& message);
    NetworkMessage ReceiveMessage(); // Non-blocking

    // Connection info
    ConnectionState GetState() const { return state_; }
    uint32_t GetLocalClientId() const { return localClientId_; }
    std::vector<uint32_t> GetConnectedClients() const;

    // Lobby management via WebSocket
    bool JoinLobby(const std::string& lobbyId);
    bool CreateLobby(const std::string& lobbyName);
    void LeaveLobby();
    
    // Handoff from WebSocket to UDP
    bool RequestUDPHandoff(const std::string& lobbyId);

    // Network statistics
    float GetLatency() const { return averageLatency_; }
    uint32_t GetPacketsSent() const { return packetsSent_; }
    uint32_t GetPacketsReceived() const { return packetsReceived_; }

private:
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    NetworkMode mode_ = NetworkMode::CLIENT;
    uint32_t localClientId_ = 0;

    // WebSocket connection for lobby
    void* websocketHandle_ = nullptr; // Will be cast to appropriate WebSocket library
    std::string websocketUrl_;
    bool websocketConnected_ = false;

    // UDP connection for game data
    void* udpSocket_ = nullptr; // Will be cast to appropriate socket library
    std::string udpServerAddress_;
    uint16_t udpServerPort_ = 0;
    bool udpConnected_ = false;

    // Message queues
    std::queue<NetworkMessage> outgoingMessages_;
    std::queue<NetworkMessage> incomingMessages_;

    // Reliability system
    std::unordered_map<uint32_t, NetworkMessage> unacknowledgedMessages_;
    uint32_t nextSequenceNumber_ = 0;

    // Network statistics
    float averageLatency_ = 0.0f;
    uint32_t packetsSent_ = 0;
    uint32_t packetsReceived_ = 0;

    // Internal methods
    bool InitializeWebSocket();
    bool InitializeUDP();
    void ProcessWebSocketMessages();
    void ProcessUDPMessages();
    void UpdateNetworkStats();
};
#endif //TRYTWOFORGAMEJAM_TRANSPORTLAYER_H