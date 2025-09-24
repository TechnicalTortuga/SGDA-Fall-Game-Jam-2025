#pragma once
#include "../Component.h"
#include <vector>
#include <functional>
#include <string>

enum class ReplicationMode {
    ALWAYS,           // Replicate every frame
    ON_CHANGE,        // Only when state changes
    PERIODIC,         // Every N seconds
    NEVER            // Local only
};

struct ReplicationRule {
    std::string propertyName;
    ReplicationMode mode;
    float updateFrequency = 0.0f;  // For PERIODIC mode
    std::function<bool()> shouldReplicate; // Custom condition
};

struct ReplicationComponent : public Component {
    std::vector<ReplicationRule> rules;

    // Bandwidth optimization
    uint32_t maxBandwidthPerSecond = 1024; // Bytes per second
    uint32_t currentBandwidthUsage = 0;

    // Interest management
    float replicationDistance = 50.0f;     // Max distance for replication
    std::vector<uint32_t> interestedClients; // Clients that should receive updates

    // Compression settings
    bool useDeltaCompression = true;
    bool useQuantization = true;           // Reduce precision for bandwidth
    float positionQuantization = 0.01f;    // 1cm precision
    float rotationQuantization = 0.1f;     // ~0.1 degree precision

    virtual const char* GetTypeName() const override { return "ReplicationComponent"; }
};
