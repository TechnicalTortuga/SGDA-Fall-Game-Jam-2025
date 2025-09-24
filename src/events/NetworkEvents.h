#pragma once
#include "Event.h"
#include <string>
#include <vector>
#include <cstdint>

// Network connection events
struct NetworkConnectedEvent : public Event {
    uint32_t clientId;
    std::string clientAddress;
    
    NetworkConnectedEvent(uint32_t id, const std::string& address) 
        : Event(EventType::NETWORK_CONNECT), clientId(id), clientAddress(address) {}
};

struct NetworkDisconnectedEvent : public Event {
    uint32_t clientId;
    std::string reason;
    
    NetworkDisconnectedEvent(uint32_t id, const std::string& disconnectReason) 
        : Event(EventType::NETWORK_DISCONNECT), clientId(id), reason(disconnectReason) {}
};

// Lobby events
struct LobbyJoinedEvent : public Event {
    std::string lobbyId;
    std::string lobbyName;
    std::vector<uint32_t> playerIds;
    
    LobbyJoinedEvent(const std::string& id, const std::string& name, const std::vector<uint32_t>& players)
        : Event(EventType::NETWORK_PLAYER_JOIN), lobbyId(id), lobbyName(name), playerIds(players) {}
};

struct LobbyLeftEvent : public Event {
    std::string lobbyId;
    std::string reason;
    
    LobbyLeftEvent(const std::string& id, const std::string& leaveReason)
        : Event(EventType::NETWORK_PLAYER_LEAVE), lobbyId(id), reason(leaveReason) {}
};

// UDP handoff events
struct UDPHandoffRequestedEvent : public Event {
    std::string lobbyId;
    std::string udpServerAddress;
    uint16_t udpServerPort;
    
    UDPHandoffRequestedEvent(const std::string& id, const std::string& address, uint16_t port)
        : Event(EventType::CUSTOM_START), lobbyId(id), udpServerAddress(address), udpServerPort(port) {}
};

struct UDPHandoffCompletedEvent : public Event {
    std::string lobbyId;
    bool success;
    std::string errorMessage;
    
    UDPHandoffCompletedEvent(const std::string& id, bool handoffSuccess, const std::string& error = "")
        : Event(static_cast<EventType>(static_cast<int>(EventType::CUSTOM_START) + 1)), lobbyId(id), success(handoffSuccess), errorMessage(error) {}
};

// Entity replication events
struct EntityReplicatedEvent : public Event {
    uint32_t networkId;
    uint32_t entityId;
    std::string componentType;
    
    EntityReplicatedEvent(uint32_t netId, uint32_t entId, const std::string& compType)
        : Event(static_cast<EventType>(static_cast<int>(EventType::CUSTOM_START) + 2)), networkId(netId), entityId(entId), componentType(compType) {}
};

struct EntityStateReceivedEvent : public Event {
    uint32_t networkId;
    std::vector<uint8_t> stateData;
    
    EntityStateReceivedEvent(uint32_t netId, const std::vector<uint8_t>& data)
        : Event(static_cast<EventType>(static_cast<int>(EventType::CUSTOM_START) + 3)), networkId(netId), stateData(data) {}
};

// Network error events
struct NetworkErrorEvent : public Event {
    std::string errorType;
    std::string errorMessage;
    uint32_t errorCode;
    
    NetworkErrorEvent(const std::string& type, const std::string& message, uint32_t code = 0)
        : Event(static_cast<EventType>(static_cast<int>(EventType::CUSTOM_START) + 4)), errorType(type), errorMessage(message), errorCode(code) {}
};

// Network statistics events
struct NetworkStatsUpdateEvent : public Event {
    float latency;
    uint32_t packetsSent;
    uint32_t packetsReceived;
    float bandwidthUsage;
    
    NetworkStatsUpdateEvent(float lat, uint32_t sent, uint32_t received, float bandwidth)
        : Event(static_cast<EventType>(static_cast<int>(EventType::CUSTOM_START) + 5)), latency(lat), packetsSent(sent), packetsReceived(received), bandwidthUsage(bandwidth) {}
};
