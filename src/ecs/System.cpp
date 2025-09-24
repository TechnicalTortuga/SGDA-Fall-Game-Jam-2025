#include "System.h"
#include "Entity.h"
#include "../core/Engine.h"
#include "utils/Logger.h"

System::System()
    : enabled_(true)
    , engine_(Engine::GetInstance())
{
    LOG_DEBUG("System created");
}

void System::AddEntity(Entity* entity)
{
    if (!entity) {
        LOG_WARNING("Attempted to add null entity to system");
        return;
    }

    if (entities_.find(entity) != entities_.end()) {
        LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " already in system");
        return;
    }

    LOG_DEBUG("Attempting to add entity " + std::to_string(entity->GetId()) + " to system with " +
              std::to_string(signature_.size()) + " signature components");

    if (EntityMatchesSignature(entity)) {
        entities_.insert(entity);
        OnEntityAdded(entity);
        LOG_INFO("Entity " + std::to_string(entity->GetId()) + " added to system - total: " +
                  std::to_string(entities_.size()));
    } else {
        LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " does not match system signature");
    }
}

void System::RemoveEntity(Entity* entity)
{
    if (!entity) {
        LOG_WARNING("Attempted to remove null entity from system");
        return;
    }

    auto it = entities_.find(entity);
    if (it != entities_.end()) {
        entities_.erase(it);
        OnEntityRemoved(entity);
        LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " removed from system");
    }
}

bool System::HasEntity(Entity* entity) const
{
    return entity && entities_.find(entity) != entities_.end();
}

void System::SetSignature(const std::vector<std::type_index>& componentTypes)
{
    signature_ = componentTypes;
}

bool System::EntityMatchesSignature(Entity* entity) const
{
    if (!entity) return false;

    // If no signature is set, accept all entities
    if (signature_.empty()) {
        LOG_INFO("System has empty signature, accepting entity " + std::to_string(entity->GetId()));
        return true;
    }

    LOG_DEBUG("Checking entity " + std::to_string(entity->GetId()) + " against signature with " +
               std::to_string(signature_.size()) + " required components");

    // Check if entity has all required components
    for (const auto& type : signature_) {
        if (!entity->HasComponent(type)) {
            LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " missing required component: " +
                      std::string(type.name()));
            return false;
        } else {
            LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " has required component: " +
                      std::string(type.name()));
        }
    }

    LOG_DEBUG("Entity " + std::to_string(entity->GetId()) + " matches signature");
    return true;
}
