#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <typeindex>
#include "System.h"
#include "Entity.h"
#include "../utils/Logger.h"

/*
EntitySystem - ECS System for entity lifecycle management and queries

This system replaces the owner coupling we removed from components by providing:
- Entity lifecycle management (creation/destruction)
- Entity queries by component types
- Entity iteration and filtering
- Entity relationship management
- Performance-optimized entity storage

This enables systems to query for entities with specific component combinations
without components needing to know about their owning entities.

Integrates with ECS System framework for proper update iteration.
*/
class EntitySystem : public System {
public:
    EntitySystem();

    /*
    Create a new entity with automatic ID generation
    Returns: Pointer to the newly created entity
    */
    Entity* CreateEntity();

    /*
    Create a new entity with specific ID
    id: The specific ID to assign to the entity
    Returns: Pointer to the newly created entity
    */
    Entity* CreateEntity(Entity::EntityId id);

    /*
    Destroy an entity and clean up all its components
    entity: The entity to destroy
    */
    void DestroyEntity(Entity* entity);

    /*
    Destroy an entity by ID
    id: The ID of the entity to destroy
    */
    void DestroyEntity(Entity::EntityId id);

    /*
    Get an entity by ID
    id: The entity ID to look up
    Returns: Pointer to the entity, or nullptr if not found
    */
    Entity* GetEntity(Entity::EntityId id) const;

    /*
    Check if an entity exists
    id: The entity ID to check
    Returns: True if the entity exists
    */
    bool EntityExists(Entity::EntityId id) const;

    /*
    Get all entities
    Returns: Vector of all active entities
    */
    const std::unordered_map<Entity::EntityId, std::unique_ptr<Entity>>& GetAllEntities() const;

    /*
    Query entities by component types
    T: The component type to query for
    Returns: Vector of entities that have the specified component
    */
    template<typename T>
    std::vector<Entity*> GetEntitiesWithComponent();

    /*
    Query entities by multiple component types
    T1, T2: The component types to query for
    Returns: Vector of entities that have all specified components
    */
    template<typename T1, typename T2>
    std::vector<Entity*> GetEntitiesWithComponents();

    /*
    Query entities by component type index
    componentType: The component type to query for
    Returns: Vector of entities that have the specified component type
    */
    std::vector<Entity*> GetEntitiesWithComponentType(std::type_index componentType);

    /*
    Find entities by predicate function
    predicate: Function that takes an Entity* and returns true if it matches
    Returns: Vector of entities that match the predicate
    */
    std::vector<Entity*> FindEntities(const std::function<bool(Entity*)>& predicate);

    /*
    Get the total number of active entities
    Returns: Number of active entities
    */
    size_t GetEntityCount() const;

    /*
    Get statistics about entity composition
    Returns: String containing entity statistics
    */
    std::string GetEntityStats() const;

    /*
    Clear all entities (for cleanup/reset)
    WARNING: This destroys all entities
    */
    void ClearAllEntities();

    /*
    Validate entity integrity (for debugging)
    Returns: True if all entities are valid
    */
    bool ValidateEntities() const;

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

private:
    ~EntitySystem() = default;
    EntitySystem(const EntitySystem&) = delete;
    EntitySystem& operator=(const EntitySystem&) = delete;

    // Entity storage
    std::unordered_map<Entity::EntityId, std::unique_ptr<Entity>> entities_;

    // ID generation
    Entity::EntityId nextEntityId_ = 1;

    // Statistics tracking
    size_t totalEntitiesCreated_ = 0;
    size_t totalEntitiesDestroyed_ = 0;
};

// Template implementations
template<typename T>
std::vector<Entity*> EntitySystem::GetEntitiesWithComponent() {
    std::vector<Entity*> result;
    result.reserve(entities_.size() / 4); // Rough estimate

    for (const auto& pair : entities_) {
        Entity* entity = pair.second.get();
        if (entity && entity->HasComponent<T>()) {
            result.push_back(entity);
        }
    }

    return result;
}

template<typename T1, typename T2>
std::vector<Entity*> EntitySystem::GetEntitiesWithComponents() {
    std::vector<Entity*> result;
    result.reserve(entities_.size() / 4); // Rough estimate

    for (const auto& pair : entities_) {
        Entity* entity = pair.second.get();
        if (entity && entity->HasComponent<T1>() && entity->HasComponent<T2>()) {
            result.push_back(entity);
        }
    }

    return result;
}
