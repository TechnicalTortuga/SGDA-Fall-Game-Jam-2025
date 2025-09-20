# PaintSplash Code Style Guide

*Comprehensive coding standards and conventions for the PaintSplash project*

---

## 📋 **Table of Contents**
1. [General Principles](#general-principles)
2. [Naming Conventions](#naming-conventions)
3. [Commenting Standards](#commenting-standards)
4. [File Organization](#file-organization)
5. [Code Structure](#code-structure)
6. [ECS-Specific Guidelines](#ecs-specific-guidelines)
7. [Build System Standards](#build-system-standards)
8. [Testing Conventions](#testing-conventions)

---

## 🎯 **General Principles**

### **Core Philosophy**
- **Clarity over Cleverness**: Code should be readable and maintainable
- **Consistency is King**: Follow established patterns throughout
- **Documentation First**: Write docs alongside code, not as an afterthought
- **Test-Driven Development**: Write tests for critical functionality
- **Performance Conscious**: Optimize where it matters, document trade-offs

### **Code Quality Standards**
- **Zero Compiler Warnings**: Treat warnings as errors
- **RAII Everywhere**: Use smart pointers, avoid raw pointers when possible
- **Exception Safety**: Code should be exception-safe where practical
- **Resource Management**: Clear ownership and cleanup responsibilities
- **Thread Safety**: Document thread-safety guarantees

---

## 🏷️ **Naming Conventions**

### **Files and Directories**
```cpp
// ✅ CORRECT: PascalCase for class files
TransformComponent.h
EntitySystem.cpp
MaterialComponent.h

// ✅ CORRECT: camelCase for utility files
pathUtils.h
configParser.cpp

// ✅ CORRECT: Directory structure
src/
├── core/          // Engine core systems
├── ecs/           // ECS architecture
│   ├── Components/ // All component classes
│   └── Systems/    // All system classes
├── networking/    // Network systems
└── utils/         // Utility functions
```

### **Classes and Types**
```cpp
// ✅ CORRECT: PascalCase for all types
class EntitySystem : public System {
    // ...
};

struct ComponentTypeInfo {
    // ...
};

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};
```

### **Functions and Methods**
```cpp
// ✅ CORRECT: PascalCase for public methods
void Initialize();
void Update(float deltaTime);
void RenderSystem::DrawEntities();

// ✅ CORRECT: camelCase for private/internal methods
void updateMatrices() const;
void validateEntityIntegrity();

// ✅ CORRECT: Descriptive names with context
void SetPosition(const Vector3& position);        // Clear what it does
void position_ = {0, 0, 0};                      // Private member with underscore
```

### **Variables and Members**
```cpp
// ✅ CORRECT: camelCase for locals and parameters
Vector3 position = {0.0f, 0.0f, 0.0f};
float deltaTime = 0.016f;

// ✅ CORRECT: Private members with trailing underscore
private:
    Vector3 position_;
    float scale_;
    bool isDirty_;

// ✅ CORRECT: Static/const with UPPER_SNAKE_CASE
static const float DEFAULT_SCALE = 1.0f;
const size_t MAX_ENTITIES = 10000;

// ✅ CORRECT: Boolean names clearly state condition
bool isVisible_ = true;
bool hasTexture_ = false;
bool shouldUpdate_ = true;
```

### **Constants and Enums**
```cpp
// ✅ CORRECT: UPPER_SNAKE_CASE for constants
const float PI = 3.14159f;
const size_t MAX_TEXTURE_SIZE = 4096;

// ✅ CORRECT: PascalCase for enum values
enum class MaterialType {
    BASIC,      // Simple diffuse + specular
    PBR,        // Physically-based rendering
    EMISSIVE,   // Glowing materials
    TRANSPARENT // Transparent materials
};
```

---

## 💬 **Commenting Standards**

### **Block Comments (Multi-line)**
```cpp
/*
Component - Pure data container for ECS architecture

Components are pure data structures with no knowledge of their containing entity.
All logic that operates on component data lives in Systems, not in components themselves.

Features:
- Cache-friendly memory layout
- Easy serialization for networking
- Independent component testing
- Parallel system processing
*/
class Component {
    // ...
};
```

### **Function Comments**
```cpp
/*
Create a new entity with automatic ID generation
Returns: Pointer to the newly created entity
*/
Entity* CreateEntity();

/*
Register a component type with the registry
T: The component type to register
name: Human-readable name for the component
networkSerializable: Whether this component can be serialized for networking
*/
template<typename T>
void RegisterComponent(const std::string& name, bool networkSerializable = false);
```

### **Inline Comments**
```cpp
// ✅ CORRECT: Explain why, not what
int entityCount = entities.size();  // Reserve space for efficiency

// ✅ CORRECT: Mark important decisions
const float EPSILON = 0.001f;  // Small value to prevent division by zero

// ✅ CORRECT: TODO comments for future work
// TODO: Implement network serialization for this component
```

### **Class Member Comments**
```cpp
class TransformComponent : public Component {
public:
    const Vector3& GetPosition() const { return position_; }

private:
    Vector3 position_;              // World position (x, y, z)
    Vector3 scale_;                 // Scale factors (x, y, z)
    Quaternion rotation_;           // Rotation quaternion
    bool isDirty_;                  // True when transform needs recalculation
};
```

### **File Headers**
```cpp
#pragma once

/*
TransformComponent - Enhanced position component with scale and rotation

This replaces the simple Position component with a full transform system
that includes position, scale, and rotation. Designed for network-ready
architecture with interpolation support.

Author: PaintSplash Team
Created: September 20, 2025
*/
```

---

## 📁 **File Organization**

### **Header File Structure**
```cpp
#pragma once

// System includes (alphabetical)
#include <memory>
#include <string>
#include <unordered_map>

// Third-party includes (alphabetical)
#include "raylib.h"
#include "raymath.h"

// Project includes (alphabetical, relative paths)
#include "../Component.h"
#include "../utils/Logger.h"

/*
ClassName - Brief description of what this class does

Detailed description of the class purpose, design decisions,
and any important implementation notes.
*/
class ClassName {
public:
    // Public methods (grouped by functionality)
    // Constructors/destructors first
    ClassName();
    ~ClassName();

    // Core functionality
    void Initialize();
    void Update(float deltaTime);

    // Accessors (getters/setters)
    const Vector3& GetPosition() const;
    void SetPosition(const Vector3& position);

private:
    // Private methods
    void updateInternalState();

    // Member variables (grouped logically)
    Vector3 position_;
    float scale_;
    bool isActive_;
};

// Template implementations (if needed)
template<typename T>
void ClassName::TemplateMethod() {
    // Implementation
}
```

### **Source File Structure**
```cpp
#include "ClassName.h"
#include "raylib.h"
#include "../utils/Logger.h"

// Constructor implementations
ClassName::ClassName()
    : position_(0.0f, 0.0f, 0.0f)
    , scale_(1.0f)
    , isActive_(true)
{
    LOG_DEBUG("ClassName created");
}

// Public method implementations (in same order as header)
const Vector3& ClassName::GetPosition() const {
    return position_;
}

void ClassName::SetPosition(const Vector3& position) {
    position_ = position;
    isDirty_ = true;
}

// Private method implementations
void ClassName::updateInternalState() {
    // Implementation
}
```

---

## 🏗️ **Code Structure**

### **Class Design Principles**
```cpp
// ✅ CORRECT: Single Responsibility Principle
class RenderSystem : public System {
    // Only handles rendering logic
};

class PhysicsSystem : public System {
    // Only handles physics simulation
};

// ✅ CORRECT: Interface segregation
class ITransformable {
public:
    virtual void SetPosition(const Vector3& pos) = 0;
    virtual const Vector3& GetPosition() const = 0;
};

// ✅ CORRECT: Composition over inheritance
class Player {
public:
    Player()
        : transform_(std::make_unique<TransformComponent>())
        , physics_(std::make_unique<PhysicsComponent>())
    {}

private:
    std::unique_ptr<TransformComponent> transform_;
    std::unique_ptr<PhysicsComponent> physics_;
};
```

### **Error Handling**
```cpp
// ✅ CORRECT: Use assertions for programming errors
assert(entity != nullptr && "Entity cannot be null");

// ✅ CORRECT: Return error codes for recoverable errors
enum class LoadResult {
    SUCCESS,
    FILE_NOT_FOUND,
    INVALID_FORMAT,
    OUT_OF_MEMORY
};

LoadResult TextureComponent::LoadTexture(const std::string& path) {
    if (!FileExists(path.c_str())) {
        LOG_ERROR("Texture file not found: " + path);
        return LoadResult::FILE_NOT_FOUND;
    }
    // ... load texture
    return LoadResult::SUCCESS;
}

// ✅ CORRECT: Use exceptions only for truly exceptional cases
try {
    texture = LoadTexture(path);
} catch (const std::bad_alloc& e) {
    LOG_FATAL("Failed to allocate memory for texture: " + std::string(e.what()));
    // Handle out of memory
}
```

### **Resource Management**
```cpp
// ✅ CORRECT: RAII pattern
class TextureResource {
public:
    TextureResource(const std::string& path) {
        texture_ = LoadTexture(path.c_str());
        if (texture_.id == 0) {
            throw std::runtime_error("Failed to load texture: " + path);
        }
    }

    ~TextureResource() {
        if (texture_.id != 0) {
            UnloadTexture(texture_);
        }
    }

    // Disable copying
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;

    // Allow moving
    TextureResource(TextureResource&& other) noexcept
        : texture_(other.texture_) {
        other.texture_.id = 0;  // Transfer ownership
    }

private:
    Texture2D texture_;
};
```

---

## 🎮 **ECS-Specific Guidelines**

### **Component Design**
```cpp
// ✅ CORRECT: Use structs for pure data components
struct TransformComponent : public Component {
    Vector3 position = {0, 0, 0};
    Vector3 scale = {1, 1, 1};
    Quaternion rotation = {0, 0, 0, 1};
    bool isDirty = false;

    // ✅ CORRECT: Simple computed properties or validation
    bool IsValid() const {
        return scale.x > 0 && scale.y > 0 && scale.z > 0;
    }

    // ❌ WRONG: No business logic in components
    // void MoveTowards(const Vector3& target, float speed) { ... }

    // ❌ WRONG: No private members in component structs
    // private: int internalData_;
};

// ✅ CORRECT: Use classes only for components with complex internal state
class MeshComponent : public Component {
public:
    // Public interface only - no private data
    void SetMaterial(int materialId) { materialId_ = materialId; }
    int GetMaterial() const { return materialId_; }

    // Complex operations that need encapsulation
    void Clear();
    void AddVertex(const Vector3& position, const Vector3& normal = {0,1,0});

private:
    std::vector<MeshVertex> vertices_;
    std::vector<MeshTriangle> triangles_;
    int materialId_ = 0;
};

// ✅ CORRECT: Component registration
void InitializeComponentRegistry() {
    ComponentRegistry::GetInstance().RegisterComponent<TransformComponent>(
        "TransformComponent", true  // Network serializable
    );
}
```

### **ECS Component Rules**
- **Use `struct` for pure data components** - No private members, all public data
- **Use `class` only when encapsulation is needed** - Complex internal state, validation logic
- **PURE DATA ORIENTATION**: Components contain ONLY essential data - NO complex state
- **Reference IDs, not objects** - Use system/component IDs for relationships
- **No virtual methods in component structs** - Pure data only
- **All member variables should be public** - Direct access encouraged
- **Default values for all members** - Ensures consistent initialization
- **No constructors/destructors in structs** - Use aggregate initialization
- **No methods in pure data structs** - All logic belongs in systems
- **System IDs for complex operations** - Reference systems by ID, not direct coupling

### **System Design**
```cpp
// ✅ CORRECT: Single responsibility systems
class RenderSystem : public System {
public:
    void Initialize() override {
        // Setup rendering resources
    }

    void Update(float deltaTime) override {
        // Nothing - rendering happens in Render()
    }

    void Render() override {
        // Collect renderable entities
        auto renderables = entitySystem_->GetEntitiesWithComponents<
            TransformComponent, MeshComponent>();

        // Render each entity
        for (Entity* entity : renderables) {
            renderEntity(entity);
        }
    }

private:
    EntitySystem* entitySystem_;
};

// ✅ CORRECT: System communication
class PhysicsSystem : public System {
public:
    void Update(float deltaTime) override {
        // Update physics simulation
        simulatePhysics(deltaTime);

        // Notify other systems of collisions
        for (const auto& collision : collisions_) {
            EventManager::GetInstance().Emit(
                CollisionEvent{collision.entityA, collision.entityB}
            );
        }
    }
};
```

### **Entity Management**
```cpp
// ✅ CORRECT: Entity creation with components
Entity* playerEntity = entitySystem->CreateEntity();
playerEntity->AddComponent<TransformComponent>({0, 5, 0});
playerEntity->AddComponent<MeshComponent>();
playerEntity->AddComponent<PlayerComponent>();

// ✅ CORRECT: Entity queries
auto players = entitySystem->GetEntitiesWithComponent<PlayerComponent>();
auto movingEntities = entitySystem->GetEntitiesWithComponents<
    TransformComponent, VelocityComponent>();
```

---

## 🔨 **Build System Standards**

### **CMake Configuration**
```cmake
# ✅ CORRECT: Standard project structure
cmake_minimum_required(VERSION 3.16)
project(PaintSplash VERSION 1.0.0 LANGUAGES CXX)

# ✅ CORRECT: Compiler standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ✅ CORRECT: Warning levels
if(MSVC)
    add_compile_options(/W4 /WX)  # W4 warnings, WX treat as errors
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ✅ CORRECT: Library organization
add_library(paintsplash_core STATIC
    src/core/Engine.cpp
    src/core/StateManager.cpp
)

add_executable(paintsplash
    src/main.cpp
)

target_link_libraries(paintsplash
    paintsplash_core
    raylib
)
```

### **Directory Structure**
```
project/
├── CMakeLists.txt          # Root build configuration
├── src/
│   ├── CMakeLists.txt      # Source build configuration
│   ├── main.cpp           # Application entry point
│   └── [organized source files]
├── build/                 # Build artifacts (gitignored)
├── docs/                  # Documentation
├── assets/                # Game assets
├── scripts/               # Build/utility scripts
└── tests/                 # Test suites
```

---

## 🧪 **Testing Conventions**

### **Unit Test Structure**
```cpp
// ✅ CORRECT: Test file organization
// tests/ecs/test_TransformComponent.cpp
#include <gtest/gtest.h>
#include "../src/ecs/Components/TransformComponent.h"

TEST(TransformComponentTest, DefaultConstructor) {
    TransformComponent transform;

    EXPECT_EQ(transform.GetPosition().x, 0.0f);
    EXPECT_EQ(transform.GetPosition().y, 0.0f);
    EXPECT_EQ(transform.GetPosition().z, 0.0f);
}

TEST(TransformComponentTest, PositionTranslation) {
    TransformComponent transform;
    Vector3 translation = {1.0f, 2.0f, 3.0f};

    transform.Translate(translation);

    EXPECT_EQ(transform.GetPosition().x, 1.0f);
    EXPECT_EQ(transform.GetPosition().y, 2.0f);
    EXPECT_EQ(transform.GetPosition().z, 3.0f);
}
```

### **Test Naming**
```cpp
// ✅ CORRECT: Descriptive test names
TEST(ComponentRegistryTest, RegisterComponent_Success) {
    // Test successful component registration
}

TEST(EntitySystemTest, CreateEntity_WithValidId) {
    // Test entity creation with specific ID
}

TEST(RenderSystemTest, DrawEntities_WithMultipleComponents) {
    // Test rendering entities with various components
}
```

### **Test Organization**
```cpp
// ✅ CORRECT: Test fixtures for complex setup
class RenderSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        entitySystem = std::make_unique<EntitySystem>();
        renderSystem = std::make_unique<RenderSystem>();
        // Setup test entities and components
    }

    void TearDown() override {
        // Cleanup test resources
    }

    std::unique_ptr<EntitySystem> entitySystem;
    std::unique_ptr<RenderSystem> renderSystem;
};
```

---

## 📝 **Documentation Standards**

### **README Structure**
```markdown
# Project Name

Brief description of what the project does.

## Features
- Feature 1
- Feature 2
- Feature 3

## Building
```bash
mkdir build && cd build
cmake ..
make
```

## Usage
Detailed usage instructions and examples.

## Architecture
High-level architecture overview.

## Contributing
Guidelines for contributors.

## License
License information.
```

### **API Documentation**
```cpp
/*
@brief Calculate the distance between two points
@param a First point
@param b Second point
@return Distance between the points
@note This function uses Euclidean distance
@see Vector3DistanceSqr for squared distance
@warning Points should be valid (non-NaN, finite values)
*/
float Vector3Distance(const Vector3& a, const Vector3& b);
```

---

## 🔍 **Code Review Checklist**

### **Before Committing**
- [ ] Code compiles without warnings
- [ ] Unit tests pass
- [ ] Code follows style guide
- [ ] Documentation updated
- [ ] No TODO comments left unresolved
- [ ] Performance impact considered
- [ ] Memory leaks checked
- [ ] Thread safety verified (if applicable)

### **Pull Request Review**
- [ ] Code is well-documented
- [ ] Tests are comprehensive
- [ ] Architecture decisions are sound
- [ ] Performance implications understood
- [ ] Security considerations addressed
- [ ] Breaking changes are justified

---

## 🎯 **Quick Reference**

### **File Naming**
- Headers: `.h` or `.hpp`
- Sources: `.cpp`
- Classes: `PascalCase`
- Functions: `PascalCase` (public), `camelCase` (private)

### **Bracing Style**
```cpp
// ✅ CORRECT: K&R style
if (condition) {
    doSomething();
} else {
    doSomethingElse();
}

// ✅ CORRECT: Single statements
if (condition) { doSomething(); }
```

### **Include Order**
```cpp
// System includes (alphabetical)
#include <memory>
#include <string>

// Third-party includes (alphabetical)
#include "raylib.h"
#include "raymath.h"

// Project includes (alphabetical)
#include "../Component.h"
#include "../utils/Logger.h"
```

### **Magic Numbers**
```cpp
// ❌ WRONG: Magic numbers
if (health < 0) { /* ... */ }

// ✅ CORRECT: Named constants
const int MIN_HEALTH = 0;
if (health < MIN_HEALTH) { /* ... */ }
```

---

*This style guide is living documentation. Update it as the codebase evolves and new patterns emerge.*

*Last Updated: September 20, 2025*
