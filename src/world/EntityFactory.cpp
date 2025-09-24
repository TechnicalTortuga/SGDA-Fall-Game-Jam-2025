#include "EntityFactory.h"
#include "../core/Engine.h"
#include "../ecs/Components/TransformComponent.h"
#include "../ecs/Components/GameObject.h"
#include "../ecs/Components/LightComponent.h"
#include "../ecs/Components/SpawnPointComponent.h"
#include "../ecs/Components/EnemyComponent.h"
#include "../ecs/Components/TriggerComponent.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Components/MaterialComponent.h"
#include "../ecs/Systems/MeshSystem.h"
#include "../ecs/Systems/WorldSystem.h"
#include "../ecs/Systems/CacheSystem.h"
#include "../utils/Logger.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>

EntityFactory::EntityFactory() {
    // Register default entity creators
    RegisterEntityCreator(GameObjectType::LIGHT_POINT, [this](const EntityDefinition& def) {
        return CreateLightEntity(def);
    });
    RegisterEntityCreator(GameObjectType::LIGHT_SPOT, [this](const EntityDefinition& def) {
        return CreateLightEntity(def);
    });
    RegisterEntityCreator(GameObjectType::LIGHT_DIRECTIONAL, [this](const EntityDefinition& def) {
        return CreateLightEntity(def);
    });
    RegisterEntityCreator(GameObjectType::AUDIO_SOURCE, [this](const EntityDefinition& def) {
        return CreateAudioEntity(def);
    });
    RegisterEntityCreator(GameObjectType::SPAWN_POINT, [this](const EntityDefinition& def) {
        return CreateSpawnPointEntity(def);
    });
    RegisterEntityCreator(GameObjectType::ENEMY, [this](const EntityDefinition& def) {
        return CreateEnemyEntity(def);
    });
    RegisterEntityCreator(GameObjectType::TRIGGER, [this](const EntityDefinition& def) {
        return CreateTriggerEntity(def);
    });
    RegisterEntityCreator(GameObjectType::WAYPOINT, [this](const EntityDefinition& def) {
        return CreateWaypointEntity(def);
    });
    RegisterEntityCreator(GameObjectType::STATIC_PROP, [this](const EntityDefinition& def) {
        return CreateStaticPropEntity(def);
    });
}

void EntityFactory::SetMaterials(const std::vector<MaterialInfo>& materials) {
    materialsMap_.clear();
    for (const auto& material : materials) {
        materialsMap_[material.id] = material;
        LOG_DEBUG("EntityFactory: Added material ID " + std::to_string(material.id) +
                 " with name '" + material.name + "' and diffuseMap '" + material.diffuseMap + "'");
    }
    LOG_INFO("EntityFactory: Loaded " + std::to_string(materialsMap_.size()) + " materials for entity creation");
}

const MaterialInfo* EntityFactory::GetMaterialById(int materialId) const {
    auto it = materialsMap_.find(materialId);
    return (it != materialsMap_.end()) ? &it->second : nullptr;
}

void EntityFactory::RegisterEntityCreator(GameObjectType type,
                                         std::function<Entity*(const EntityDefinition&)> creator) {
    creators_[type] = creator;
}

Entity* EntityFactory::CreateEntityFromDefinition(const EntityDefinition& definition) {
    LOG_INFO("EntityFactory: Creating entity of type '" + definition.className + "' with ID " + std::to_string(definition.id));
    // Engine is now a singleton, always available

    auto it = creators_.find(definition.type);
    if (it != creators_.end()) {
        try {
            Entity* entity = it->second(definition);
            LOG_INFO("EntityFactory: Created entity '" + definition.name +
                     "' of type " + definition.className + " (ID: " + std::to_string(entity->GetId()) + ")");
            return entity;
        } catch (const std::exception& e) {
            LOG_ERROR("EntityFactory: Failed to create entity '" + definition.name +
                      "' of type " + definition.className + ": " + std::string(e.what()));
            return nullptr;
        }
    } else {
        LOG_WARNING("EntityFactory: No creator registered for entity type '" +
                    definition.className + "' - creating as static prop");
        return CreateStaticPropEntity(definition);
    }
}

std::vector<Entity*> EntityFactory::CreateEntitiesFromDefinitions(const std::vector<std::unique_ptr<EntityDefinition>>& definitions) {
    std::vector<Entity*> entities;
    entities.reserve(definitions.size());

    for (const auto& definitionPtr : definitions) {
        if (definitionPtr) {
            Entity* entity = CreateEntityFromDefinition(*definitionPtr);
            if (entity) {
                entities.push_back(entity);
            }
        }
    }

    LOG_INFO("EntityFactory: Created " + std::to_string(entities.size()) +
             " entities from " + std::to_string(definitions.size()) + " definitions");
    return entities;
}

Entity* EntityFactory::CreateLightEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Add light-specific component - copy from parsed component data
    LightComponent* lightComp = entity->AddComponent<LightComponent>();
    lightComp->type = definition.light.type;
    lightComp->color = definition.light.color;
    lightComp->intensity = definition.light.intensity;
    lightComp->castShadows = definition.light.castShadows;
    lightComp->enabled = definition.light.enabled;
    lightComp->radius = definition.light.radius;
    lightComp->shadowBias = definition.light.shadowBias;
    lightComp->shadowResolution = definition.light.shadowResolution;
    lightComp->range = definition.light.range;
    lightComp->innerAngle = definition.light.innerAngle;
    lightComp->outerAngle = definition.light.outerAngle;
    lightComp->shadowMapSize = definition.light.shadowMapSize;
    lightComp->shadowCascadeCount = definition.light.shadowCascadeCount;
    lightComp->shadowDistance = definition.light.shadowDistance;

    return entity;
}

Entity* EntityFactory::CreateAudioEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Add audio-specific component - copy from parsed component data
    AudioComponent* audioComp = entity->AddComponent<AudioComponent>();
    audioComp->audioType = definition.audio.audioType;
    audioComp->clipPath = definition.audio.clipPath;
    audioComp->volume = definition.audio.volume;
    audioComp->pitch = definition.audio.pitch;
    audioComp->loop = definition.audio.loop;
    audioComp->playOnStart = definition.audio.playOnStart;
    audioComp->spatialBlend = definition.audio.spatialBlend;
    audioComp->minDistance = definition.audio.minDistance;
    audioComp->maxDistance = definition.audio.maxDistance;
    audioComp->rolloffMode = definition.audio.rolloffMode;
    audioComp->dopplerLevel = definition.audio.dopplerLevel;
    audioComp->spread = definition.audio.spread;
    audioComp->reverbZoneMix = definition.audio.reverbZoneMix;
    audioComp->priority = definition.audio.priority;
    audioComp->outputAudioMixerGroup = definition.audio.outputAudioMixerGroup;
    audioComp->audioName = definition.audio.audioName;

    return entity;
}

void EntityFactory::AddCollidableComponent(Entity* entity, const EntityDefinition& definition) {
    // Check if this entity has collider properties (size indicates collider is present)
    if (definition.collidable.size.x > 0.0f || definition.collidable.size.y > 0.0f || definition.collidable.size.z > 0.0f) {
        Collidable* collidable = entity->AddComponent<Collidable>(definition.collidable.size);
        collidable->SetCollisionLayer(definition.collidable.collisionLayer);
        collidable->SetCollisionMask(definition.collidable.collisionMask);
        collidable->SetStatic(definition.collidable.isStatic);
        collidable->SetTrigger(definition.collidable.isTrigger);

        LOG_INFO("Added Collidable component to entity: size=(" +
                std::to_string(definition.collidable.size.x) + "," +
                std::to_string(definition.collidable.size.y) + "," +
                std::to_string(definition.collidable.size.z) + ")");
    }
}

void EntityFactory::AddMeshComponent(Entity* entity, const EntityDefinition& definition) {
    // Check if this entity has mesh properties (non-empty mesh name or model path)
    if (!definition.mesh.meshName.empty() || !definition.mesh.modelPath.empty() ||
        definition.mesh.primitiveShape != "cube") {

        MeshComponent* meshComp = entity->AddComponent<MeshComponent>();
        meshComp->meshName = definition.mesh.meshName;
        meshComp->meshType = static_cast<MeshComponent::MeshType>(definition.mesh.type);
        meshComp->primitiveShape = definition.mesh.primitiveShape;
        meshComp->isStatic = definition.mesh.type != decltype(definition.mesh)::MeshType::MODEL; // Models might be dynamic
        meshComp->isActive = true;

        // If a material ID is specified, store it for later texture setting
        if (definition.mesh.materialId >= 0) {
            // Materials are now handled through MaterialComponent, not MeshSystem
                meshComp->materialEntityId = definition.mesh.materialId;
            }
        }

        // The actual mesh geometry creation will be handled by the MeshSystem
        // based on the type (model, primitive, composite) and parameters stored in the entity definition
        LOG_INFO("Added MeshComponent to entity: name='" + definition.mesh.meshName +
                "', type=" + std::to_string(static_cast<int>(definition.mesh.type)));
}

void EntityFactory::AddSpriteComponent(Entity* entity, const EntityDefinition& definition) {
    // Check if this entity has sprite properties (texture path indicates sprite is present)
    if (!definition.sprite.texturePath.empty()) {
        Sprite* spriteComp = entity->AddComponent<Sprite>(definition.sprite.texturePath);
        spriteComp->SetTint(definition.sprite.color);

        // Additional sprite properties would be set here if the Sprite component supported them
        // For now, basic texture loading is handled by the Sprite constructor
        LOG_INFO("Added Sprite component to entity: texture='" + definition.sprite.texturePath + "'");
    }
}

void EntityFactory::AddMaterialComponent(Entity* entity, const EntityDefinition& definition) {
    LOG_DEBUG("AddMaterialComponent called for entity '" + definition.name + "' (ID: " + std::to_string(entity->GetId()) + ")");

    // Get MaterialSystem for creating materials
    MaterialSystem* materialSystem = Engine::GetInstance().GetSystem<MaterialSystem>();
    if (!materialSystem) {
        LOG_ERROR("AddMaterialComponent: MaterialSystem not available");
        return;
    }

    MaterialProperties props;
    uint32_t materialId = 0;

    // 🎯 FIX: Check if entity has a material_id property to look up YAML material
    auto matIt = definition.properties.find("material_id");
    if (matIt != definition.properties.end()) {
        try {
            // Try different type casts since YAML parsing might store as string
            int yamlMaterialId = 0;
            try {
                yamlMaterialId = std::any_cast<int>(matIt->second);
                LOG_DEBUG("Material ID cast as int: " + std::to_string(yamlMaterialId));
            } catch (const std::bad_any_cast&) {
                try {
                    yamlMaterialId = static_cast<int>(std::any_cast<float>(matIt->second));
                    LOG_DEBUG("Material ID cast as float->int: " + std::to_string(yamlMaterialId));
                } catch (const std::bad_any_cast&) {
                    std::string materialIdStr = std::any_cast<std::string>(matIt->second);
                    yamlMaterialId = std::stoi(materialIdStr);
                    LOG_DEBUG("Material ID cast as string->int: " + std::to_string(yamlMaterialId));
                }
            }
            
            const MaterialInfo* materialInfo = GetMaterialById(yamlMaterialId);
            
            if (materialInfo) {
                LOG_INFO("🎨 ENTITY USING YAML MATERIAL:");
                LOG_INFO("  YAML Material ID: " + std::to_string(yamlMaterialId));
                LOG_INFO("  Material name: " + materialInfo->name);
                LOG_INFO("  Diffuse map: " + materialInfo->diffuseMap);
                
                // Set material properties from YAML material info (like CreateStaticPropEntity does)
                props.primaryColor = materialInfo->diffuseColor;
                props.secondaryColor = BLACK;
                props.specularColor = materialInfo->specularColor;
                props.shininess = materialInfo->shininess;
                props.alpha = materialInfo->alpha;
                props.roughness = materialInfo->roughness;
                props.metallic = materialInfo->metallic;
                props.ao = materialInfo->ao;
                props.emissiveColor = materialInfo->emissiveColor;
                props.emissiveIntensity = materialInfo->emissiveIntensity;

                // Set material type
                if (materialInfo->type == "PBR") {
                    props.type = CachedMaterialData::MaterialType::PBR;
                } else if (materialInfo->type == "UNLIT") {
                    props.type = CachedMaterialData::MaterialType::UNLIT;
                } else if (materialInfo->type == "EMISSIVE") {
                    props.type = CachedMaterialData::MaterialType::EMISSIVE;
                } else if (materialInfo->type == "TRANSPARENT") {
                    props.type = CachedMaterialData::MaterialType::TRANSPARENT;
                } else {
                    props.type = CachedMaterialData::MaterialType::BASIC;
                }

                // 🎨 KEY FIX: Set texture maps from YAML material
                props.diffuseMap = materialInfo->diffuseMap;
                if (props.diffuseMap.empty()) {
                    props.diffuseMap = materialInfo->name; // Fallback to name
                }
                props.normalMap = materialInfo->normalMap;
                props.specularMap = materialInfo->specularMap;
                props.roughnessMap = materialInfo->roughnessMap;
                props.metallicMap = materialInfo->metallicMap;
                props.aoMap = materialInfo->aoMap;
                props.emissiveMap = materialInfo->emissiveMap;

                // Set rendering flags
                props.doubleSided = materialInfo->doubleSided;
                props.depthWrite = true;
                props.depthTest = true;
                props.castShadows = true;

                // Set material name
                props.materialName = "entity_" + std::to_string(yamlMaterialId) + "_" + materialInfo->name;

                LOG_INFO("✅ CREATED TEXTURED MATERIAL from YAML for entity");
            } else {
                LOG_WARNING("❌ Material ID " + std::to_string(yamlMaterialId) + " not found in YAML, using solid color fallback");
                // Fall through to solid color creation below
            }
        } catch (const std::exception& e) {
            LOG_WARNING("❌ Invalid material_id property, using solid color fallback: " + std::string(e.what()));
            // Fall through to solid color creation below
        }
    }

    // If no YAML material found, create solid color or gradient material
    if (props.materialName.empty()) {
        // Check if this entity uses gradient colors
        if (definition.material.colorMode == decltype(definition.material)::ColorMode::GRADIENT) {
            LOG_INFO("🎨 ENTITY USING GRADIENT MATERIAL");

            // Set gradient properties - use gradientStart and gradientEnd directly
            props.primaryColor = definition.material.gradientStart;  // Start color (Blue)
            props.secondaryColor = definition.material.gradientEnd;   // End color (Yellow)
            props.specularColor = WHITE;
            props.shininess = definition.material.shininess;
            props.alpha = 1.0f;

            // Set PBR defaults
            props.roughness = 0.5f;
            props.metallic = 0.0f;
            props.ao = 1.0f;
            props.emissiveColor = BLACK;
            props.emissiveIntensity = 1.0f;

            props.type = CachedMaterialData::MaterialType::BASIC;

            props.materialName = "entity_gradient_" + std::to_string(entity->GetId());

            LOG_INFO("✅ CREATED GRADIENT MATERIAL: " + props.materialName +
                     " (" + std::to_string((int)props.primaryColor.r) + "," + std::to_string((int)props.primaryColor.g) + "," + std::to_string((int)props.primaryColor.b) +
                     ") -> (" + std::to_string((int)props.secondaryColor.r) + "," + std::to_string((int)props.secondaryColor.g) + "," + std::to_string((int)props.secondaryColor.b) + ")");
        } else {
            LOG_INFO("🎨 ENTITY USING SOLID COLOR MATERIAL (no material_id or YAML lookup failed)");

            // Set basic properties from definition
            props.primaryColor = definition.material.diffuseColor;
            props.secondaryColor = BLACK; // No gradient
            props.specularColor = WHITE;
            props.shininess = definition.material.shininess;
            props.alpha = 1.0f;

            // Set PBR defaults
            props.roughness = 0.5f;
            props.metallic = 0.0f;
            props.ao = 1.0f;
            props.emissiveColor = BLACK;
            props.emissiveIntensity = 1.0f;

            props.type = CachedMaterialData::MaterialType::BASIC;

            props.materialName = "entity_solid_" + std::to_string(entity->GetId());
        }

        // Set rendering flags
        props.doubleSided = false;
        props.depthWrite = true;
        props.depthTest = true;
        props.castShadows = true;
    }

    // Create material through MaterialSystem
    materialId = materialSystem->GetOrCreateMaterial(props);

    // Add MaterialComponent with the material ID
    MaterialComponent* materialComp = entity->AddComponent<MaterialComponent>(materialId);

    // Set gradient mode based on colorMode (for solid color materials)
    if (definition.material.colorMode == decltype(definition.material)::ColorMode::GRADIENT) {
        materialComp->SetLinearGradient();
    } else {
        materialComp->SetSolidColor();
    }

    LOG_INFO("✅ ADDED MaterialComponent to entity:");
    LOG_INFO("  System Material ID: " + std::to_string(materialId));
    LOG_INFO("  Material name: " + props.materialName);
    LOG_INFO("  Diffuse map: " + (props.diffuseMap.empty() ? "NONE (solid color)" : props.diffuseMap));
    LOG_INFO("  Gradient mode: " + std::to_string(materialComp->GetGradientMode()));
}

Entity* EntityFactory::CreateSpawnPointEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Add spawn point component
    SpawnPointComponent* spawnComp = entity->AddComponent<SpawnPointComponent>();
    spawnComp->type = definition.spawnPoint.type;
    spawnComp->team = definition.spawnPoint.team;
    spawnComp->priority = definition.spawnPoint.priority;
    spawnComp->cooldownTime = definition.spawnPoint.cooldownTime;
    spawnComp->enabled = true;

    return entity;
}

Entity* EntityFactory::CreateEnemyEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Add enemy component
    EnemyComponent* enemyComp = entity->AddComponent<EnemyComponent>();
    enemyComp->type = definition.enemy.type;
    enemyComp->health = definition.enemy.health;
    enemyComp->maxHealth = definition.enemy.health; // Start with full health
    enemyComp->damage = definition.enemy.damage;
    enemyComp->moveSpeed = definition.enemy.moveSpeed;
    enemyComp->team = definition.enemy.team;

    return entity;
}

Entity* EntityFactory::CreateTriggerEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Add trigger component
    TriggerComponent* triggerComp = entity->AddComponent<TriggerComponent>();
    triggerComp->type = definition.trigger.type;
    triggerComp->size = definition.trigger.size;
    triggerComp->radius = definition.trigger.radius;
    triggerComp->height = definition.trigger.height;
    triggerComp->maxActivations = definition.trigger.maxActivations;
    triggerComp->enabled = true;

    return entity;
}

Entity* EntityFactory::CreateWaypointEntity(const EntityDefinition& definition) {
    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Waypoints are primarily just game objects with transform
    // Additional waypoint logic would be handled by navigation systems

    return entity;
}

Entity* EntityFactory::CreateStaticPropEntity(const EntityDefinition& definition) {
    LOG_INFO("CreateStaticPropEntity: Creating entity '" + definition.name + "' with ID " + std::to_string(definition.id) + ", property count: " + std::to_string(definition.properties.size()));

    // Log all properties for debugging
    for (const auto& prop : definition.properties) {
        try {
            std::string valueStr = "[unknown]";
            if (prop.second.type() == typeid(int)) {
                valueStr = std::to_string(std::any_cast<int>(prop.second));
            } else if (prop.second.type() == typeid(float)) {
                valueStr = std::to_string(std::any_cast<float>(prop.second));
            } else if (prop.second.type() == typeid(std::string)) {
                valueStr = std::any_cast<std::string>(prop.second);
            }
            LOG_DEBUG("CreateStaticPropEntity: Property '" + prop.first + "' = " + valueStr);
        } catch (const std::bad_any_cast&) {
            LOG_DEBUG("CreateStaticPropEntity: Property '" + prop.first + "' = [bad_cast]");
        }
    }

    Entity* entity = Engine::GetInstance().CreateEntity();

    // Setup common components
    SetupTransformComponents(entity, definition);
    SetupGameObjectComponent(entity, definition);

    // Check if this static prop has mesh properties
    LOG_INFO("CreateStaticPropEntity: Checking properties for entity '" + definition.name + "', property count: " + std::to_string(definition.properties.size()));
    for (const auto& prop : definition.properties) {
        LOG_INFO("  Property: " + prop.first);
    }

    // Add collidable component if specified
    AddCollidableComponent(entity, definition);

    // Add mesh component if specified
    AddMeshComponent(entity, definition);

    // Add sprite component if specified
    AddSpriteComponent(entity, definition);

    // Add material component (always add it for rendering)
    AddMaterialComponent(entity, definition);

    auto meshTypeIt = definition.properties.find("mesh_type");
    if (meshTypeIt != definition.properties.end()) {
        LOG_INFO("CreateStaticPropEntity: Found mesh_type property");
        std::string meshType = std::any_cast<std::string>(meshTypeIt->second);

        // Get MeshSystem
        auto meshSystem = Engine::GetInstance().GetSystem<MeshSystem>();
        if (meshSystem) {
            // 🔧 FIX: Don't create a new MeshComponent, use the existing one from AddMeshComponent
            auto meshComp = entity->GetComponent<MeshComponent>();
            if (!meshComp) {
                LOG_ERROR("CreateStaticPropEntity: No MeshComponent found after AddMeshComponent call");
                return entity;
            }

            LOG_INFO("🛠️ CREATING MESH: type='" + meshType + "' for entity '" + definition.name + "'");

            if (meshType == "cube") {
                // Try to extract size with error handling
                float size = 1.0f; // Default
                auto sizeIt = definition.properties.find("size");
                if (sizeIt != definition.properties.end()) {
                    try {
                        size = std::any_cast<float>(sizeIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            size = static_cast<float>(std::any_cast<int>(sizeIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast size as float or int for cube");
                        }
                    }
                }
                
                Color color = WHITE; // Default white, will be overridden by material
                meshSystem->CreateCube(entity, size, color);
            } else if (meshType == "sphere") {
                float radius = 1.0f;
                auto radiusIt = definition.properties.find("radius");
                if (radiusIt != definition.properties.end()) {
                    try {
                        radius = std::any_cast<float>(radiusIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            radius = static_cast<float>(std::any_cast<int>(radiusIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast radius for sphere");
                        }
                    }
                }
                meshSystem->CreateSphere(entity, radius);
            } else if (meshType == "capsule") {
                float radius = 0.5f;
                float height = 2.0f;
                auto radiusIt = definition.properties.find("radius");
                if (radiusIt != definition.properties.end()) {
                    try {
                        radius = std::any_cast<float>(radiusIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            radius = static_cast<float>(std::any_cast<int>(radiusIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast radius for capsule");
                        }
                    }
                }
                auto heightIt = definition.properties.find("height");
                if (heightIt != definition.properties.end()) {
                    try {
                        height = std::any_cast<float>(heightIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            height = static_cast<float>(std::any_cast<int>(heightIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast height for capsule");
                        }
                    }
                }
                meshSystem->CreateCapsule(entity, radius, height);
            } else if (meshType == "cylinder") {
                float radius = 1.0f;
                float height = 2.0f;
                auto radiusIt = definition.properties.find("radius");
                if (radiusIt != definition.properties.end()) {
                    try {
                        radius = std::any_cast<float>(radiusIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            radius = static_cast<float>(std::any_cast<int>(radiusIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast radius for cylinder");
                        }
                    }
                }
                auto heightIt = definition.properties.find("height");
                if (heightIt != definition.properties.end()) {
                    try {
                        height = std::any_cast<float>(heightIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            height = static_cast<float>(std::any_cast<int>(heightIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast height for cylinder");
                        }
                    }
                }
                meshSystem->CreateCylinder(entity, radius, height);
            } else if (meshType == "pyramid") {
                // Try to extract size with error handling
                float size = 1.0f; // Default
                auto sizeIt = definition.properties.find("size");
                if (sizeIt != definition.properties.end()) {
                    try {
                        size = std::any_cast<float>(sizeIt->second);
                    } catch (const std::bad_any_cast&) {
                        try {
                            size = static_cast<float>(std::any_cast<int>(sizeIt->second));
                        } catch (const std::bad_any_cast&) {
                            LOG_WARNING("CreateStaticPropEntity: Failed to cast size as float or int for pyramid");
                        }
                    }
                }
                
                float height = size * 1.5f; // Make height 1.5x base size
                std::vector<Color> faceColors = {WHITE, WHITE, WHITE, WHITE}; // Default white faces
                meshSystem->CreatePyramid(entity, size, height, faceColors);
            }
        }
    }

    // 🔧 REMOVED: Duplicate material creation logic - handled by AddMaterialComponent() now

    return entity;
}

void EntityFactory::SetupTransformComponents(Entity* entity, const EntityDefinition& definition) {
    // Add transform component
    TransformComponent* transform = entity->AddComponent<TransformComponent>();
    transform->position = definition.position;
    transform->rotation = definition.rotation;
    transform->scale = definition.scale;
    transform->isActive = true;
}

void EntityFactory::SetupGameObjectComponent(Entity* entity, const EntityDefinition& definition) {
    // Add game object component
    GameObject* gameObj = entity->AddComponent<GameObject>();
    gameObj->type = definition.type;
    gameObj->name = definition.name;
    gameObj->className = definition.className;
    gameObj->enabled = true;

    // Copy properties
    gameObj->properties = definition.properties;

    // Copy tags if any were specified in the definition
    // (This could be extended to parse tags from the map format)
}

Engine& EntityFactory::GetEngine() const {
    return Engine::GetInstance();
}
