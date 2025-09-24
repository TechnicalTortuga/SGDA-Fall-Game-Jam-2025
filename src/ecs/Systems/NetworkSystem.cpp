#include "NetworkSystem.h"
#include "../../networking/TransportLayer.h"
#include "../Entity.h"
#include "../Components/NetworkComponent.h"
#include "../../events/NetworkEvents.h"
#include "../../events/EventManager.h"
#include "../../utils/Logger.h"
#include "raylib.h"

NetworkSystem::NetworkSystem() {
    SetSignature<NetworkComponent>();
    LOG_INFO("NetworkSystem created");
}

NetworkSystem::~NetworkSystem() {
    ShutdownNetwork();
    LOG_INFO("NetworkSystem destroyed");
}

void NetworkSystem::Initialize() {
    LOG_INFO("NetworkSystem initialized");
}

void NetworkSystem::Update(float deltaTime) {
    if (!isInitialized_) {
        return;
    }

    // Process incoming messages
    ProcessIncomingMessages();
    
    // Process outgoing messages
    ProcessOutgoingMessages();
    
    // Update network statistics
    lastUpdateTime_ += deltaTime;
    if (lastUpdateTime_ >= updateInterval_) {
        // Send periodic updates for entities that need replication
        for (auto* entity : entities_) {
            if (HasAuthorityOver(entity->GetComponent<NetworkComponent>()->networkId)) {
                SynchronizeEntity(entity);
            }
        }
        lastUpdateTime_ = 0.0f;
    }
}

void NetworkSystem::Shutdown() {
    ShutdownNetwork();
    LOG_INFO("NetworkSystem shutdown");
}

bool NetworkSystem::InitializeNetwork(NetworkMode mode, uint16_t port) {
    if (isInitialized_) {
        LOG_WARNING("Network already initialized");
        return true;
    }

    mode_ = mode;
    
    if (!transport_.Initialize(mode, port)) {
        LOG_ERROR("Failed to initialize transport layer");
        return false;
    }

    isInitialized_ = true;
    LOG_INFO("Network initialized successfully in mode: " + std::to_string(static_cast<int>(mode)));
    return true;
}

void NetworkSystem::ShutdownNetwork() {
    if (!isInitialized_) {
        return;
    }

    transport_.Shutdown();
    isInitialized_ = false;
    LOG_INFO("Network shutdown completed");
}

void NetworkSystem::RegisterNetworkEntity(Entity* entity) {
    if (!entity) {
        LOG_WARNING("Attempted to register null entity");
        return;
    }

    auto* networkComp = entity->GetComponent<NetworkComponent>();
    if (!networkComp) {
        LOG_WARNING("Entity does not have NetworkComponent");
        return;
    }

    // Assign network ID if not already assigned
    if (networkComp->networkId == 0) {
        networkComp->networkId = nextNetworkId_++;
    }

    networkIdToEntity_[networkComp->networkId] = entity;
    entityToNetworkId_[entity] = networkComp->networkId;

    LOG_INFO("Registered network entity with ID: " + std::to_string(networkComp->networkId));
}

void NetworkSystem::UnregisterNetworkEntity(Entity* entity) {
    if (!entity) {
        return;
    }

    auto it = entityToNetworkId_.find(entity);
    if (it != entityToNetworkId_.end()) {
        uint32_t networkId = it->second;
        networkIdToEntity_.erase(networkId);
        entityToNetworkId_.erase(it);
        LOG_INFO("Unregistered network entity with ID: " + std::to_string(networkId));
    }
}

Entity* NetworkSystem::GetEntityByNetworkId(uint32_t networkId) {
    auto it = networkIdToEntity_.find(networkId);
    return (it != networkIdToEntity_.end()) ? it->second : nullptr;
}

bool NetworkSystem::HasAuthorityOver(uint32_t networkId) const {
    if (mode_ == NetworkMode::HOST) {
        return true; // Host has authority over everything
    }
    
    auto it = networkIdToEntity_.find(networkId);
    if (it != networkIdToEntity_.end()) {
        auto* networkComp = it->second->GetComponent<NetworkComponent>();
        return networkComp && networkComp->ownerClientId == transport_.GetLocalClientId();
    }
    
    return false;
}

void NetworkSystem::RequestAuthorityTransfer(uint32_t entityNetId, uint32_t newOwnerId) {
    // Implementation for authority transfer
    LOG_INFO("Requesting authority transfer for entity " + std::to_string(entityNetId) + " to client " + std::to_string(newOwnerId));
}

bool NetworkSystem::JoinLobby(const std::string& lobbyId) {
    if (!isInitialized_) {
        LOG_ERROR("Network not initialized");
        return false;
    }

    return transport_.JoinLobby(lobbyId);
}

bool NetworkSystem::CreateLobby(const std::string& lobbyName) {
    if (!isInitialized_) {
        LOG_ERROR("Network not initialized");
        return false;
    }

    return transport_.CreateLobby(lobbyName);
}

void NetworkSystem::LeaveLobby() {
    if (isInitialized_) {
        transport_.LeaveLobby();
    }
}

bool NetworkSystem::RequestUDPHandoff(const std::string& lobbyId) {
    if (!isInitialized_) {
        LOG_ERROR("Network not initialized");
        return false;
    }

    return transport_.RequestUDPHandoff(lobbyId);
}

void NetworkSystem::ProcessIncomingMessages() {
    while (true) {
        NetworkMessage message = transport_.ReceiveMessage();
        if (message.messageType == 0) { // No more messages
            break;
        }
        
        HandleNetworkMessage(message);
    }
}

void NetworkSystem::ProcessOutgoingMessages() {
    // Process any queued outgoing messages
    // This would typically be handled by the transport layer
}

void NetworkSystem::HandleNetworkMessage(const NetworkMessage& message) {
    // Handle different message types
    switch (message.messageType) {
        case 1: // Entity state update
            DeserializeEntityState(message.senderId, message.payload);
            break;
        case 2: // Lobby event
            // Handle lobby events
            break;
        case 3: // UDP handoff
            // Handle UDP handoff
            break;
        default:
            LOG_WARNING("Unknown message type: " + std::to_string(message.messageType));
            break;
    }
}

void NetworkSystem::SynchronizeEntity(Entity* entity) {
    if (!entity) {
        return;
    }

    auto* networkComp = entity->GetComponent<NetworkComponent>();
    if (!networkComp) {
        return;
    }

    // Create message with entity state
    NetworkMessage message;
    message.messageType = 1; // Entity state update
    message.senderId = transport_.GetLocalClientId();
    message.targetId = 0; // Broadcast
    message.reliable = true;
    message.sequenceNumber = transport_.GetPacketsSent() + 1;
    message.timestamp = GetTime() * 1000; // Convert to milliseconds

    // Serialize entity state (simplified)
    // In a real implementation, this would serialize all relevant components
    message.payload.push_back(static_cast<uint8_t>(networkComp->networkId));
    message.payload.push_back(static_cast<uint8_t>(networkComp->ownerClientId));

    transport_.SendMessage(message);
}

void NetworkSystem::DeserializeEntityState(uint32_t networkId, const std::vector<uint8_t>& data) {
    Entity* entity = GetEntityByNetworkId(networkId);
    if (!entity) {
        LOG_WARNING("Received state for unknown entity: " + std::to_string(networkId));
        return;
    }

    // Deserialize entity state (simplified)
    // In a real implementation, this would deserialize all relevant components
    if (data.size() >= 2) {
        auto* networkComp = entity->GetComponent<NetworkComponent>();
        if (networkComp) {
            // Update network component state
            networkComp->lastReplicationTime = GetTime();
        }
    }
}
