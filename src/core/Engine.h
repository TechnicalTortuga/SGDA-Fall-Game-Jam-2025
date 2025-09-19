#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "utils/Logger.h"

class StateManager;
class EventManager;
class System;
class Entity;

class Engine {
public:
    Engine();
    ~Engine();

    // Core engine methods
    bool Initialize();
    void Update(float deltaTime);
    void Render();  // Now actually renders ECS systems
    void Shutdown();

    // Access to managers
    StateManager* GetStateManager() const { return stateManager_; }

    // Entity management
    Entity* CreateEntity();
    void DestroyEntity(Entity* entity);
    Entity* GetEntityById(uint64_t id) const;
    const std::unordered_map<uint64_t, std::unique_ptr<Entity>>& GetEntities() const { return entities_; }

    // System management
    template<typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        static_assert(std::is_base_of<System, T>::value,
                      "T must inherit from System");

        // Check if system already exists
        for (auto& system : systems_) {
            if (dynamic_cast<T*>(system.get())) {
                LOG_WARNING("System already exists");
                return static_cast<T*>(system.get());
            }
        }

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = system.get();
        rawPtr->SetEngine(this);
        systems_.push_back(std::move(system));

        LOG_DEBUG("Added system to engine");
        return rawPtr;
    }

    template<typename T>
    T* GetSystem() const {
        static_assert(std::is_base_of<System, T>::value,
                      "T must inherit from System");

        for (auto& system : systems_) {
            T* casted = dynamic_cast<T*>(system.get());
            if (casted) {
                return casted;
            }
        }

        return nullptr;
    }

    template<typename T>
    bool HasSystem() const {
        static_assert(std::is_base_of<System, T>::value,
                      "T must inherit from System");

        return GetSystem<T>() != nullptr;
    }

    void RemoveSystem(System* system);

    // Entity-System management
    void UpdateEntityRegistration(Entity* entity);

    // Engine utilities
    void Clear();
    size_t GetEntityCount() const { return entities_.size(); }
    size_t GetSystemCount() const { return systems_.size(); }

    // System access helpers
    template<typename T>
    T* GetSystemByType() {
        for (auto& system : systems_) {
            T* casted = dynamic_cast<T*>(system.get());
            if (casted) return casted;
        }
        return nullptr;
    }

private:
    StateManager* stateManager_;
    EventManager* eventManager_;

    std::unordered_map<uint64_t, std::unique_ptr<Entity>> entities_;
    std::vector<std::unique_ptr<System>> systems_;

    uint64_t nextEntityId_;

    uint64_t GenerateEntityId();
    void InitializeEventManager();
    void InitializeStateManager();
};
