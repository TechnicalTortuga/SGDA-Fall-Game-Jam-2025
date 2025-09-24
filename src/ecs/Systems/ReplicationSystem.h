#pragma once
#include "../System.h"
#include "NetworkSystem.h"
#include <unordered_map>
#include <functional>
#include <typeindex>

class ReplicationSystem : public System {
public:
    ReplicationSystem();
    virtual ~ReplicationSystem();

    // System overrides
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Network system integration
    void SetNetworkSystem(NetworkSystem* network) { network_ = network; }

    // Component replication registration
    template<typename T>
    void RegisterComponentForReplication();

    // State synchronization
    void ReplicateEntityState(Entity* entity);
    void ReceiveEntityState(uint32_t networkId, const std::vector<uint8_t>& stateData);

    // Delta compression
    std::vector<uint8_t> CreateDeltaUpdate(const Component& oldState, const Component& newState);
    void ApplyDeltaUpdate(Component& target, const std::vector<uint8_t>& delta);

    // Snapshots for desync recovery
    void SendPeriodicSnapshot(Entity* entity, float interval = 5.0f);

    // Interest management
    void UpdateInterestLists();
    bool ShouldReplicateToClient(Entity* entity, uint32_t clientId) const;

private:
    NetworkSystem* network_ = nullptr;

    // Replication registry
    std::unordered_map<std::type_index, std::function<void(Entity*, std::vector<uint8_t>&)>> serializers_;
    std::unordered_map<std::type_index, std::function<void(Entity*, const std::vector<uint8_t>&)>> deserializers_;

    // Change tracking for delta compression
    std::unordered_map<uint32_t, std::unordered_map<std::type_index, std::vector<uint8_t>>> lastKnownStates_;

    // Snapshot management
    std::unordered_map<uint32_t, float> lastSnapshotTime_;
    float snapshotInterval_ = 5.0f; // 5 seconds between snapshots

    // Interest management
    std::unordered_map<uint32_t, std::vector<uint32_t>> clientInterestLists_; // clientId -> entityIds
    float interestUpdateInterval_ = 1.0f; // Update interest lists every second
    float lastInterestUpdate_ = 0.0f;

    // Helper methods
    void SerializeComponent(const Component& component, std::vector<uint8_t>& data);
    void DeserializeComponent(Component& component, const std::vector<uint8_t>& data);
    bool HasComponentChanged(Entity* entity, const std::type_index& componentType);
    void UpdateLastKnownState(Entity* entity, const std::type_index& componentType);
};
