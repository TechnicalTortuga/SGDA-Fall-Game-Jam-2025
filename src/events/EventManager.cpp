#include "EventManager.h"
#include "utils/Logger.h"
#include <chrono>

EventManager::EventManager()
    : nextSubscriptionId_(1)
{
    LOG_INFO("EventManager initialized");
}

EventManager::~EventManager()
{
    Clear();
    LOG_INFO("EventManager destroyed");
}

uint64_t EventManager::Subscribe(EventType type, EventCallback callback)
{
    uint64_t subscriptionId = GenerateSubscriptionId();

    Subscription subscription;
    subscription.id = subscriptionId;
    subscription.type = type;
    subscription.callback = std::move(callback);

    subscribers_[type].push_back(std::move(subscription));

    LOG_DEBUG("Subscribed to event type " + std::to_string(static_cast<int>(type)) +
              " with ID " + std::to_string(subscriptionId));

    return subscriptionId;
}

void EventManager::Unsubscribe(uint64_t subscriptionId)
{
    for (auto& [type, subscriptions] : subscribers_) {
        auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
            [subscriptionId](const Subscription& sub) {
                return sub.id == subscriptionId;
            });

        if (it != subscriptions.end()) {
            subscriptions.erase(it);
            LOG_DEBUG("Unsubscribed event with ID " + std::to_string(subscriptionId));
            return;
        }
    }

    LOG_WARNING("Attempted to unsubscribe non-existent subscription ID: " +
                std::to_string(subscriptionId));
}

void EventManager::PostEvent(EventType type, std::unique_ptr<EventData> data)
{
    Event event(type, std::move(data));
    DispatchEvent(event);
}

void EventManager::QueueEvent(EventType type, std::unique_ptr<EventData> data)
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    eventQueue_.emplace(type, std::move(data));
}

void EventManager::DispatchEvents()
{
    std::queue<Event> eventsToDispatch;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventsToDispatch.push(std::move(eventQueue_.front()));
            eventQueue_.pop();
        }
    }

    while (!eventsToDispatch.empty()) {
        DispatchEvent(eventsToDispatch.front());
        eventsToDispatch.pop();
    }
}

void EventManager::DispatchEvent(const Event& event)
{
    auto it = subscribers_.find(event.type);
    if (it == subscribers_.end()) {
        return; // No subscribers for this event type
    }

    for (const auto& subscription : it->second) {
        try {
            subscription.callback(event);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in event callback for type " +
                      std::to_string(static_cast<int>(event.type)) + ": " + e.what());
        }
    }
}

void EventManager::Clear()
{
    subscribers_.clear();

    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<Event> empty;
    std::swap(eventQueue_, empty);

    LOG_INFO("EventManager cleared");
}

size_t EventManager::GetQueuedEventCount() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    return eventQueue_.size();
}

size_t EventManager::GetSubscriberCount(EventType type) const
{
    auto it = subscribers_.find(type);
    return (it != subscribers_.end()) ? it->second.size() : 0;
}

uint64_t EventManager::GenerateSubscriptionId()
{
    return nextSubscriptionId_++;
}

// Event implementation
Event::Event(EventType t, std::unique_ptr<EventData> d)
    : type(t), data(std::move(d))
{
    using namespace std::chrono;
    timestamp = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

Event::Event(const Event& other)
    : type(other.type), timestamp(other.timestamp)
{
    if (other.data) {
        // Note: This requires copy constructors for EventData subclasses
        // For now, we'll do a shallow copy - proper implementation would
        // need virtual clone() methods in EventData
        data = std::unique_ptr<EventData>(other.data.get());
    }
}

Event& Event::operator=(const Event& other)
{
    if (this != &other) {
        type = other.type;
        timestamp = other.timestamp;
        if (other.data) {
            data = std::unique_ptr<EventData>(other.data.get());
        } else {
            data = nullptr;
        }
    }
    return *this;
}
