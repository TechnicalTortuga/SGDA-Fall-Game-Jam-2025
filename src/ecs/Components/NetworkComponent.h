#pragma once
#include "../Component.h"
#include <cstdint>

enum class NetworkAuthority {
    SERVER_AUTHORITATIVE,    // Server has final say (paint scores, health)
    CLIENT_PREDICTED,        // Client predicts, server corrects (movement)
    OWNER_AUTHORITATIVE      // Owning client has authority (local effects)
};

struct NetworkComponent : public Component {
    uint32_t networkId = 0;           // Unique across network (server assigned)
    uint32_t ownerClientId = 0;       // Client that owns this entity
    NetworkAuthority authority;       // Authority model for this component

    // Replication state
    float lastReplicationTime = 0.0f; // Last time state was sent
    uint32_t replicationPriority = 1; // 0=never, 1=normal, 2=high, 3=critical

    // Interpolation data (for client-side prediction)
    bool useInterpolation = false;
    float interpolationSpeed = 10.0f; // Units per second

    // Dirty flags for delta compression
    bool positionDirty = false;
    bool rotationDirty = false;
    bool stateDirty = false;

    // Network validation
    uint32_t lastValidatedTick = 0;   // Server validation timestamp

    virtual const char* GetTypeName() const override { return "NetworkComponent"; }
};
