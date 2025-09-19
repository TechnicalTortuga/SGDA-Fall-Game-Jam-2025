#pragma once

class Entity;

class Component {
public:
    Component() : owner_(nullptr) {}
    virtual ~Component() = default;

    // Prevent copying
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // Allow moving
    Component(Component&&) = default;
    Component& operator=(Component&&) = default;

    // Access to owner entity
    Entity* GetOwner() const { return owner_; }

    // Virtual methods for component lifecycle
    virtual void OnAttach() {}    // Called when component is attached to entity
    virtual void OnDetach() {}    // Called when component is detached from entity

private:
    friend class Entity;
    Entity* owner_;
};
