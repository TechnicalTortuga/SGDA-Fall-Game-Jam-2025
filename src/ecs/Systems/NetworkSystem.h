#pragma once
#include "../System.h"
#include "../../networking/TransportLayer.h"
#include <unordered_map>

class NetworkSystem : public System {
public:
    NetworkSystem();
    virtual ~NetworkSystem();

    // System overrides
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Mode initialization (called from main/Game based on UI/flags)
    bool InitializeNetwork(NetworkMode mode, uint16_t port = 0);
    void ShutdownNetwork();

    // Game state
    bool IsHost() const { return mode_ == NetworkMode::HOST; }
    bool IsConnected() const { return transport_.GetState() == ConnectionState::CONNECTED; }
    uint32_t GetLocalClientId() const { return transport_.GetLocalClientId(); }

    // Entity networking
    void RegisterNetworkEntity(Entity* entity);
    void UnregisterNetworkEntity(Entity* entity);
    Entity* GetEntityByNetworkId(uint32_t networkId);

    // Authority management
    bool HasAuthorityOver(uint32_t networkId) const;
    void RequestAuthorityTransfer(uint32_t entityNetId, uint32_t newOwnerId);

    // Lobby management
    bool JoinLobby(const std::string& lobbyId);
    bool CreateLobby(const std::string& lobbyName);
    void LeaveLobby();
    bool RequestUDPHandoff(const std::string& lobbyId);

    // Network statistics
    float GetLatency() const { return transport_.GetLatency(); }
    uint32_t GetPacketsSent() const { return transport_.GetPacketsSent(); }
    uint32_t GetPacketsReceived() const { return transport_.GetPacketsReceived(); }

    // Transport layer access
    TransportLayer& GetTransport() { return transport_; }

private:
    TransportLayer transport_;
    NetworkMode mode_ = NetworkMode::CLIENT;

    // Entity management
    std::unordered_map<uint32_t, Entity*> networkIdToEntity_;
    std::unordered_map<Entity*, uint32_t> entityToNetworkId_;
    uint32_t nextNetworkId_ = 1;

    // Network state
    bool isInitialized_ = false;
    float lastUpdateTime_ = 0.0f;
    float updateInterval_ = 1.0f / 60.0f; // 60 Hz updates

    // Message processing
    void ProcessIncomingMessages();
    void ProcessOutgoingMessages();
    void HandleNetworkMessage(const NetworkMessage& message);

    // Entity synchronization
    void SynchronizeEntity(Entity* entity);
    void DeserializeEntityState(uint32_t networkId, const std::vector<uint8_t>& data);
};
