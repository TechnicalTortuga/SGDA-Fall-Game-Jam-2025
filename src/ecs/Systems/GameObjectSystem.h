#pragma once

#include "../System.h"
#include "../Components/GameObject.h"
#include <unordered_map>
#include <vector>

/*
GameObjectSystem - Manages dynamic Game Object behavior and lifecycle

The GameObjectSystem is responsible for coordinating all Game Object behavior,
including lights, enemies, triggers, spawn points, and other dynamic entities.
It handles Game Object interactions, state management, and event processing.

This system acts as a coordinator for all Game Object subsystems.
*/

class GameObjectSystem : public System {
public:
    GameObjectSystem();
    ~GameObjectSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Render() override;
    void Initialize() override;
    void Shutdown() override;

    // Game Object management
    void RegisterGameObject(Entity* entity);
    void UnregisterGameObject(Entity* entity);
    void UpdateGameObject(Entity* entity, float deltaTime);

    // Game Object queries
    std::vector<Entity*> GetGameObjectsByType(GameObjectType type) const;
    std::vector<Entity*> GetGameObjectsByTag(const std::string& tag) const;
    std::vector<Entity*> GetActiveGameObjects() const;

    // Light management
    void UpdateLights(float deltaTime);
    std::vector<Entity*> GetActiveLights() const;

    // Enemy management
    void UpdateEnemies(float deltaTime);
    std::vector<Entity*> GetActiveEnemies() const;

    // Trigger management
    void UpdateTriggers(float deltaTime);
    std::vector<Entity*> GetActiveTriggers() const;

    // Spawn point management
    std::vector<Entity*> GetSpawnPoints(int team = -1) const; // -1 = all teams
    Entity* FindBestSpawnPoint(int team = 0) const;

    // Event handling
    void OnGameObjectEvent(Entity* entity, const std::string& eventType);

private:
    // Game Object registry
    std::vector<Entity*> gameObjects_;
    std::unordered_map<GameObjectType, std::vector<Entity*>> gameObjectsByType_;
    std::unordered_map<std::string, std::vector<Entity*>> gameObjectsByTag_;

    // Cached collections for performance
    std::vector<Entity*> activeLights_;
    std::vector<Entity*> activeEnemies_;
    std::vector<Entity*> activeTriggers_;

    // Helper methods
    void UpdateCaches();
    bool IsGameObjectActive(Entity* entity) const;
    void ProcessGameObjectTags(Entity* entity);
};
