#pragma once

#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_set>

class Entity;
class Engine;

class System {
public:
    System();
    virtual ~System() = default;

    // Core system methods
    virtual void Update(float deltaTime) = 0;
    virtual void Render() {}
    virtual void Initialize() {}
    virtual void Shutdown() {}
    virtual void OnEntityAdded(Entity* entity) {}
    virtual void OnEntityRemoved(Entity* entity) {}

    // System state
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // Engine access (stored reference for performance)
    // Use engine_ member directly

    // Entity management
    void AddEntity(Entity* entity);
    void RemoveEntity(Entity* entity);
    bool HasEntity(Entity* entity) const;

    // Component signature for entity filtering
    template<typename... Components>
    void SetSignature() {
        signature_.clear();
        (signature_.push_back(typeid(Components)), ...);
    }

    void SetSignature(const std::vector<std::type_index>& componentTypes);

    // Check if entity matches this system's signature
    bool EntityMatchesSignature(Entity* entity) const;

    // Get entities managed by this system
    const std::unordered_set<Entity*>& GetEntities() const { return entities_; }

    // Get signature information (for debugging)
    const std::vector<std::type_index>& GetSignature() const { return signature_; }
    size_t GetSignatureSize() const { return signature_.size(); }

protected:
    std::unordered_set<Entity*> entities_;
    std::vector<std::type_index> signature_;
    bool enabled_;
    Engine& engine_;
};
