#pragma once

#include <unordered_map>
#include <typeindex>
#include <string>
#include <memory>
#include <functional>
#include "../utils/Logger.h"
#include "Component.h"

/*
ComponentRegistry - Manages component type registration and factory functions

This registry enables runtime component type management without direct coupling,
supporting the ECR (ECS Compliance Refactoring) goals of removing owner references
and enabling lightweight component queries.
*/
class ComponentRegistry {
public:
    // Component type information
    struct ComponentTypeInfo {
        std::string name;
        size_t size;
        std::function<std::unique_ptr<Component>()> factory;
        bool isNetworkSerializable = false;
    };

    static ComponentRegistry& GetInstance();

    /*
    Register a component type with the registry
    T: The component type to register
    name: Human-readable name for the component
    networkSerializable: Whether this component can be serialized for networking
    */
    template<typename T>
    void RegisterComponent(const std::string& name, bool networkSerializable = false);

    /*
    Create a component instance by type index
    typeIndex: The type index of the component to create
    Returns: Unique pointer to the created component
    */
    std::unique_ptr<Component> CreateComponent(std::type_index typeIndex) const;

    /*
    Create a component instance by name
    name: The registered name of the component
    Returns: Unique pointer to the created component
    */
    std::unique_ptr<Component> CreateComponent(const std::string& name) const;

    /*
    Get type information for a component
    typeIndex: The type index to look up
    Returns: Const reference to the type information
    */
    const ComponentTypeInfo* GetTypeInfo(std::type_index typeIndex) const;

    /*
    Get type information for a component by name
    name: The registered name to look up
    Returns: Const reference to the type information
    */
    const ComponentTypeInfo* GetTypeInfo(const std::string& name) const;

    /*
    Check if a component type is registered
    typeIndex: The type index to check
    Returns: True if the type is registered
    */
    bool IsRegistered(std::type_index typeIndex) const;

    /*
    Check if a component name is registered
    name: The name to check
    Returns: True if the name is registered
    */
    bool IsRegistered(const std::string& name) const;

    /*
    Get all registered component types
    Returns: Vector of type indices for all registered components
    */
    std::vector<std::type_index> GetRegisteredTypes() const;

    /*
    Get the component name for a type index
    typeIndex: The type index to look up
    Returns: The registered name, or empty string if not found
    */
    std::string GetComponentName(std::type_index typeIndex) const;

    /*
    Get the type index for a component name
    name: The name to look up
    Returns: The type index, or typeid(void) if not found
    */
    std::type_index GetComponentType(const std::string& name) const;

    /*
    Clear all registered component types
    WARNING: This should only be used for testing or system reset
    */
    void ClearRegistry();

    /*
    Get the number of registered component types
    Returns: Number of registered types
    */
    size_t GetRegisteredCount() const;

private:
    ComponentRegistry() = default;
    ~ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    // Registry storage
    std::unordered_map<std::type_index, ComponentTypeInfo> typeRegistry_;
    std::unordered_map<std::string, std::type_index> nameToTypeMap_;
};

// Template implementation
template<typename T>
void ComponentRegistry::RegisterComponent(const std::string& name, bool networkSerializable) {
    std::type_index typeIndex = std::type_index(typeid(T));

    // Check if already registered
    if (typeRegistry_.find(typeIndex) != typeRegistry_.end()) {
        // Log warning about re-registration
        return;
    }

    // Create type info
    ComponentTypeInfo info;
    info.name = name;
    info.size = sizeof(T);
    info.factory = []() -> std::unique_ptr<Component> {
        return std::make_unique<T>();
    };
    info.isNetworkSerializable = networkSerializable;

    // Register the type
    typeRegistry_[typeIndex] = std::move(info);
    nameToTypeMap_.emplace(name, typeIndex);
}
