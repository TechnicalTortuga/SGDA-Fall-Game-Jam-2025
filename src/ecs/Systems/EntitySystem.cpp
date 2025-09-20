#include "EntitySystem.h"

EntitySystem::EntitySystem() {
    LOG_INFO("EntitySystem initialized");
}

// System interface implementations
void EntitySystem::Initialize() {
    LOG_INFO("EntitySystem: Initializing entity management system");
    // Initialize any required resources
}

void EntitySystem::Update(float deltaTime) {
    // Entity system doesn't need per-frame updates
    // This could be used for entity cleanup, validation, etc.
    static float timeAccumulator = 0.0f;
    timeAccumulator += deltaTime;

    // Periodic validation (every 5 seconds)
    if (timeAccumulator >= 5.0f) {
        ValidateEntities();
        timeAccumulator = 0.0f;
    }
}

void EntitySystem::Shutdown() {
    LOG_INFO("EntitySystem: Shutting down, cleaning up " + std::to_string(GetEntityCount()) + " entities");
    ClearAllEntities();
}

Entity* EntitySystem::CreateEntity() {
    return CreateEntity(nextEntityId_++);
}

Entity* EntitySystem::CreateEntity(Entity::EntityId id) {
    if (entities_.find(id) != entities_.end()) {
        LOG_WARNING("Entity with ID " + std::to_string(id) + " already exists, cannot create duplicate");
        return nullptr;
    }

    auto entity = std::make_unique<Entity>(id);
    Entity* entityPtr = entity.get();

    entities_[id] = std::move(entity);
    totalEntitiesCreated_++;

    LOG_DEBUG("Created entity with ID: " + std::to_string(id));
    return entityPtr;
}

void EntitySystem::DestroyEntity(Entity* entity) {
    if (!entity) {
        LOG_WARNING("Attempted to destroy null entity");
        return;
    }

    DestroyEntity(entity->GetId());
}

void EntitySystem::DestroyEntity(Entity::EntityId id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        LOG_WARNING("Attempted to destroy non-existent entity with ID: " + std::to_string(id));
        return;
    }

    entities_.erase(it);
    totalEntitiesDestroyed_++;

    LOG_DEBUG("Destroyed entity with ID: " + std::to_string(id));
}

Entity* EntitySystem::GetEntity(Entity::EntityId id) const {
    auto it = entities_.find(id);
    return (it != entities_.end()) ? it->second.get() : nullptr;
}

bool EntitySystem::EntityExists(Entity::EntityId id) const {
    return entities_.find(id) != entities_.end();
}

const std::unordered_map<Entity::EntityId, std::unique_ptr<Entity>>& EntitySystem::GetAllEntities() const {
    return entities_;
}

std::vector<Entity*> EntitySystem::GetEntitiesWithComponentType(std::type_index componentType) {
    std::vector<Entity*> result;
    result.reserve(entities_.size() / 4); // Rough estimate

    for (const auto& pair : entities_) {
        Entity* entity = pair.second.get();
        if (entity && entity->HasComponent(componentType)) {
            result.push_back(entity);
        }
    }

    return result;
}

std::vector<Entity*> EntitySystem::FindEntities(const std::function<bool(Entity*)>& predicate) {
    std::vector<Entity*> result;
    result.reserve(entities_.size() / 4); // Rough estimate

    for (const auto& pair : entities_) {
        Entity* entity = pair.second.get();
        if (entity && predicate(entity)) {
            result.push_back(entity);
        }
    }

    return result;
}

size_t EntitySystem::GetEntityCount() const {
    return entities_.size();
}

std::string EntitySystem::GetEntityStats() const {
    std::stringstream ss;
    ss << "EntityManager Statistics:\n";
    ss << "  Active Entities: " << entities_.size() << "\n";
    ss << "  Total Created: " << totalEntitiesCreated_ << "\n";
    ss << "  Total Destroyed: " << totalEntitiesDestroyed_ << "\n";

    // Component distribution analysis
    std::unordered_map<std::string, size_t> componentCounts;

    for (const auto& pair : entities_) {
        const Entity* entity = pair.second.get();
        if (!entity) continue;

        // Count components per entity
        size_t componentCount = entity->GetComponentCount();
        std::string key = std::to_string(componentCount) + " components";
        componentCounts[key]++;
    }

    ss << "  Entity Composition:\n";
    for (const auto& pair : componentCounts) {
        ss << "    " << pair.first << ": " << pair.second << " entities\n";
    }

    return ss.str();
}

void EntitySystem::ClearAllEntities() {
    size_t entityCount = entities_.size();
    entities_.clear();
    totalEntitiesDestroyed_ += entityCount;

    LOG_INFO("Cleared all entities (" + std::to_string(entityCount) + " entities destroyed)");
}

bool EntitySystem::ValidateEntities() const {
    bool isValid = true;

    for (const auto& pair : entities_) {
        const Entity* entity = pair.second.get();

        if (!entity) {
            LOG_ERROR("EntityManager: Null entity pointer found for ID: " + std::to_string(pair.first));
            isValid = false;
            continue;
        }

        if (entity->GetId() != pair.first) {
            LOG_ERROR("EntityManager: Entity ID mismatch - stored ID: " + std::to_string(pair.first) +
                     ", entity ID: " + std::to_string(entity->GetId()));
            isValid = false;
        }

        if (!entity->IsActive()) {
            LOG_WARNING("EntityManager: Inactive entity found with ID: " + std::to_string(entity->GetId()));
        }
    }

    if (isValid) {
        LOG_DEBUG("EntityManager: All entities validated successfully (" + std::to_string(entities_.size()) + " entities)");
    } else {
        LOG_ERROR("EntityManager: Entity validation failed");
    }

    return isValid;
}
