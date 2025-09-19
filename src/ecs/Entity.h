#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <string>

class Component;

class Entity {
public:
    using EntityId = uint64_t;

    explicit Entity(EntityId id);
    ~Entity();

    EntityId GetId() const { return id_; }

    // Component management
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value,
                      "T must inherit from Component");

        std::type_index typeIndex(typeid(T));

        if (HasComponent(typeIndex)) {
            return GetComponent<T>();
        }

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->owner_ = this;
        T* rawPtr = component.get();
        components_[typeIndex] = std::move(component);


        return rawPtr;
    }

    template<typename T>
    T* GetComponent() const {
        static_assert(std::is_base_of<Component, T>::value,
                      "T must inherit from Component");

        std::type_index typeIndex(typeid(T));
        auto it = components_.find(typeIndex);

        if (it != components_.end()) {
            return static_cast<T*>(it->second.get());
        }

        return nullptr;
    }

    template<typename T>
    bool HasComponent() const {
        static_assert(std::is_base_of<Component, T>::value,
                      "T must inherit from Component");

        std::type_index typeIndex(typeid(T));
        return components_.find(typeIndex) != components_.end();
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of<Component, T>::value,
                      "T must inherit from Component");

        std::type_index typeIndex(typeid(T));
        auto it = components_.find(typeIndex);

        if (it != components_.end()) {
            components_.erase(it);
        }
    }

    // General component management
    void AddComponent(std::unique_ptr<Component> component);
    Component* GetComponent(std::type_index type) const;
    bool HasComponent(std::type_index type) const;
    void RemoveComponent(std::type_index type);

    // Utility
    bool IsActive() const { return active_; }
    void SetActive(bool isActive) { active_ = isActive; }

    size_t GetComponentCount() const { return components_.size(); }

    std::string ToString() const;

private:
    EntityId id_;
    bool active_;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components_;
};
