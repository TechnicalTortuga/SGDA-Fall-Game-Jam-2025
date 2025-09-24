#pragma once

#include "MapLoader.h"
#include "../ecs/Entity.h"
#include <memory>
#include <unordered_map>
#include <vector>

class Engine; // Forward declaration

/*
EntityFactory - Creates ECS entities from map entity definitions

The EntityFactory is responsible for converting parsed EntityDefinition objects
from map files into actual ECS entities with appropriate components. It handles
the instantiation of different entity types (lights, spawn points, enemies, etc.)
with their proper component configurations.

This decouples map parsing from entity creation, allowing for different
entity creation strategies and easier testing.
*/

class EntityFactory {
public:
    EntityFactory();
    ~EntityFactory() = default;

    // Get engine instance (singleton)
    Engine& GetEngine() const;

    // Entity creation from map definitions
    Entity* CreateEntityFromDefinition(const EntityDefinition& definition);

    // Bulk entity creation
    std::vector<Entity*> CreateEntitiesFromDefinitions(const std::vector<std::unique_ptr<EntityDefinition>>& definitions);

    // Entity type registration (for extensibility)
    void RegisterEntityCreator(GameObjectType type,
                              std::function<Entity*(const EntityDefinition&)> creator);

    // Material management for entity creation
    void SetMaterials(const std::vector<MaterialInfo>& materials);
    const MaterialInfo* GetMaterialById(int materialId) const;

    // Utility methods

private:
    // Map materials for entity creation (set from WorldSystem)
    std::unordered_map<int, MaterialInfo> materialsMap_;
    // Entity creation methods for different types
    Entity* CreateLightEntity(const EntityDefinition& definition);
    Entity* CreateAudioEntity(const EntityDefinition& definition);
    Entity* CreateSpawnPointEntity(const EntityDefinition& definition);
    void AddCollidableComponent(Entity* entity, const EntityDefinition& definition);
    void AddMeshComponent(Entity* entity, const EntityDefinition& definition);
    void AddSpriteComponent(Entity* entity, const EntityDefinition& definition);
    void AddMaterialComponent(Entity* entity, const EntityDefinition& definition);
    Entity* CreateEnemyEntity(const EntityDefinition& definition);
    Entity* CreateTriggerEntity(const EntityDefinition& definition);
    Entity* CreateWaypointEntity(const EntityDefinition& definition);
    Entity* CreateStaticPropEntity(const EntityDefinition& definition);

    // Helper methods
    void SetupTransformComponents(Entity* entity, const EntityDefinition& definition);
    void SetupGameObjectComponent(Entity* entity, const EntityDefinition& definition);

    // Creator function registry
    std::unordered_map<GameObjectType, std::function<Entity*(const EntityDefinition&)>> creators_;
};
