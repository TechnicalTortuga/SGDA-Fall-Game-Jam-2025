#include "Engine.h"
#include "StateManager.h"
#include "events/EventManager.h"
#include "ecs/Entity.h"
#include "ecs/System.h"
#include "ecs/Systems/RenderSystem.h"
#include "ecs/Systems/PlayerSystem.h"
#include "ecs/Systems/WorldSystem.h"
#include "ecs/Systems/InputSystem.h"
#include "rendering/WorldRenderer.h"
#include "ecs/Systems/CollisionSystem.h"
#include "physics/PhysicsSystem.h"
#include "ui/ConsoleSystem.h"
#include "utils/Logger.h"

Engine::Engine()
    : stateManager_(nullptr), eventManager_(nullptr), nextEntityId_(1)
{
    LOG_INFO("Engine created");
}

Engine::~Engine()
{
    Shutdown();
    LOG_INFO("Engine destroyed");
}

bool Engine::Initialize() {
    LOG_INFO("Initializing engine systems...");

    try {
        // Initialize core managers
        InitializeEventManager();
        InitializeStateManager();

        // Create core systems
        auto renderSystem = AddSystem<RenderSystem>();
        auto inputSystem = AddSystem<InputSystem>();
        auto playerSystem = AddSystem<PlayerSystem>();
        auto worldSystem = AddSystem<WorldSystem>();
        auto collisionSystem = AddSystem<CollisionSystem>();
        auto physicsSystem = AddSystem<PhysicsSystem>();
        auto consoleSystem = AddSystem<ConsoleSystem>();

        // NOTE: MovementSystem removed - PhysicsSystem handles all movement now

        // Initialize systems
        for (auto& system : systems_) {
            system->Initialize();
        }

        // Set up system interdependencies
        if (renderSystem && playerSystem) {
            playerSystem->SetRenderer(renderSystem->GetRenderer());
        }

        if (consoleSystem && playerSystem) {
            consoleSystem->SetPlayerEntity(playerSystem->GetPlayer());
        }

        if (collisionSystem && consoleSystem) {
            consoleSystem->SetCollisionSystem(collisionSystem);
        }

        if (collisionSystem && physicsSystem) {
            physicsSystem->SetCollisionSystem(collisionSystem);
        }

        if (collisionSystem && worldSystem) {
            worldSystem->ConnectCollisionSystem(collisionSystem);
        }

        if (inputSystem && playerSystem) {
            playerSystem->SetInputSystem(inputSystem);
        }

        LOG_INFO("Engine initialization completed successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Engine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void Engine::Shutdown() {
    LOG_INFO("Shutting down engine...");

    // Shutdown all systems in reverse order
    for (auto it = systems_.rbegin(); it != systems_.rend(); ++it) {
        (*it)->Shutdown();
    }

    // Clean up managers
    if (stateManager_) {
        delete stateManager_;
        stateManager_ = nullptr;
    }
    if (eventManager_) {
        delete eventManager_;
        eventManager_ = nullptr;
    }

    Clear();
    LOG_INFO("Engine shutdown completed");
}

void Engine::Update(float deltaTime)
{
    LOG_DEBUG("Engine: Updating " + std::to_string(systems_.size()) + " systems");

    // Update state manager first
    if (stateManager_) {
        stateManager_->Update(deltaTime);
    }

    // Update all systems
    int systemIndex = 0;
    for (auto& system : systems_) {
        LOG_DEBUG("Engine: Updating system " + std::to_string(systemIndex));
        system->Update(deltaTime);
        systemIndex++;
    }

    // Process events
    if (eventManager_) {
        eventManager_->DispatchEvents();
    }
}

void Engine::Render()
{
    // Reduce logging frequency to prevent memory pressure
    static int frameCount = 0;
    frameCount++;

    // Log only every 60 frames (once per second at 60 FPS) to reduce memory pressure
    if (frameCount % 60 == 0) {
        LOG_INFO("Engine::Render called (frame " + std::to_string(frameCount) + ")");
    }

    // Step 1: Find and call WorldRenderer for static world geometry
    for (auto& system : systems_) {
        WorldSystem* worldSystem = dynamic_cast<WorldSystem*>(system.get());
        if (worldSystem && worldSystem->GetWorldRenderer()) {
            if (frameCount % 60 == 0) {
                LOG_INFO("Found WorldRenderer; rendering static world geometry");
            }

            // Get the camera from the first available camera source
            Camera3D camera = {0};
            bool cameraFound = false;

            // Try to get camera from RenderSystem first
            for (auto& sys : systems_) {
                RenderSystem* renderSys = dynamic_cast<RenderSystem*>(sys.get());
                if (renderSys) {
                    camera = {
                        renderSys->GetRenderer()->GetCameraPosition(),
                        renderSys->GetRenderer()->GetCameraTarget(),
                        {0.0f, 1.0f, 0.0f},  // Up vector
                        renderSys->GetRenderer()->GetCameraZoom(),
                        CAMERA_PERSPECTIVE
                    };
                    cameraFound = true;
                    break;
                }
            }

            if (cameraFound) {
                worldSystem->GetWorldRenderer()->Render(camera);
                if (frameCount % 60 == 0) {
                    LOG_INFO("WorldRenderer completed static geometry rendering");
                }
            } else {
                if (frameCount % 60 == 0) {
                    LOG_WARNING("Could not find camera for WorldRenderer");
                }
            }
            break;
        }
    }

    // Step 2: Find and call RenderSystem for dynamic entities
    for (auto& system : systems_) {
        // Check if this is a RenderSystem by trying to cast
        RenderSystem* renderSystem = dynamic_cast<RenderSystem*>(system.get());
        if (renderSystem) {
            if (frameCount % 60 == 0) {
                LOG_INFO("Found RenderSystem; rendering dynamic entities");
            }
            // Call the Render method which handles the actual rendering
            renderSystem->Render();
            if (frameCount % 60 == 0) {
                LOG_INFO("RenderSystem completed dynamic entity rendering");
            }
            break;  // Only one render system should exist
        }
    }

    // Render state manager overlays (menus, HUD, etc.)
    if (stateManager_) {
        stateManager_->Render();
    }

    if (frameCount % 60 == 0) {
        LOG_DEBUG("Engine::Render() completed");
    }
}

Entity* Engine::CreateEntity()
{
    uint64_t id = GenerateEntityId();
    auto entity = std::make_unique<Entity>(id);
    Entity* rawPtr = entity.get();
    entities_[id] = std::move(entity);

    LOG_DEBUG("Created entity with ID: " + std::to_string(id));

    // Don't register with systems immediately - wait until components are added
    // This prevents signature matching issues where entities are checked before they have components

    return rawPtr;
}


void Engine::DestroyEntity(Entity* entity)
{
    if (!entity) {
        LOG_WARNING("Attempted to destroy null entity");
        return;
    }

    auto it = entities_.find(entity->GetId());
    if (it != entities_.end()) {
        // Remove entity from all systems
        for (auto& system : systems_) {
            system->RemoveEntity(entity);
        }

        entities_.erase(it);
        LOG_DEBUG("Destroyed entity with ID: " + std::to_string(entity->GetId()));
    } else {
        LOG_WARNING("Attempted to destroy non-existent entity with ID: " +
                   std::to_string(entity->GetId()));
    }
}

Entity* Engine::GetEntityById(uint64_t id) const
{
    auto it = entities_.find(id);
    return (it != entities_.end()) ? it->second.get() : nullptr;
}

void Engine::Clear()
{
    // Clear all systems first
    systems_.clear();

    // Then clear all entities
    entities_.clear();

    // Reset ID counter
    nextEntityId_ = 1;

    LOG_INFO("Engine cleared");
}

uint64_t Engine::GenerateEntityId()
{
    return nextEntityId_++;
}

void Engine::InitializeEventManager()
{
    if (!eventManager_) {
        eventManager_ = new EventManager();
        LOG_INFO("EventManager initialized");
    }
}

void Engine::InitializeStateManager()
{
    if (!stateManager_ && eventManager_) {
        stateManager_ = new StateManager(eventManager_);
        LOG_INFO("StateManager initialized");
    }
}


void Engine::RemoveSystem(System* system)
{
    if (!system) {
        LOG_WARNING("Attempted to remove null system");
        return;
    }

    auto it = std::find_if(systems_.begin(), systems_.end(),
        [system](const std::unique_ptr<System>& s) {
            return s.get() == system;
        });

    if (it != systems_.end()) {
        systems_.erase(it);
        LOG_DEBUG("Removed system from engine");
    } else {
        LOG_WARNING("Attempted to remove non-existent system");
    }
}

void Engine::UpdateEntityRegistration(Entity* entity)
{
    if (!entity) {
        LOG_WARNING("Attempted to update registration for null entity");
        return;
    }

    LOG_INFO("Engine::UpdateEntityRegistration - registering entity " + std::to_string(entity->GetId()) + " with " + std::to_string(systems_.size()) + " systems");

    // Check this entity against all systems
    for (auto& system : systems_) {
        LOG_DEBUG("Registering entity " + std::to_string(entity->GetId()) + " with system");
        system->AddEntity(entity);
    }

    LOG_INFO("Entity " + std::to_string(entity->GetId()) + " registration completed");
}
