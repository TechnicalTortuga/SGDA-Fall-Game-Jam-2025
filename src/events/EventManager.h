#pragma once

#include "Event.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>

using EventCallback = std::function<void(const Event&)>;

class EventManager {
public:
    EventManager();
    ~EventManager();

    // Subscribe to events
    uint64_t Subscribe(EventType type, EventCallback callback);
    void Unsubscribe(uint64_t subscriptionId);

    // Post events (immediate dispatch)
    void PostEvent(EventType type, std::unique_ptr<EventData> data = nullptr);

    // Queue events (dispatch on next Update)
    void QueueEvent(EventType type, std::unique_ptr<EventData> data = nullptr);

    // Dispatch queued events
    void DispatchEvents();

    // Immediate dispatch of single event
    void DispatchEvent(const Event& event);

    // Clear all subscriptions and queued events
    void Clear();

    // Get statistics
    size_t GetQueuedEventCount() const;
    size_t GetSubscriberCount(EventType type) const;

private:
    struct Subscription {
        uint64_t id;
        EventType type;
        EventCallback callback;
    };

    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::queue<Event> eventQueue_;
    mutable std::mutex queueMutex_;
    uint64_t nextSubscriptionId_;

    uint64_t GenerateSubscriptionId();
};
