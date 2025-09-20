#include "ComponentRegistry.h"
#include <algorithm>

ComponentRegistry& ComponentRegistry::GetInstance() {
    static ComponentRegistry instance;
    return instance;
}

std::unique_ptr<Component> ComponentRegistry::CreateComponent(std::type_index typeIndex) const {
    auto it = typeRegistry_.find(typeIndex);
    if (it == typeRegistry_.end()) {
        LOG_ERROR("ComponentRegistry: Attempted to create unregistered component type");
        return nullptr;
    }

    if (!it->second.factory) {
        LOG_ERROR("ComponentRegistry: No factory function for component type: " + it->second.name);
        return nullptr;
    }

    auto component = it->second.factory();
    LOG_DEBUG("ComponentRegistry: Created component: " + it->second.name);
    return component;
}

std::unique_ptr<Component> ComponentRegistry::CreateComponent(const std::string& name) const {
    auto it = nameToTypeMap_.find(name);
    if (it == nameToTypeMap_.end()) {
        LOG_ERROR("ComponentRegistry: Attempted to create unregistered component: " + name);
        return nullptr;
    }

    return CreateComponent(it->second);
}

const ComponentRegistry::ComponentTypeInfo* ComponentRegistry::GetTypeInfo(std::type_index typeIndex) const {
    auto it = typeRegistry_.find(typeIndex);
    return (it != typeRegistry_.end()) ? &it->second : nullptr;
}

const ComponentRegistry::ComponentTypeInfo* ComponentRegistry::GetTypeInfo(const std::string& name) const {
    auto it = nameToTypeMap_.find(name);
    if (it == nameToTypeMap_.end()) {
        return nullptr;
    }

    return GetTypeInfo(it->second);
}

bool ComponentRegistry::IsRegistered(std::type_index typeIndex) const {
    return typeRegistry_.find(typeIndex) != typeRegistry_.end();
}

bool ComponentRegistry::IsRegistered(const std::string& name) const {
    return nameToTypeMap_.find(name) != nameToTypeMap_.end();
}

std::vector<std::type_index> ComponentRegistry::GetRegisteredTypes() const {
    std::vector<std::type_index> types;
    types.reserve(typeRegistry_.size());

    for (const auto& pair : typeRegistry_) {
        types.push_back(pair.first);
    }

    return types;
}

std::string ComponentRegistry::GetComponentName(std::type_index typeIndex) const {
    const ComponentTypeInfo* info = GetTypeInfo(typeIndex);
    return info ? info->name : "";
}

std::type_index ComponentRegistry::GetComponentType(const std::string& name) const {
    auto it = nameToTypeMap_.find(name);
    return (it != nameToTypeMap_.end()) ? it->second : std::type_index(typeid(void));
}

void ComponentRegistry::ClearRegistry() {
    typeRegistry_.clear();
    nameToTypeMap_.clear();
}

size_t ComponentRegistry::GetRegisteredCount() const {
    return typeRegistry_.size();
}

// Template implementation is in the header file
