//
// Created by sergio on 9/23/25.
//

#ifndef TRYTWOFORGAMEJAM_COMPONENT_H
#define TRYTWOFORGAMEJAM_COMPONENT_H

#pragma once

/*
Component - Pure data container for ECS architecture

Components are pure data structures with no knowledge of their containing entity.
All logic that operates on component data lives in Systems, not in components themselves.

This design enables:
- Cache-friendly memory layout
- Easy serialization for networking
- Independent component testing
- Parallel system processing
*/
class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    // Prevent copying (use move semantics)
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // Allow moving
    Component(Component&&) = default;
    Component& operator=(Component&&) = default;

    // Optional: Component type identification for debugging
    virtual const char* GetTypeName() const { return "Component"; }
};


#endif //TRYTWOFORGAMEJAM_COMPONENT_H