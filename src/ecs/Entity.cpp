#include "Entity.h"
#include "Component.h"
#include "utils/Logger.h"
#include <sstream>

Entity::Entity(EntityId id)
    : id_(id), active_(true)
{
    LOG_DEBUG("Entity created with ID: " + std::to_string(id_));
}

Entity::~Entity()
{
    components_.clear();
    LOG_DEBUG("Entity destroyed with ID: " + std::to_string(id_));
}

void Entity::AddComponent(std::unique_ptr<Component> component)
{
    if (!component) {
        LOG_WARNING("Attempted to add null component to entity " + std::to_string(id_));
        return;
    }

    std::type_index typeIndex(typeid(*component));
    // Removed owner coupling - components should not know about their entity
    components_[typeIndex] = std::move(component);
    LOG_DEBUG("Added component " + std::string(component->GetTypeName()) + " to entity " + std::to_string(id_));
}

Component* Entity::GetComponent(std::type_index type) const
{
    auto it = components_.find(type);
    return (it != components_.end()) ? it->second.get() : nullptr;
}

bool Entity::HasComponent(std::type_index type) const
{
    return components_.find(type) != components_.end();
}

void Entity::RemoveComponent(std::type_index type)
{
    auto it = components_.find(type);
    if (it != components_.end()) {
        components_.erase(it);
    }
}

std::string Entity::ToString() const
{
    std::stringstream ss;
    ss << "Entity[" << id_ << "](" << components_.size() << " components)";
    return ss.str();
}
