#include "GameObjectSystem.h"
#include "../Entity.h"
#include "../Components/GameObject.h"
#include "../Components/LightComponent.h"
#include "../Components/EnemyComponent.h"
#include "../Components/TriggerComponent.h"
#include "../Components/SpawnPointComponent.h"
#include "../Components/TransformComponent.h"
#include "../../utils/Logger.h"
#include <algorithm>

GameObjectSystem::GameObjectSystem() {
    LOG_INFO("GameObjectSystem created");
}

void GameObjectSystem::Initialize() {
    LOG_INFO("GameObjectSystem initialized");
    UpdateCaches();
}

void GameObjectSystem::Shutdown() {
    LOG_INFO("GameObjectSystem shutdown");
    gameObjects_.clear();
    gameObjectsByType_.clear();
    gameObjectsByTag_.clear();
    activeLights_.clear();
    activeEnemies_.clear();
    activeTriggers_.clear();
}

void GameObjectSystem::Update(float deltaTime) {
    // Update all active Game Objects
    for (Entity* entity : gameObjects_) {
        if (IsGameObjectActive(entity)) {
            UpdateGameObject(entity, deltaTime);
        }
    }

    // Update specific Game Object types
    UpdateLights(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateTriggers(deltaTime);

    // Update caches periodically (every 60 frames at 60fps = 1 second)
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        UpdateCaches();
    }
}

void GameObjectSystem::Render() {
    // Game Object rendering is handled by other systems (RenderSystem, etc.)
    // This system focuses on behavior and state management
}

void GameObjectSystem::RegisterGameObject(Entity* entity) {
    if (!entity) return;

    GameObject* gameObj = entity->GetComponent<GameObject>();
    if (!gameObj) return;

    // Add to main registry
    if (std::find(gameObjects_.begin(), gameObjects_.end(), entity) == gameObjects_.end()) {
        gameObjects_.push_back(entity);
    }

    // Add to type registry
    gameObjectsByType_[gameObj->type].push_back(entity);

    // Add to tag registries
    ProcessGameObjectTags(entity);

    LOG_INFO("Registered GameObject: " + gameObj->name + " (" + gameObj->className + ")");
}

void GameObjectSystem::UnregisterGameObject(Entity* entity) {
    if (!entity) return;

    // Remove from main registry
    auto it = std::find(gameObjects_.begin(), gameObjects_.end(), entity);
    if (it != gameObjects_.end()) {
        gameObjects_.erase(it);
    }

    // Remove from type registries
    GameObject* gameObj = entity->GetComponent<GameObject>();
    if (gameObj) {
        auto& typeList = gameObjectsByType_[gameObj->type];
        auto typeIt = std::find(typeList.begin(), typeList.end(), entity);
        if (typeIt != typeList.end()) {
            typeList.erase(typeIt);
        }

        // Remove from tag registries
        for (const std::string& tag : gameObj->tags) {
            auto& tagList = gameObjectsByTag_[tag];
            auto tagIt = std::find(tagList.begin(), tagList.end(), entity);
            if (tagIt != tagList.end()) {
                tagList.erase(tagIt);
            }
        }
    }

    LOG_INFO("Unregistered GameObject");
}

void GameObjectSystem::UpdateGameObject(Entity* entity, float deltaTime) {
    if (!entity) return;

    GameObject* gameObj = entity->GetComponent<GameObject>();
    if (!gameObj) return;

    // Handle rotation for entities with rotation_speed property
    auto rotationSpeedIt = gameObj->properties.find("rotation_speed");
    if (rotationSpeedIt != gameObj->properties.end()) {
        try {
            float rotationSpeed = std::any_cast<float>(rotationSpeedIt->second);
            auto transform = entity->GetComponent<TransformComponent>();
            if (transform) {
                // Rotate around Y axis
                float rotationDelta = rotationSpeed * deltaTime * DEG2RAD;
                Quaternion rotationQuat = QuaternionFromAxisAngle((Vector3){0, 1, 0}, rotationDelta);
                transform->rotation = QuaternionMultiply(transform->rotation, rotationQuat);
                transform->needsMatrixUpdate = true;
            }
        } catch (const std::bad_any_cast&) {
            // Invalid rotation_speed type, ignore
        }
    }

    // Type-specific updates are handled in dedicated methods
}

std::vector<Entity*> GameObjectSystem::GetGameObjectsByType(GameObjectType type) const {
    auto it = gameObjectsByType_.find(type);
    if (it != gameObjectsByType_.end()) {
        return it->second;
    }
    return {};
}

std::vector<Entity*> GameObjectSystem::GetGameObjectsByTag(const std::string& tag) const {
    auto it = gameObjectsByTag_.find(tag);
    if (it != gameObjectsByTag_.end()) {
        return it->second;
    }
    return {};
}

std::vector<Entity*> GameObjectSystem::GetActiveGameObjects() const {
    std::vector<Entity*> active;
    for (Entity* entity : gameObjects_) {
        if (IsGameObjectActive(entity)) {
            active.push_back(entity);
        }
    }
    return active;
}

void GameObjectSystem::UpdateLights(float deltaTime) {
    for (Entity* entity : activeLights_) {
        // Light-specific updates (flickering, animation, etc.)
        // For now, lights are static, but this could be extended
        LightComponent* light = entity->GetComponent<LightComponent>();
        if (light) {
            // Placeholder for light animation logic
        }
    }
}

std::vector<Entity*> GameObjectSystem::GetActiveLights() const {
    return activeLights_;
}

void GameObjectSystem::UpdateEnemies(float deltaTime) {
    for (Entity* entity : activeEnemies_) {
        EnemyComponent* enemy = entity->GetComponent<EnemyComponent>();
        if (enemy) {
            // Basic enemy AI logic placeholder
            // In a full implementation, this would handle:
            // - Pathfinding to targets/waypoints
            // - Attack behavior
            // - Health/damage processing
            // - State transitions

            // For now, just ensure enemy stays "alive"
            if (enemy->health <= 0) {
                enemy->state = EnemyState::DEAD;
            }
        }
    }
}

std::vector<Entity*> GameObjectSystem::GetActiveEnemies() const {
    return activeEnemies_;
}

void GameObjectSystem::UpdateTriggers(float deltaTime) {
    for (Entity* entity : activeTriggers_) {
        TriggerComponent* trigger = entity->GetComponent<TriggerComponent>();
        if (trigger) {
            // Trigger logic would go here
            // - Check for entities entering/exiting trigger volume
            // - Fire trigger events
            // - Handle trigger state changes

            // Placeholder for trigger update logic
        }
    }
}

std::vector<Entity*> GameObjectSystem::GetActiveTriggers() const {
    return activeTriggers_;
}

std::vector<Entity*> GameObjectSystem::GetSpawnPoints(int team) const {
    std::vector<Entity*> spawnPoints;

    for (Entity* entity : gameObjects_) {
        if (!IsGameObjectActive(entity)) continue;

        GameObject* gameObj = entity->GetComponent<GameObject>();
        SpawnPointComponent* spawnComp = entity->GetComponent<SpawnPointComponent>();

        if (gameObj && gameObj->type == GameObjectType::SPAWN_POINT && spawnComp) {
            if (team == -1 || spawnComp->team == team) {
                spawnPoints.push_back(entity);
            }
        }
    }

    return spawnPoints;
}

Entity* GameObjectSystem::FindBestSpawnPoint(int team) const {
    auto spawnPoints = GetSpawnPoints(team);

    if (spawnPoints.empty()) return nullptr;

    // For now, return the first available spawn point
    // In a full implementation, this would consider:
    // - Distance from other players
    // - Safety (not in combat zones)
    // - Team balance
    // - Spawn point priority/cooldown

    for (Entity* spawnPoint : spawnPoints) {
        SpawnPointComponent* spawnComp = spawnPoint->GetComponent<SpawnPointComponent>();
        if (spawnComp && !spawnComp->occupied) {
            return spawnPoint;
        }
    }

    // If all are occupied, return the first one
    return spawnPoints[0];
}

void GameObjectSystem::OnGameObjectEvent(Entity* entity, const std::string& eventType) {
    if (!entity) return;

    GameObject* gameObj = entity->GetComponent<GameObject>();
    if (!gameObj) return;

    LOG_INFO("GameObject event: " + gameObj->name + " - " + eventType);

    // Handle different event types
    if (eventType == "activated") {
        // Handle activation events
    } else if (eventType == "deactivated") {
        // Handle deactivation events
    } else if (eventType == "destroyed") {
        // Handle destruction events
        UnregisterGameObject(entity);
    }

    // Type-specific event handling could go here
}

void GameObjectSystem::UpdateCaches() {
    // Update active lights cache
    activeLights_.clear();
    auto lights = GetGameObjectsByType(GameObjectType::LIGHT_POINT);
    auto spotLights = GetGameObjectsByType(GameObjectType::LIGHT_SPOT);
    auto dirLights = GetGameObjectsByType(GameObjectType::LIGHT_DIRECTIONAL);

    for (auto& light : lights) {
        if (IsGameObjectActive(light)) activeLights_.push_back(light);
    }
    for (auto& light : spotLights) {
        if (IsGameObjectActive(light)) activeLights_.push_back(light);
    }
    for (auto& light : dirLights) {
        if (IsGameObjectActive(light)) activeLights_.push_back(light);
    }

    // Update active enemies cache
    activeEnemies_.clear();
    auto enemies = GetGameObjectsByType(GameObjectType::ENEMY);
    for (auto& enemy : enemies) {
        if (IsGameObjectActive(enemy)) activeEnemies_.push_back(enemy);
    }

    // Update active triggers cache
    activeTriggers_.clear();
    auto triggers = GetGameObjectsByType(GameObjectType::TRIGGER);
    for (auto& trigger : triggers) {
        if (IsGameObjectActive(trigger)) activeTriggers_.push_back(trigger);
    }
}

bool GameObjectSystem::IsGameObjectActive(Entity* entity) const {
    if (!entity || !entity->IsActive()) return false;

    GameObject* gameObj = entity->GetComponent<GameObject>();
    return gameObj && gameObj->enabled;
}

void GameObjectSystem::ProcessGameObjectTags(Entity* entity) {
    GameObject* gameObj = entity->GetComponent<GameObject>();
    if (!gameObj) return;

    for (const std::string& tag : gameObj->tags) {
        gameObjectsByTag_[tag].push_back(entity);
    }
}
