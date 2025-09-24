#include "WorldSystem.h"
#include "RenderSystem.h"
#include "../../rendering/Skybox.h"

#include "../Entity.h"
#include "../Components/Position.h"
#include "../Components/Sprite.h"
#include "../Components/Collidable.h"
#include "../Components/Velocity.h"
#include "../Components/TransformComponent.h"
#include "../Systems/MeshSystem.h"
#include "../Systems/LODSystem.h"
#include "../Systems/CacheSystem.h"
#include "../../core/Engine.h"
#include "../../rendering/TextureManager.h"
#include "../../utils/Logger.h"
#include "../../utils/PathUtils.h"
#include "../../world/EntityFactory.h"
#include "../Systems/GameObjectSystem.h"
#include "../Systems/LightSystem.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <filesystem>
#include <fstream>

WorldSystem::WorldSystem()
    : collisionSystem_(nullptr), renderSystem_(nullptr), bspTreeSystem_(nullptr)
    , worldGeometry_(std::make_unique<WorldGeometry>())
    , mapLoader_()
    , mapLoaded_(false)
    , texturesNeedLoading_(false)
    , entityFactory_(std::make_unique<EntityFactory>())
{
    LOG_INFO("WorldSystem constructor called");
}

void WorldSystem::Initialize() {
    LOG_INFO("WorldSystem initialize called");

    // EntityFactory uses singleton Engine
    LOG_INFO("WorldSystem: EntityFactory initialized");

    // Export geometry for development (one-time operation)
    static bool geometryExported = false;
    if (!geometryExported) {
        ExportGeometryToFile();
        geometryExported = true;
    }

    // Initialize WorldGeometry (creates skybox and other resources)
    if (worldGeometry_) {
        // Set AssetSystem for skybox loading
        auto assetSystem = engine_.GetSystem<AssetSystem>();
        if (assetSystem) {
            worldGeometry_->SetAssetSystem(assetSystem);
            LOG_INFO("WorldGeometry AssetSystem set for skybox loading");
        }

        worldGeometry_->Initialize();
        LOG_INFO("WorldGeometry initialized with skybox support");
    }

    // Get system references
    collisionSystem_ = engine_.GetSystem<CollisionSystem>();
    bspTreeSystem_ = engine_.GetSystem<BSPTreeSystem>();
    // renderSystem_ setup moved to Engine for unified rendering

    // EntityFactory is now managed globally by Engine

    if (collisionSystem_) {
        LOG_INFO("WorldSystem acquired CollisionSystem reference");
    } else {
        LOG_WARNING("WorldSystem could not acquire CollisionSystem reference");
    }
    
    if (bspTreeSystem_) {
        LOG_INFO("WorldSystem acquired BSPTreeSystem reference");
    } else {
        LOG_WARNING("WorldSystem could not acquire BSPTreeSystem reference");
    }

    // Load the map but defer texture loading until AssetSystem is ready
    if (LoadDefaultMap()) {
        LOG_INFO("WorldSystem initialized (stage vs actors architecture) - textures will load later");
    } else {
        LOG_ERROR("Failed to load default map during WorldSystem initialization");
    }

    // Mark that textures need loading when AssetSystem becomes available
    texturesNeedLoading_ = true;
}

bool WorldSystem::LoadDefaultMap() {
    LOG_INFO("Loading default test map");

    // Unload current map
    UnloadMap();

    // Load the YAML test map file
    std::string exeDir = Utils::GetExecutableDir();
    std::string testMapPath = exeDir + "/assets/maps/test_level_yaml.map";
    MapData mapData = mapLoader_.LoadMap(testMapPath);

    // If that fails, try CWD-relative
    if (mapData.entities.empty()) {
        LOG_WARNING("Failed to load YAML map from exe-relative path, trying CWD-relative");
        testMapPath = "assets/maps/test_level_yaml.map";
        mapData = mapLoader_.LoadMap(testMapPath);
    }

    // Try to load YAML map first
    if (!mapData.entities.empty()) {
        // YAML entities loaded successfully
        if (mapData.faces.empty()) {
            LOG_ERROR("YAML has entities but no geometry - this shouldn't happen with the new map");
            return false;
        } else {
            LOG_INFO("YAML map loaded with geometry from file");
        }

        LOG_INFO("YAML map loaded (faces: " + std::to_string(mapData.faces.size()) +
                 ", entities: " + std::to_string(mapData.entities.size()) + ")");
    } else {
        LOG_ERROR("YAML map loading failed - falling back to programmatic creation");
        // Fallback to programmatic creation
        mapData = CreateTestMap();
        if (mapData.faces.empty()) {
            LOG_ERROR("Programmatic creation also failed - No surfaces found");
            return false;
        }
        LOG_INFO("Using programmatic test map (faces: " + std::to_string(mapData.faces.size()) +
                 ", entities: " + std::to_string(mapData.entities.size()) + ")");
    }

    // Process the map data
    ProcessMapData(mapData);
    mapLoaded_ = true;
    LOG_INFO("Programmatic test map created and processed successfully");
    return true;
}

void WorldSystem::Shutdown() {
    UnloadMap();
    LOG_INFO("WorldSystem shutdown complete");
}

void WorldSystem::Update(float deltaTime) {
    // Check if we need to load textures (deferred from initialization)
    static int updateCount = 0;
    updateCount++;

    if (updateCount % 60 == 0) { // Log every second at 60fps
        LOG_INFO("WorldSystem::Update: texturesNeedLoading_=" + std::string(texturesNeedLoading_ ? "true" : "false") +
                 ", mapLoaded_=" + std::string(mapLoaded_ ? "true" : "false"));
    }

    // Materials are now loaded upfront during map processing, no deferred loading needed

    // Update dynamic world elements

    // Rotate only the pyramid entity (in Room 3, X=-15), not the cube (in Room 2, X=21)
    for (Entity* entity : dynamicEntities_) {
        if (entity && entity->IsActive()) {
            auto position = entity->GetComponent<Position>();
            auto transform = entity->GetComponent<TransformComponent>();
            if (transform && position) {
                // Only rotate entities in Room 3 (pyramid), not Room 2 (cube)
                if (position->GetX() < 0.0f) { // Room 3 is at negative X coordinates
                    // Rotate at 90 degrees per second around Y axis (PI/2 radians per second)
                    float rotationSpeed = PI / 2.0f; // 90 degrees per second in radians
                    Quaternion rotationDelta = QuaternionFromAxisAngle({0, 1, 0}, rotationSpeed * deltaTime);
                    transform->rotation = QuaternionNormalize(QuaternionMultiply(transform->rotation, rotationDelta));
                    
                    // Debug: Log rotation every 60 frames to check if it's working
                    static int frameCount = 0;
                    frameCount++;
                    if (frameCount % 60 == 0) {
                        LOG_INFO("🔄 Pyramid rotation - deltaTime: " + std::to_string(deltaTime) + 
                                ", rotSpeed: " + std::to_string(rotationSpeed * deltaTime) + 
                                ", quat: (" + std::to_string(transform->rotation.x) + ", " + 
                                std::to_string(transform->rotation.y) + ", " + 
                                std::to_string(transform->rotation.z) + ", " + 
                                std::to_string(transform->rotation.w) + ")");
                    }
                }
            }
        }
    }
}

void WorldSystem::Render() {
    // World rendering is handled by the RenderSystem
    // This could be used for debug visualization
}

bool WorldSystem::LoadMap(const std::string& mapPath) {
    LOG_INFO("Loading map: " + mapPath);

    // Unload current map
    UnloadMap();

    // Load and parse map file into raw MapData with path resolution
    MapData mapData = mapLoader_.LoadMap(mapPath);

    // If direct path fails, try executable-relative
    if (mapData.faces.empty()) {
        std::string exeDir = Utils::GetExecutableDir();
        std::string exeRelativePath = exeDir + "/" + mapPath;
        LOG_WARNING("Direct map path failed, trying exe-relative: " + exeRelativePath);
        mapData = mapLoader_.LoadMap(exeRelativePath);
    }

    if (mapData.faces.empty()) {
        LOG_WARNING("Failed to load map from file: " + mapPath + " - falling back to programmatic creation");
        // TEMPORARY: Fall back to programmatic creation for testing
        mapData = CreateTestMap();
        if (mapData.faces.empty()) {
            LOG_ERROR("Programmatic creation also failed - No surfaces found");
            return false;
        }
        LOG_INFO("Using programmatic test map (faces: " + std::to_string(mapData.faces.size()) + ")");
    }

    // Process the raw MapData through the new building pipeline
    ProcessMapData(mapData);
    mapLoaded_ = true;

    // Resolve any pending textures for meshes
    LOG_INFO("About to get MeshSystem for texture resolution");
    auto meshSystem = engine_.GetSystem<MeshSystem>();
    if (meshSystem) {
        LOG_INFO("Found MeshSystem, calling ResolvePendingTextures");
        meshSystem->ResolvePendingTextures();
        LOG_INFO("ResolvePendingTextures completed");
    } else {
        LOG_ERROR("MeshSystem not found when trying to resolve pending textures");
        // Try to log all available systems
        LOG_ERROR("Available systems:");
        // We can't easily log all systems here, but this should help debug
    }

    LOG_INFO("Map loaded successfully from: " + mapPath +
             " (Faces: " + std::to_string(mapData.faces.size()) +
             ", Dynamic Entities: " + std::to_string(dynamicEntities_.size()) + ")");

    // Connect collision system with the newly loaded BSP tree
    if (collisionSystem_ && worldGeometry_) {
        collisionSystem_->SetWorld(worldGeometry_->GetWorld());
        LOG_INFO("Collision system connected to BSP tree after map loading");
    }
    // BSP and Renderer setup moved to Engine for unified rendering

    return true;
}


void WorldSystem::UnloadMap() {
    if (mapLoaded_) {
        DestroyDynamicEntities();
        worldGeometry_->Clear();
        
        // Clear all caches to free resources during map unload
        if (renderSystem_) {
            auto* renderer = renderSystem_->GetRenderer();
            if (renderer) {
                // Clear model cache
                auto* modelCache = renderer->GetModelCache();
                if (modelCache) {
                    modelCache->Clear();
                    LOG_INFO("Cleared model cache during map unload");
                }
            }
        } else {
            LOG_WARNING("RenderSystem not connected - model cache not cleared during map unload");
        }
        
        // Clear material cache (consistent with model cache pattern)
        auto* materialSystem = engine_.GetSystem<MaterialSystem>();
        if (materialSystem) {
            auto* materialCache = materialSystem->GetMaterialCache();
            if (materialCache) {
                materialCache->Clear();
                LOG_INFO("Cleared material cache during map unload");
            }
        } else {
            LOG_WARNING("MaterialSystem not available - material cache not cleared during map unload");
        }
        
        mapLoaded_ = false;
        LOG_INFO("Map unloaded - WorldGeometry, dynamic entities, and Model cache cleared");
    }
}

// NEW ARCHITECTURE: Main processing pipeline
void WorldSystem::ProcessMapData(MapData& mapData) {
    LOG_INFO("Processing MapData through UNIFIED pipeline...");

    // Step 0: SKIP material validation for now - it's breaking texture paths
    LOG_INFO("ProcessMapData: SKIPPING material validation (debug mode - MaterialValidator replaces texture paths)");
    /*
    auto validationResult = materialValidator_.ValidateMaterials(mapData);
    
    if (!validationResult.isValid) {
        LOG_WARNING("ProcessMapData: Material validation found issues, attempting repairs");
        materialValidator_.RepairInvalidMaterials(mapData, validationResult);
        materialValidator_.AssignFallbackTextures(mapData, validationResult);
        
        // Re-validate after repairs
        auto revalidationResult = materialValidator_.ValidateMaterials(mapData);
        if (!revalidationResult.isValid) {
            LOG_ERROR("ProcessMapData: Material validation still failing after repairs");
        } else {
            LOG_INFO("ProcessMapData: Material validation passed after repairs");
        }
    } else {
        LOG_INFO("ProcessMapData: Material validation passed without issues");
    }
    */

    // Step 1: Load materials using existing ECS system
    LOG_INFO("ProcessMapData: Loading materials through existing ECS system");
    LoadTexturesAndMaterials(mapData);

    // Step 2: Build the WorldGeometry (static world data) with unified materials
    LOG_INFO("ProcessMapData: Calling BuildWorldGeometry");
    BuildWorldGeometry(mapData);

    // Step 3: Build BSP tree now that materials are loaded and faces have materialEntityId
    LOG_INFO("ProcessMapData: Calling BuildBSPTreeAfterMaterials");
    BuildBSPTreeAfterMaterials();

    // Step 4: Create render batches
    LOG_INFO("ProcessMapData: Calling CreateRenderBatches");
    CreateRenderBatches(mapData);

    // Step 4: Setup skybox
    LOG_INFO("ProcessMapData: Calling SetupSkybox");
    SetupSkybox(mapData);

    // Step 5: Create dynamic entities immediately
    LOG_INFO("ProcessMapData: Calling CreateDynamicEntitiesFromMap");
    CreateDynamicEntitiesFromMap(mapData);

    LOG_INFO("MapData processing complete");
}

// NEW: Build WorldGeometry from MapData
void WorldSystem::BuildWorldGeometry(MapData& mapData) {
    LOG_INFO("Building WorldGeometry from MapData");

    // Clear any existing data
    worldGeometry_->Clear();

    // Set basic level info
    worldGeometry_->SetLevelName(mapData.name);
    worldGeometry_->SetSkyColor(mapData.skyColor);

    // Initialize materials map with default WorldMaterial objects for each material ID used in faces
    usedMaterialIds_.clear(); // Clear previous material IDs
    if (!mapData.faces.empty()) {
        // Create a mutable copy of faces to allow modification
        std::vector<Face> mutableFaces = mapData.faces;
        for (auto& face : mutableFaces) {
            // Verify material exists in mapping
            auto it = materialIdMap_.find(face.materialId);
            if (it != materialIdMap_.end()) {
                // Material exists, keep the materialId as-is
                LOG_DEBUG("Face verified materialId " + std::to_string(face.materialId) + " exists in registry");
            } else {
                LOG_WARNING("No material found for materialId " + std::to_string(face.materialId) +
                           " during geometry creation - using fallback material 0");
                face.materialId = 0; // Fallback to first material
            }

            if (face.materialId >= 0) {
                usedMaterialIds_.insert(face.materialId);
            }
        }

        // Update the mapData with the modified faces
        mapData.faces = mutableFaces;

        // Set the faces with materialEntityIds in WorldGeometry
        worldGeometry_->faces = mapData.faces;
    } else if (!mapData.brushes.empty()) {
        // Create a mutable copy of brushes to allow face modification
        std::vector<Brush> mutableBrushes = mapData.brushes;
        for (auto& brush : mutableBrushes) {
            for (auto& face : brush.faces) {
                // Verify material exists in mapping
                auto it = materialIdMap_.find(face.materialId);
                if (it != materialIdMap_.end()) {
                    // Material exists, keep the materialId as-is
                    LOG_DEBUG("Brush face verified materialId " + std::to_string(face.materialId) + " exists in registry");
                } else {
                    LOG_WARNING("No material found for brush face materialId " + std::to_string(face.materialId) +
                               " during geometry creation - using fallback material 0");
                    face.materialId = 0; // Fallback to first material
                }

                if (face.materialId >= 0) {
                    usedMaterialIds_.insert(face.materialId);
                }
            }
        }

        // Update the mapData with the modified brushes
        mapData.brushes = mutableBrushes;
    }

    // Material ID mappings will be set up when materials are loaded in LoadTexturesAndMaterials
    // No need to pre-initialize with defaults here

    // Debug: Show how many faces got each material ID
    std::map<int, int> materialCounts;
    for (const auto& face : mapData.faces) {
        materialCounts[face.materialId]++;
    }
    for (const auto& brush : mapData.brushes) {
        for (const auto& face : brush.faces) {
            materialCounts[face.materialId]++;
        }
    }

    for (const auto& [materialId, count] : materialCounts) {
        LOG_INFO("Material ID " + std::to_string(materialId) + ": " + std::to_string(count) + " faces");
    }


    LOG_INFO("Initialized " + std::to_string(usedMaterialIds_.size()) + " materials in WorldGeometry");

    // Create and build BSP tree (prefer brushes; otherwise faces)
    if (!mapData.brushes.empty()) {
        // Flatten brushes to faces for BSP building
        std::vector<Face> flattenedFaces;
        for (const auto& brush : mapData.brushes) {
            for (const auto& face : brush.faces) {
                flattenedFaces.push_back(face);
            }
        }

        LOG_INFO("BuildWorldGeometry: Building BSP tree from brushes with " + std::to_string(flattenedFaces.size()) + " faces");

        // BSP tree will be built after material assignment

        worldGeometry_->BuildBSPFromBrushes(mapData.brushes);
        size_t faceCount = 0; for (const auto& b : mapData.brushes) faceCount += b.faces.size();
        LOG_INFO("WorldGeometry built from brushes with ~" + std::to_string(faceCount) + " faces");
    } else if (!mapData.faces.empty()) {
        LOG_INFO("BuildWorldGeometry: Processed " + std::to_string(mapData.faces.size()) + " faces with material assignment");

        // BSP tree will be built later in the pipeline after materials are loaded
        LOG_INFO("BuildWorldGeometry: BSP tree building deferred until after material loading");
    } else {
        LOG_WARNING("No brushes or faces in MapData; WorldGeometry will be empty");
    }
}

// NEW: Create render batches (placeholder for now)
void WorldSystem::CreateRenderBatches(const MapData& mapData) {
    LOG_INFO("Creating render batches");

    // Placeholder: batching for faces (future)
    std::unordered_map<int, int> faceCounts;
    for (const auto& f : mapData.faces) {
        faceCounts[f.materialId]++;
    }
    LOG_INFO("Counted faces across materials: " + std::to_string(faceCounts.size()) + " groups");
}

// Load textures and create materials using AssetSystem
void WorldSystem::LoadTexturesAndMaterials(const MapData& mapData) {
    LOG_INFO("LoadTexturesAndMaterials using AssetSystem for map: " + mapData.name);

    // Get MaterialSystem for material management
    MaterialSystem* materialSystem = engine_.GetSystem<MaterialSystem>();
    LOG_DEBUG("MaterialSystem retrieved from engine: " + std::string(materialSystem ? "valid" : "null"));

    // Get AssetSystem for texture management
    AssetSystem* assetSystem = engine_.GetSystem<AssetSystem>();
    LOG_DEBUG("AssetSystem retrieved from engine: " + std::string(assetSystem ? "valid" : "null"));

    // Clear existing material ID mapping
    materialIdMap_.clear();

    LOG_INFO("Processing " + std::to_string(mapData.materials.size()) + " materials from map file");

    if (mapData.materials.empty()) {
        LOG_ERROR("No materials found in map file! World geometry will not have materials.");
        return;
    }

    for (const auto& materialInfo : mapData.materials) {
        LOG_INFO("Processing material: id=" + std::to_string(materialInfo.id) +
                 ", name='" + materialInfo.name + "', type='" + materialInfo.type + "'");
        LOG_DEBUG("Material properties - diffuseColor: (" +
                 std::to_string(materialInfo.diffuseColor.r) + "," + std::to_string(materialInfo.diffuseColor.g) + "," +
                 std::to_string(materialInfo.diffuseColor.b) + "," + std::to_string(materialInfo.diffuseColor.a) + ")");

        if (materialInfo.id < 0) {
            LOG_WARNING("Skipping material with invalid ID: " + std::to_string(materialInfo.id));
            continue;
        }

        // Create MaterialProperties for MaterialSystem
        MaterialProperties props;

        // Set basic material properties
        props.primaryColor = materialInfo.diffuseColor;  // Map diffuse to primary (for solid/gradients)
        props.secondaryColor = BLACK;  // Default secondary for gradients
        props.specularColor = materialInfo.specularColor;
        props.shininess = materialInfo.shininess;
        props.alpha = materialInfo.alpha;

        // Set PBR properties
        props.roughness = materialInfo.roughness;
        props.metallic = materialInfo.metallic;
        props.ao = materialInfo.ao;

        // Set emission properties
        props.emissiveColor = materialInfo.emissiveColor;
        props.emissiveIntensity = materialInfo.emissiveIntensity;

        // Set material type
        if (materialInfo.type == "PBR") {
            props.type = CachedMaterialData::MaterialType::PBR;
        } else if (materialInfo.type == "UNLIT") {
            props.type = CachedMaterialData::MaterialType::UNLIT;
        } else if (materialInfo.type == "EMISSIVE") {
            props.type = CachedMaterialData::MaterialType::EMISSIVE;
        } else if (materialInfo.type == "TRANSPARENT") {
            props.type = CachedMaterialData::MaterialType::TRANSPARENT;
        } else {
            props.type = CachedMaterialData::MaterialType::BASIC;
        }

        // Set texture maps - in YAML format, texture path is stored in diffuseMap field after parsing
        LOG_DEBUG("MaterialInfo diffuseMap: '" + materialInfo.diffuseMap + "', name: '" + materialInfo.name + "'");
        props.diffuseMap = materialInfo.diffuseMap;  // Use diffuseMap field
        if (props.diffuseMap.empty()) {
            // Fallback to name if diffuseMap is empty (legacy compatibility)
            props.diffuseMap = materialInfo.name;
            LOG_DEBUG("Using name as fallback for diffuseMap: '" + props.diffuseMap + "'");
        }
        if (props.diffuseMap.empty()) {
            // Final fallback to a purple dev texture
            props.diffuseMap = "textures/devtextures/Purple/proto_wall_purple.png";
            LOG_DEBUG("Using purple fallback texture: '" + props.diffuseMap + "'");
        }
        LOG_DEBUG("Setting diffuseMap to: '" + props.diffuseMap + "'");
        props.normalMap = materialInfo.normalMap;
        props.specularMap = materialInfo.specularMap;
        props.roughnessMap = materialInfo.roughnessMap;
        props.metallicMap = materialInfo.metallicMap;
        props.aoMap = materialInfo.aoMap;
        props.emissiveMap = materialInfo.emissiveMap;

        // Set rendering flags
        props.doubleSided = materialInfo.doubleSided;
        props.depthWrite = true;  // Default values
        props.depthTest = true;
        props.castShadows = true;

        // Set material name - extract from texture path or use a default
        // For now, use the texture filename as the material name
        std::string texturePath = materialInfo.name;
        size_t lastSlash = texturePath.find_last_of('/');
        size_t lastDot = texturePath.find_last_of('.');
        if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
            props.materialName = texturePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
        } else {
            props.materialName = "Material_" + std::to_string(materialInfo.id);
        }
        LOG_DEBUG("Setting materialName to: '" + props.materialName + "'");

        // Load textures through AssetSystem (MaterialSystem will handle this internally)
        if (assetSystem) {
            if (!props.diffuseMap.empty()) {
                assetSystem->LoadTexture(props.diffuseMap);
            }
            if (!props.normalMap.empty()) {
                assetSystem->LoadTexture(props.normalMap);
            }
            if (!props.specularMap.empty()) {
                assetSystem->LoadTexture(props.specularMap);
            }
            if (!props.roughnessMap.empty()) {
                assetSystem->LoadTexture(props.roughnessMap);
            }
            if (!props.metallicMap.empty()) {
                assetSystem->LoadTexture(props.metallicMap);
            }
            if (!props.aoMap.empty()) {
                assetSystem->LoadTexture(props.aoMap);
            }
            if (!props.emissiveMap.empty()) {
                assetSystem->LoadTexture(props.emissiveMap);
            }
        }

        // Create material through MaterialSystem (flyweight pattern)
        uint32_t materialSystemId = materialSystem->GetOrCreateMaterial(props);

        // Map original material ID to new material ID (for WorldSystem)
        materialIdMap_[materialInfo.id] = materialSystemId;

        // Also map in WorldGeometry for surface ID to material ID lookup
        worldGeometry_->materialIdMap[materialInfo.id] = materialSystemId;

        LOG_DEBUG("Material " + std::to_string(materialInfo.id) + " mapped to MaterialSystem ID " +
                  std::to_string(materialSystemId) + " ('" + materialInfo.name + "')");
    }

    LOG_INFO("Loaded " + std::to_string(materialIdMap_.size()) + " materials through MaterialSystem");

    // BSP tree will be built later in the pipeline with properly material-assigned faces
}

void WorldSystem::BuildBSPTreeAfterMaterials() {
    LOG_INFO("=== BuildBSPTreeAfterMaterials STARTED ===");
    LOG_INFO("Building BSP tree with material-assigned faces");

    LOG_DEBUG("bspTreeSystem_ check: " + std::string(bspTreeSystem_ ? "AVAILABLE" : "NULL"));
    if (!bspTreeSystem_) {
        LOG_WARNING("No BSPTreeSystem available, BSP tree will not be built");
        return;
    }

    LOG_DEBUG("worldGeometry_ check: " + std::string(worldGeometry_ ? "AVAILABLE" : "NULL"));
    if (!worldGeometry_) {
        LOG_WARNING("No worldGeometry_ available");
        return;
    }

    LOG_DEBUG("worldGeometry_->faces.size(): " + std::to_string(worldGeometry_->faces.size()));
    if (worldGeometry_->faces.empty()) {
        LOG_WARNING("No faces available for BSP tree building");
        return;
    }
    
    // Build Quake-style world using the material-assigned faces from worldGeometry
    auto world = bspTreeSystem_->LoadWorld(worldGeometry_->faces);

    if (!world) {
        LOG_ERROR("Failed to build Quake-style world");
        return;
    }

    worldGeometry_->SetWorld(std::move(world));

    LOG_INFO("Quake-style world built successfully with " +
             std::to_string(worldGeometry_->GetWorld()->surfaces.size()) + " surfaces");
}

// Legacy fallback for when AssetSystem is unavailable
void WorldSystem::LoadTexturesLegacy(const MapData& mapData) {
    LOG_INFO("Using legacy material loading - creating basic MaterialSystem materials");

    MaterialSystem* materialSystem = engine_.GetSystem<MaterialSystem>();
    if (!materialSystem) {
        LOG_ERROR("MaterialSystem not available for legacy loading");
        return;
    }

    // Create basic materials through MaterialSystem
    MaterialProperties props;

    // Wall material (id: 0)
    props.primaryColor = GRAY;
    props.secondaryColor = BLACK;
    props.shininess = 10.0f;
    props.materialName = "wall_default";
    uint32_t wallId = materialSystem->GetOrCreateMaterial(props);
    worldGeometry_->materialIdMap[0] = wallId;

    // Floor material (id: 1)
    props.primaryColor = DARKGRAY;
    props.secondaryColor = BLACK;
    props.shininess = 5.0f;
    props.materialName = "floor_default";
    uint32_t floorId = materialSystem->GetOrCreateMaterial(props);
    worldGeometry_->materialIdMap[1] = floorId;

    // Ceiling material (id: 2)
    props.primaryColor = LIGHTGRAY;
    props.secondaryColor = BLACK;
    props.shininess = 5.0f;
    props.materialName = "ceiling_default";
    uint32_t ceilingId = materialSystem->GetOrCreateMaterial(props);
    worldGeometry_->materialIdMap[2] = ceilingId;

    // Orange material (id: 3)
    props.primaryColor = ORANGE;
    props.secondaryColor = BLACK;
    props.shininess = 32.0f;
    props.materialName = "orange_default";
    uint32_t orangeId = materialSystem->GetOrCreateMaterial(props);
    worldGeometry_->materialIdMap[3] = orangeId;

    LOG_INFO("Created " + std::to_string(worldGeometry_->materialIdMap.size()) + " default materials through MaterialSystem");

    // Note: In the new system, textures are loaded through AssetSystem during LoadTexturesAndMaterials
    // This legacy function now just ensures material mappings exist
    for (const auto& materialInfo : mapData.materials) {
        if (materialInfo.id < 0) {
            LOG_WARNING("Skipping material with invalid ID: " + std::to_string(materialInfo.id));
            continue;
        }

        // Ensure we have a mapping for this material ID
        if (worldGeometry_->materialIdMap.find(materialInfo.id) == worldGeometry_->materialIdMap.end()) {
            // Create a basic material for this ID if not already mapped
            MaterialProperties props;
            props.primaryColor = materialInfo.diffuseColor;
            props.secondaryColor = BLACK;
            props.shininess = (materialInfo.id == 0) ? 10.0f : 5.0f;
            props.diffuseMap = materialInfo.diffuseMap;
            props.materialName = materialInfo.name;

            uint32_t materialSystemId = materialSystem->GetOrCreateMaterial(props);
            worldGeometry_->materialIdMap[materialInfo.id] = materialSystemId;

            LOG_INFO("Created legacy material mapping for ID " + std::to_string(materialInfo.id) +
                     " -> MaterialSystem ID " + std::to_string(materialSystemId));
        }
    }
}

void WorldSystem::LoadDeferredTextures() {
    LOG_INFO("LoadDeferredTextures: Loading textures that were deferred from initialization");

    // Get AssetSystem
    auto assetSystem = engine_.GetSystem<AssetSystem>();
    if (!assetSystem) {
        LOG_ERROR("LoadDeferredTextures: AssetSystem not available");
        return;
    }

    // Load textures using the correct paths from the map file
    const std::vector<std::pair<std::string, int>> mapTextures = {
        {"textures/devtextures/Dark/proto_wall_dark.png", 0},          // Material ID 0 - walls
        {"textures/devtextures/Light/proto_1024_light.png", 1},        // Material ID 1 - floor
        {"textures/devtextures/Green/proto_1024_green.png", 2},        // Material ID 2 - ceiling
        {"textures/devtextures/Orange/proto_1024_orange.png", 3}       // Material ID 3 - stairs/other
    };

    for (const auto& [texturePath, materialId] : mapTextures) {
        LOG_INFO("LoadDeferredTextures: Loading texture " + texturePath + " for material ID " + std::to_string(materialId));

        if (assetSystem->LoadTexture(texturePath)) {
            LOG_INFO("LoadDeferredTextures: Successfully loaded texture from " + texturePath);

            // Get the texture handle and update WorldMaterial
            auto textureHandle = assetSystem->GetTextureHandle(texturePath);
            Texture2D* loadedTexture = assetSystem->GetTexture(textureHandle);

            if (loadedTexture && loadedTexture->id > 0) {
                LOG_INFO("LoadDeferredTextures: Texture loaded successfully for material ID " + std::to_string(materialId) +
                         " (ID: " + std::to_string(loadedTexture->id) + ")");

                // In the new system, textures are managed by AssetSystem and materials by MaterialSystem
                // No need to update WorldMaterial objects anymore

            } else {
                LOG_WARNING("LoadDeferredTextures: Failed to get texture pointer for " + texturePath);
            }
        } else {
            LOG_WARNING("LoadDeferredTextures: Failed to load texture from " + texturePath);
        }
    }

    LOG_INFO("LoadDeferredTextures: Deferred texture loading completed");

    // Update batch colors now that textures are loaded
    if (worldGeometry_) {
        // Material colors now handled directly in renderer - no batch color updates needed
        LOG_INFO("LoadDeferredTextures: Updated batch colors for textured materials");
    }
}

// Helper function - now deprecated with new MaterialSystem
// Textures are loaded upfront in LoadTexturesAndMaterials
void WorldSystem::UpdateMaterialComponentWithTexture(int materialId, AssetSystem::TextureHandle textureHandle) {
    LOG_INFO("UpdateMaterialComponentWithTexture: DEPRECATED - MaterialSystem handles textures upfront. "
             "Material ID " + std::to_string(materialId) + ", texture: " + textureHandle.path);
    // No-op - textures are loaded upfront in LoadTexturesAndMaterials
}

// Setup skybox - load the real cubemap or fail
void WorldSystem::SetupSkybox(const MapData& mapData) {
    LOG_INFO("Skybox: setting up with cubemap support");

    // Load the real cubemap
    std::string skyboxPath = "textures/skyboxcubemaps/cubemap_cloudy&blue.png";
    if (worldGeometry_->skybox->LoadFromFile(skyboxPath)) {
        LOG_INFO("Skybox: loaded cubemap from: " + skyboxPath);
    } else {
        LOG_ERROR("Skybox: FAILED to load cubemap from: " + skyboxPath);
        LOG_ERROR("Skybox: Skybox will not render - check cubemap file and loading code");
    }
}

// NEW: Create dynamic entities only
void WorldSystem::CreateDynamicEntitiesFromMap(const MapData& mapData) {
    LOG_INFO("Creating dynamic entities from MapData - found " + std::to_string(mapData.entities.size()) + " entities to create");

    // Clear existing dynamic entities
    DestroyDynamicEntities();

    // Set materials in EntityFactory so it can create entities with proper materials
    if (entityFactory_) {
        entityFactory_->SetMaterials(mapData.materials);
    }

    // Create entities from parsed entity definitions using EntityFactory
    if (!mapData.entities.empty()) {
        LOG_INFO("Creating entities from " + std::to_string(mapData.entities.size()) + " entity definitions");

        auto gameObjectSystem = engine_.GetSystem<GameObjectSystem>();

        if (!entityFactory_ || !gameObjectSystem) {
            LOG_ERROR("WorldSystem: EntityFactory or GameObjectSystem not available");
            return;
        }

        std::vector<Entity*> createdEntities = entityFactory_->CreateEntitiesFromDefinitions(mapData.entities);

        LOG_INFO("WorldSystem: Created " + std::to_string(createdEntities.size()) + " entities");

        // Store the created entities
        dynamicEntities_.insert(dynamicEntities_.end(), createdEntities.begin(), createdEntities.end());

        // Register Game Objects with the GameObjectSystem AND the Engine's ECS registry
        for (Entity* entity : createdEntities) {
            GameObject* gameObj = entity->GetComponent<GameObject>();
            if (gameObj) {
                gameObjectSystem->RegisterGameObject(entity);
                
                // Also register light entities directly with LightSystem
                if (gameObj->type == GameObjectType::LIGHT_POINT || 
                    gameObj->type == GameObjectType::LIGHT_SPOT || 
                    gameObj->type == GameObjectType::LIGHT_DIRECTIONAL) {
                    auto lightSystem = engine_.GetSystem<LightSystem>();
                    if (lightSystem) {
                        lightSystem->RegisterLight(entity);
                        LOG_INFO("🔆 Registered light entity " + std::to_string(entity->GetId()) + " with LightSystem");
                    }
                }
            }
            
            // CRITICAL: Register entity with Engine's ECS system so all systems can see it
            engine_.UpdateEntityRegistration(entity);
            LOG_INFO("Registered entity " + std::to_string(entity->GetId()) + " with Engine ECS systems");
        }

        LOG_INFO("Created and registered " + std::to_string(createdEntities.size()) + " entities from map definitions");
    } else {
        // No entities found in map data - if you need test entities,
        // uncomment the line below.
        // AddTestDynamicEntity();
    }

    LOG_INFO("Total dynamic entities: " + std::to_string(dynamicEntities_.size()));
}

// RENAMED: Destroy dynamic entities only
void WorldSystem::DestroyDynamicEntities() {
    auto gameObjectSystem = engine_.GetSystem<GameObjectSystem>();

    for (Entity* entity : dynamicEntities_) {
        if (entity) {
            // Unregister from GameObjectSystem if it's a Game Object
            GameObject* gameObj = entity->GetComponent<GameObject>();
            if (gameObj && gameObjectSystem) {
                gameObjectSystem->UnregisterGameObject(entity);
            }

            engine_.DestroyEntity(entity);
        }
    }
    dynamicEntities_.clear();
    LOG_INFO("Dynamic entities destroyed");
}


// DELEGATED: Point containment check
bool WorldSystem::ContainsPoint(const Vector3& point) const {
    // TODO: Implement point containment using new World system
    // For now, return false (no collision)
    return false;
}

// DELEGATED: Ray casting
float WorldSystem::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    // TODO: Implement ray casting using new World system
    // For now, return maxDistance (no hit)
    return maxDistance;
}

bool WorldSystem::FindSpawnPoint(Vector3& spawnPoint) const {
    // For now, use a default spawn point
    // In a full implementation, this would read spawn points from the map
    spawnPoint = {0.0f, 2.0f, 0.0f};
    return true;
}

void WorldSystem::ConnectCollisionSystem(CollisionSystem* collisionSystem) {
    collisionSystem_ = collisionSystem;

    // If map is already loaded, connect immediately
    if (mapLoaded_ && worldGeometry_) {
        collisionSystem_->SetWorld(worldGeometry_->GetWorld());
        LOG_INFO("Collision system connected to existing BSP tree");
    }
}

void WorldSystem::ConnectRenderSystem(RenderSystem* renderSystem) {
    renderSystem_ = renderSystem;
    LOG_INFO("RenderSystem connected to WorldSystem for asset cache management");
}

MapData WorldSystem::CreateTestMap() {
    LOG_INFO("Creating test map data");

    // Create map data structure
    MapData mapData;
    mapData.name = "Test Map - Stage vs Actors";
    mapData.skyColor = SKYBLUE;
    mapData.floorHeight = 0.0f;
    mapData.ceilingHeight = 8.0f;

    // Add basic materials (without loading them - just metadata)
    {
        MaterialInfo wallMat;
        wallMat.id = 0;
        wallMat.name = "Wall Material";
        wallMat.diffuseMap = "textures/devtextures/Dark/proto_wall_dark.png";
        wallMat.diffuseColor = {139, 69, 19, 255}; // Brown color
        mapData.materials.push_back(wallMat);
    }
    {
        MaterialInfo floorMat;
        floorMat.id = 1;
        floorMat.name = "Floor Material";
        floorMat.diffuseMap = "textures/devtextures/Light/proto_1024_light.png";
        floorMat.diffuseColor = {169, 169, 169, 255}; // Light gray
        mapData.materials.push_back(floorMat);
    }
    {
        MaterialInfo ceilingMat;
        ceilingMat.id = 2;
        ceilingMat.name = "Ceiling Material";
        ceilingMat.diffuseMap = "textures/devtextures/Green/proto_1024_green.png";
        ceilingMat.diffuseColor = {144, 238, 144, 255}; // Light green
        mapData.materials.push_back(ceilingMat);
    }
    {
        MaterialInfo slopeMat;
        slopeMat.id = 3;
        slopeMat.name = "Slope Material";
        slopeMat.diffuseMap = "textures/devtextures/Orange/proto_1024_orange.png";
        slopeMat.diffuseColor = {255, 165, 0, 255}; // Orange
        mapData.materials.push_back(slopeMat);
    }

    // Create geometry and add to map data
    AddTestGeometry(mapData);

    LOG_INFO("Test map data created with " + std::to_string(mapData.faces.size()) + " faces");
    return mapData;
}



// Export programmatic geometry to YAML format for the test map
std::string WorldSystem::ExportGeometryToYaml() {
    LOG_INFO("Exporting programmatic geometry to YAML format");

    MapData mapData = CreateTestMap();
    std::stringstream yaml;

    yaml << "# Auto-generated geometry from CreateTestMap()\n";
    yaml << "brushes:\n";

    int brushId = 1;
    for (size_t i = 0; i < mapData.faces.size(); ++i) {
        const Face& face = mapData.faces[i];

        yaml << "  - id: " << brushId++ << "\n";
        yaml << "    faces:\n";
        yaml << "      - vertices:\n";

        for (const Vector3& vertex : face.vertices) {
            yaml << "          - [" << vertex.x << ", " << vertex.y << ", " << vertex.z << "]\n";
        }

        // Export UV coordinates if available
        if (!face.uvs.empty() && face.uvs.size() == face.vertices.size()) {
            yaml << "        uvs:\n";
            for (const Vector2& uv : face.uvs) {
                yaml << "          - [" << uv.x << ", " << uv.y << "]\n";
            }
        }

        yaml << "        material: " << face.materialId << "\n";
        yaml << "        tint: [" << (int)face.tint.r << ", " << (int)face.tint.g << ", "
             << (int)face.tint.b << ", " << (int)face.tint.a << "]\n";
    }

    LOG_INFO("Exported " + std::to_string(mapData.faces.size()) + " faces to YAML format");
    return yaml.str();
}

// Export geometry to a file for development
void WorldSystem::ExportGeometryToFile() {
    LOG_INFO("Exporting geometry to file for YAML map development");

    std::string yamlContent = ExportGeometryToYaml();

    // Write to a temporary file
    std::ofstream outFile("geometry_export.yaml");
    if (outFile.is_open()) {
        outFile << yamlContent;
        outFile.close();
        LOG_INFO("Geometry exported to geometry_export.yaml");
    } else {
        LOG_ERROR("Failed to open geometry_export.yaml for writing");
    }
}

// Add geometry for the test map - Enhanced with slopes, platforms, and larger rooms
void WorldSystem::AddTestGeometry(MapData& mapData) {
    LOG_INFO("Adding enhanced multi-room geometry with slopes and platforms to test map");

    const float wallHeight = 8.0f;
    const float floorY = 0.0f;
    const float ceilingY = 8.0f;

    // === ROOM 1 (Starting area): -5 to 5 in X, -5 to 5 in Z, NO CEILING ===
    
    // Helper lambdas to create faces from legacy params
    auto AddHorizontalFace = [&](const Vector3& start, const Vector3& end, float y, int mat, Color tint){
        Face f; f.materialId = mat; f.tint = tint;
        Vector3 p1{start.x, y, start.z};
        Vector3 p2{end.x,   y, start.z};
        Vector3 p3{end.x,   y, end.z};
        Vector3 p4{start.x, y, end.z};
        // If this is a floor (assume floorY), wind CCW as seen from above to get +Y normal
        if (fabsf(y - floorY) < 0.001f) {
            f.vertices = {p1, p4, p3, p2};
            // UVs for floor - XZ coordinates
            float uvScale = 0.1f;
            f.uvs = {{p1.x * uvScale, p1.z * uvScale},
                    {p4.x * uvScale, p4.z * uvScale},
                    {p3.x * uvScale, p3.z * uvScale},
                    {p2.x * uvScale, p2.z * uvScale}};
        } else {
            // Ceiling: keep winding to get -Y normal
            f.vertices = {p1, p2, p3, p4};
            // UVs for ceiling - XZ coordinates (flipped)
            float uvScale = 0.1f;
            f.uvs = {{p1.x * uvScale, p1.z * uvScale},
                    {p2.x * uvScale, p2.z * uvScale},
                    {p3.x * uvScale, p3.z * uvScale},
                    {p4.x * uvScale, p4.z * uvScale}};
        }
        f.RecalculateNormal();
        mapData.faces.push_back(f);
    };
    auto AddVerticalWall = [&](const Vector3& start, const Vector3& end, float height, int mat, Color tint){
        Face f; f.materialId = mat; f.tint = tint;
        float bottomY = start.y; float topY = bottomY + height;
        Vector3 bottomLeft{start.x, bottomY, start.z};
        Vector3 bottomRight{end.x,   bottomY, end.z};
        Vector3 topRight{end.x,   topY, end.z};
        Vector3 topLeft{start.x, topY, start.z};
        f.vertices = {bottomLeft, bottomRight, topRight, topLeft};
        // UVs for vertical walls - X and Y coordinates
        float uvScale = 0.1f;
        f.uvs = {{bottomLeft.x * uvScale, bottomLeft.y * uvScale},
                {bottomRight.x * uvScale, bottomRight.y * uvScale},
                {topRight.x * uvScale, topRight.y * uvScale},
                {topLeft.x * uvScale, topLeft.y * uvScale}};
        f.RecalculateNormal();
        mapData.faces.push_back(f);
    };
    
    // Helper to create solid boxes with all 6 faces and correct winding
    auto AddSolidBox = [&](const Vector3& minCorner, const Vector3& maxCorner, int mat, Color tint) {
        LOG_INFO("AddSolidBox DEBUG: Creating box with material ID " + std::to_string(mat) + " tint (" + 
                 std::to_string(tint.r) + "," + std::to_string(tint.g) + "," + std::to_string(tint.b) + ")");
        float minX = minCorner.x, minY = minCorner.y, minZ = minCorner.z;
        float maxX = maxCorner.x, maxY = maxCorner.y, maxZ = maxCorner.z;
        
        // All faces use the same material ID as passed to AddSolidBox
        // Top face (+Y normal)
        Face topFace;
        topFace.materialId = mat;
        topFace.tint = tint;
        topFace.vertices = {{minX, maxY, minZ}, {minX, maxY, maxZ}, {maxX, maxY, maxZ}, {maxX, maxY, minZ}};
        // Add UVs for top face - XZ coordinates
        float uvScale = 0.1f;
        topFace.uvs = {{minX * uvScale, minZ * uvScale},
                      {minX * uvScale, maxZ * uvScale},
                      {maxX * uvScale, maxZ * uvScale},
                      {maxX * uvScale, minZ * uvScale}};
        topFace.RecalculateNormal(); mapData.faces.push_back(topFace);

        // Bottom face (-Y normal)
        Face bottomFace;
        bottomFace.materialId = mat;
        bottomFace.tint = tint;
        bottomFace.vertices = {{minX, minY, minZ}, {maxX, minY, minZ}, {maxX, minY, maxZ}, {minX, minY, maxZ}};
        // Add UVs for bottom face - XZ coordinates (flipped for bottom)
        bottomFace.uvs = {{minX * uvScale, maxZ * uvScale},
                         {maxX * uvScale, maxZ * uvScale},
                         {maxX * uvScale, minZ * uvScale},
                         {minX * uvScale, minZ * uvScale}};
        bottomFace.RecalculateNormal(); mapData.faces.push_back(bottomFace);
        
        // Front face (-Z normal)
        Face frontFace;
        frontFace.materialId = mat;
        frontFace.tint = tint;
        frontFace.vertices = {{minX, minY, minZ}, {minX, maxY, minZ}, {maxX, maxY, minZ}, {maxX, minY, minZ}};
        // Add UVs for front face - X and Y coordinates
        frontFace.uvs = {{minX * uvScale, minY * uvScale},
                        {minX * uvScale, maxY * uvScale},
                        {maxX * uvScale, maxY * uvScale},
                        {maxX * uvScale, minY * uvScale}};
        frontFace.RecalculateNormal();
        LOG_INFO("STAIRCASE FACE: front face normal (" + std::to_string(frontFace.normal.x) + "," +
                 std::to_string(frontFace.normal.y) + "," + std::to_string(frontFace.normal.z) +
                 ") material ID " + std::to_string(frontFace.materialId));
        mapData.faces.push_back(frontFace);
        
        // Back face (+Z normal)
        Face backFace;
        backFace.materialId = mat;
        backFace.tint = tint;
        backFace.vertices = {{maxX, minY, maxZ}, {maxX, maxY, maxZ}, {minX, maxY, maxZ}, {minX, minY, maxZ}};
        // Add UVs for back face - X and Y coordinates (flipped)
        backFace.uvs = {{maxX * uvScale, minY * uvScale},
                       {maxX * uvScale, maxY * uvScale},
                       {minX * uvScale, maxY * uvScale},
                       {minX * uvScale, minY * uvScale}};
        backFace.RecalculateNormal(); mapData.faces.push_back(backFace);
        
        // Left face (-X normal)
        Face leftFace;
        leftFace.materialId = mat;
        leftFace.tint = tint;
        leftFace.vertices = {{minX, minY, maxZ}, {minX, maxY, maxZ}, {minX, maxY, minZ}, {minX, minY, minZ}};
        // Add UVs for left face - Y and Z coordinates (flipped)
        leftFace.uvs = {{minY * uvScale, maxZ * uvScale},
                       {maxY * uvScale, maxZ * uvScale},
                       {maxY * uvScale, minZ * uvScale},
                       {minY * uvScale, minZ * uvScale}};
        leftFace.RecalculateNormal(); mapData.faces.push_back(leftFace);
        
        // Right face (+X normal)
        Face rightFace;
        rightFace.materialId = mat;
        rightFace.tint = tint;
        rightFace.vertices = {{maxX, minY, minZ}, {maxX, maxY, minZ}, {maxX, maxY, maxZ}, {maxX, minY, maxZ}};
        // Add UVs for right face - Y and Z coordinates
        rightFace.uvs = {{minY * uvScale, minZ * uvScale},
                        {maxY * uvScale, minZ * uvScale},
                        {maxY * uvScale, maxZ * uvScale},
                        {minY * uvScale, maxZ * uvScale}};
        rightFace.RecalculateNormal(); mapData.faces.push_back(rightFace);
    };

    // Room 1 Floor (horizontal face) material 3 (orange) - TESTING FLOOR TINTING
    AddHorizontalFace(Vector3{-5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, 5.0f}, floorY, 3, WHITE);
    
    // Room 1 North wall (vertical surface) material 0 (wall)
    AddVerticalWall(Vector3{-5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 South wall (vertical surface)
    AddVerticalWall(Vector3{5.0f, floorY, 5.0f}, Vector3{-5.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 West wall (vertical surface) - split to create corridor opening
    // South part: Z=5 to Z=2
    AddVerticalWall(Vector3{-5.0f, floorY, 5.0f}, Vector3{-5.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    // North part: Z=-2 to Z=-5
    AddVerticalWall(Vector3{-5.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 East wall (with opening for corridor from Z=-2 to Z=2)
    AddVerticalWall(Vector3{5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{5.0f, floorY, 2.0f}, Vector3{5.0f, floorY, 5.0f}, wallHeight, 0, WHITE);

    // === NORTH CORRIDOR FROM ROOM 1: Z -5 to -25, X -2 to 2, HAS CEILING ===
    AddHorizontalFace(Vector3{-2.0f, floorY, -25.0f}, Vector3{2.0f, floorY, -5.0f}, floorY, 1, WHITE); // floor
    // West wall (X=-2)
    AddVerticalWall(Vector3{-2.0f, floorY, -25.0f}, Vector3{-2.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    // East wall (X=2)
    AddVerticalWall(Vector3{2.0f, floorY, -5.0f}, Vector3{2.0f, floorY, -25.0f}, wallHeight, 0, WHITE);
    // Ceiling
    AddHorizontalFace(Vector3{-2.0f, ceilingY, -25.0f}, Vector3{2.0f, ceilingY, -5.0f}, ceilingY, 2, WHITE);

    // === NORTH ROOM: X -12 to 12, Z -45 to -25, HAS CEILING ===
    // Floor
    AddHorizontalFace(Vector3{-12.0f, floorY, -45.0f}, Vector3{12.0f, floorY, -25.0f}, floorY, 1, WHITE);
    // North wall (Z=-45)
    AddVerticalWall(Vector3{-12.0f, floorY, -45.0f}, Vector3{12.0f, floorY, -45.0f}, wallHeight, 0, WHITE);
    // South wall (Z=-25) with opening to corridor at X in [-2,2]
    AddVerticalWall(Vector3{12.0f, floorY, -25.0f}, Vector3{2.0f, floorY, -25.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{-2.0f, floorY, -25.0f}, Vector3{-12.0f, floorY, -25.0f}, wallHeight, 0, WHITE);
    // East wall (X=12)
    AddVerticalWall(Vector3{12.0f, floorY, -25.0f}, Vector3{12.0f, floorY, -45.0f}, wallHeight, 0, WHITE);
    // West wall (X=-12)
    AddVerticalWall(Vector3{-12.0f, floorY, -45.0f}, Vector3{-12.0f, floorY, -25.0f}, wallHeight, 0, WHITE);
    // Ceiling
    AddHorizontalFace(Vector3{-12.0f, ceilingY, -45.0f}, Vector3{12.0f, ceilingY, -25.0f}, ceilingY, 2, WHITE);

    // === CORRIDOR: X=5 to X=15, Z=-2 to Z=2, HAS CEILING ===
    
    // Corridor Floor (material 1)
    AddHorizontalFace(Vector3{5.0f, floorY, -2.0f}, Vector3{15.0f, floorY, 2.0f}, floorY, 1, WHITE);
    
    // Corridor North wall (swap start/end to flip normal inward)
    AddVerticalWall(Vector3{15.0f, floorY, 2.0f}, Vector3{5.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    
    // Corridor South wall (swap start/end to flip normal inward)
    AddVerticalWall(Vector3{5.0f, floorY, -2.0f}, Vector3{15.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    
    // Corridor ceiling as horizontal face at ceilingY (material 2)
    AddHorizontalFace(Vector3{5.0f, ceilingY, -2.0f}, Vector3{15.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === ROOM 2 (Destination): X=15 to X=27, Z=-6 to Z=6, HAS CEILING ===
    
    // Room 2 Floor (material 1)
    AddHorizontalFace(Vector3{15.0f, floorY, -6.0f}, Vector3{27.0f, floorY, 6.0f}, floorY, 1, WHITE);
    
    // Room 2 North wall
    AddVerticalWall(Vector3{15.0f, floorY, -6.0f}, Vector3{27.0f, floorY, -6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 South wall
    AddVerticalWall(Vector3{27.0f, floorY, 6.0f}, Vector3{15.0f, floorY, 6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 East wall (with opening for corridor to Room 3 from Z=-2 to Z=2)
    AddVerticalWall(Vector3{27.0f, floorY, -6.0f}, Vector3{27.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{27.0f, floorY, 2.0f}, Vector3{27.0f, floorY, 6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 West wall (with opening for corridor from Z=-2 to Z=2)
    AddVerticalWall(Vector3{15.0f, floorY, 6.0f}, Vector3{15.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{15.0f, floorY, -2.0f}, Vector3{15.0f, floorY, -6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 ceiling (material 2)
    AddHorizontalFace(Vector3{15.0f, ceilingY, -6.0f}, Vector3{27.0f, ceilingY, 6.0f}, ceilingY, 2, WHITE);


    // === CORRIDOR 2: Room 2 to Room 3: X=27 to X=37, Z=-2 to Z=2, HAS CEILING ===
    
    // Corridor 2 Floor (material 1)
    AddHorizontalFace(Vector3{27.0f, floorY, -2.0f}, Vector3{37.0f, floorY, 2.0f}, floorY, 1, WHITE);
    
    // Corridor 2 North wall
    AddVerticalWall(Vector3{37.0f, floorY, 2.0f}, Vector3{27.0f, floorY, 2.0f}, wallHeight, 0, WHITE);

    // Corridor 2 South wall
    AddVerticalWall(Vector3{27.0f, floorY, -2.0f}, Vector3{37.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    
    // Corridor 2 ceiling (material 2)
    AddHorizontalFace(Vector3{27.0f, ceilingY, -2.0f}, Vector3{37.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === NEW: ENHANCED TESTING FEATURES ===
    
    // === ROOM 3 (Large Testing Area): X=37 to X=55, Z=-12 to Z=12 ===
    
    // Room 3 Floor (large open area for testing)
    AddHorizontalFace(Vector3{37.0f, floorY, -12.0f}, Vector3{55.0f, floorY, 12.0f}, floorY, 1, WHITE);
    
    // Room 3 walls (opening in west wall connects to Corridor 2)
    AddVerticalWall(Vector3{37.0f, floorY, -12.0f}, Vector3{55.0f, floorY, -12.0f}, wallHeight, 0, WHITE); // North
    AddVerticalWall(Vector3{55.0f, floorY, 12.0f}, Vector3{37.0f, floorY, 12.0f}, wallHeight, 0, WHITE);   // South
    AddVerticalWall(Vector3{55.0f, floorY, -12.0f}, Vector3{55.0f, floorY, 12.0f}, wallHeight, 0, WHITE);  // East
    
    // Room 3 West wall with opening (door from Corridor 2, Z=-2 to Z=2)
    AddVerticalWall(Vector3{37.0f, floorY, 12.0f}, Vector3{37.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{37.0f, floorY, -2.0f}, Vector3{37.0f, floorY, -12.0f}, wallHeight, 0, WHITE);
    
    // Room 3 ceiling REMOVED - open like Room 1

    // === TESTING PLATFORMS (Various heights for jump testing) ===
    
    const float platform1Y = 1.0f;  // Low platform - should step up automatically
    const float platform2Y = 2.0f;  // Medium platform - requires jumping
    const float platform3Y = 3.5f;  // High platform - requires jumping
    
    // Platform 1 (low step-up test) - solid box from X=40-43, Z=8-11, Y=0-1
    AddSolidBox(Vector3{40.0f, floorY, 8.0f}, Vector3{43.0f, platform1Y, 11.0f}, 0, WHITE);

    // Platform 2 (medium jump test) - solid box from X=46-49, Z=8-11, Y=0-2
    AddSolidBox(Vector3{46.0f, floorY, 8.0f}, Vector3{49.0f, platform2Y, 11.0f}, 0, WHITE);

    // Platform 3 (high jump test) - solid box from X=52-55, Z=8-11, Y=0-3.5
    AddSolidBox(Vector3{52.0f, floorY, 8.0f}, Vector3{55.0f, platform3Y, 11.0f}, 0, WHITE);

    // === SLOPE TESTING AREA ===
    
    // Create stepped platforms to simulate slope climbing (X=40-52, Z=-11 to Z=-8)
    const int slopeSteps = 6;
    const float slopeStartX = 40.0f;
    const float slopeEndX = 52.0f;
    const float slopeStartZ = -11.0f;
    const float slopeEndZ = -8.0f;
    const float slopeStartY = 0.0f;
    const float slopeEndY = 3.0f;
    
    for (int i = 0; i < slopeSteps; i++) {
        float t = (float)i / (float)(slopeSteps - 1);
        float nextT = (float)(i + 1) / (float)(slopeSteps - 1);
        
        float x1 = slopeStartX + t * (slopeEndX - slopeStartX);
        float x2 = slopeStartX + nextT * (slopeEndX - slopeStartX);
        float stepTop = slopeStartY + nextT * (slopeEndY - slopeStartY);
        
        // Create solid box for each slope step (full height from floor to step top)
        AddSolidBox(Vector3{x1, floorY, slopeStartZ},
                    Vector3{x2, stepTop, slopeEndZ},
                    3, WHITE); // Use material ID 3 (orange texture) for stepped slopes
    }
    
    // === SMOOTH SLOPE TESTING AREA ===
    
    // Create a true smooth slope using angled faces (X=40-52, Z=5 to Z=8)
    const float smoothSlopeStartX = 40.0f;
    const float smoothSlopeEndX = 52.0f;
    const float smoothSlopeStartZ = 5.0f;
    const float smoothSlopeEndZ = 8.0f;
    const float smoothSlopeStartY = 0.0f;
    const float smoothSlopeEndY = 2.5f;
    
    // Create SOLID slope with proper geometry (top surface + back wall + side walls)

    // TOP SURFACE - Two triangles forming the slope
    Face slopeFace1;
    slopeFace1.materialId = 3; // Use material ID 3 (orange texture) for slopes
    slopeFace1.tint = WHITE;
    slopeFace1.vertices = {
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeStartZ}, // Bottom left
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeEndZ},   // Bottom right
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ}        // Top right
    };
    slopeFace1.RecalculateNormal();
    LOG_INFO("SLOPE TOP FACE 1: Created with materialId=" + std::to_string(slopeFace1.materialId) +
             ", normal (" + std::to_string(slopeFace1.normal.x) + "," +
             std::to_string(slopeFace1.normal.y) + "," + std::to_string(slopeFace1.normal.z) + ")");
    mapData.faces.push_back(slopeFace1);

    Face slopeFace2;
    slopeFace2.materialId = 3; // Use material ID 3 (orange texture) for slopes
    slopeFace2.tint = WHITE;
    slopeFace2.vertices = {
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeStartZ}, // Bottom left
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ},       // Top right
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ}      // Top left
    };
    slopeFace2.RecalculateNormal();
    LOG_INFO("SLOPE TOP FACE 2: Created with normal (" + std::to_string(slopeFace2.normal.x) + "," +
             std::to_string(slopeFace2.normal.y) + "," + std::to_string(slopeFace2.normal.z) + ")");
    mapData.faces.push_back(slopeFace2);

    // BACK WALL - Solid wall at the end of the slope
    Face backWall1;
    backWall1.materialId = 0; // Use material ID 0 (dark texture)
    backWall1.tint = WHITE;
    backWall1.vertices = {
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ},     // Top left
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ},       // Top right
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeEndZ}      // Bottom right
    };
    backWall1.RecalculateNormal();
    mapData.faces.push_back(backWall1);

    Face backWall2;
    backWall2.materialId = 0; // Use material ID 0 (dark texture)
    backWall2.tint = WHITE;
    backWall2.vertices = {
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ},     // Top left
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeEndZ},     // Bottom right
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeStartZ}    // Bottom left
    };
    backWall2.RecalculateNormal();
    mapData.faces.push_back(backWall2);


    // === STAIRS TEST AREA ===
    
    // Create proper solid stairs in the corner (X=42-47, Z=-5 to Z=0)
    const int numStairs = 5;
    const float stairHeight = 0.4f; // Each step is 0.4 units high (within step-up range)
    const float stairDepth = 1.0f;
    
    for (int i = 0; i < numStairs; i++) {
        float stairY = floorY + (i + 1) * stairHeight; // Top of this step
        float stairZ = -5.0f + i * stairDepth;          // Front edge of this step (original position)

        // Create solid box for each step (full height from floor to step top)
        LOG_INFO("Creating staircase step " + std::to_string(i) + " at Y=" + std::to_string(stairY) +
                 " Z=[" + std::to_string(stairZ) + "," + std::to_string(stairZ + stairDepth) + "]");
        LOG_INFO("STAIRS DEBUG: Adding stair step " + std::to_string(i) + " with material ID 3 (orange texture)");
        AddSolidBox(Vector3{42.0f, floorY, stairZ},
                    Vector3{47.0f, stairY, stairZ + stairDepth},
                    3, WHITE); // Use material ID 3 (orange texture) for stairs
    }

    // === CORRIDOR 2: Connecting Room 1 to Room 3 (X=-5 to X=-10, Z=-2 to Z=2) ===

    // North wall (constant Z=-2, spans X=-10 to X=-5)
    AddVerticalWall(Vector3{-10.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    // South wall (constant Z=2, spans X=-5 to X=-10)
    AddVerticalWall(Vector3{-5.0f, floorY, 2.0f}, Vector3{-10.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    // Floor
    AddHorizontalFace(Vector3{-10.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, 2.0f}, floorY, 1, WHITE);
    // Ceiling
    AddHorizontalFace(Vector3{-10.0f, ceilingY, -2.0f}, Vector3{-5.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === ROOM 3 (Beyond Corridor 2): -20 to -10 in X, -5 to 5 in Z, NO CEILING ===

    // North wall
    AddVerticalWall(Vector3{-20.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    // South wall
    AddVerticalWall(Vector3{-10.0f, floorY, 5.0f}, Vector3{-20.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    // East wall (with corridor opening from Z=-2 to Z=2)
    AddVerticalWall(Vector3{-10.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{-10.0f, floorY, 2.0f}, Vector3{-10.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    // West wall
    AddVerticalWall(Vector3{-20.0f, floorY, 5.0f}, Vector3{-20.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    // Floor
    AddHorizontalFace(Vector3{-20.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, 5.0f}, floorY, 1, WHITE);
    // NO CEILING - open to sky!

    LOG_INFO("Enhanced test map now includes platforms, slopes, stairs, and Room 3 for comprehensive testing");
}

void WorldSystem::CreateRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating first room geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

void WorldSystem::CreateCorridorGeometry(MapData& mapData) {
    LOG_INFO("Creating corridor geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

void WorldSystem::CreateSecondRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating second room geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

// REMOVED: CreateSurfaceEntity - ARCHITECTURAL FIX
// Static surfaces are no longer converted to entities!
// They are stored in WorldGeometry and rendered by WorldRenderer.

// NEW: Add a rotating pyramid in Room 3 to test Mesh component
void WorldSystem::AddTestDynamicEntity() {
    // Add orange cube entity in Room 2 (static, non-rotating test)
    LOG_INFO("Adding orange cube entity in Room 2");

    Entity* cubeEntity = engine_.CreateEntity();
    LOG_INFO("Created cube entity with ID: " + std::to_string(cubeEntity->GetId()));

    // Add position component in Room 2 (center-ish)
    cubeEntity->AddComponent<Position>(21.0f, 2.0f, 0.0f); // Floating above ground
    LOG_INFO("Added Position component to cube at (21, 2, 0) in Room 2");

    // Add MaterialComponent with purple color (as default for meshes)
    MaterialSystem* materialSystem = engine_.GetSystem<MaterialSystem>();
    if (materialSystem) {
        MaterialProperties props;
        props.primaryColor = {128, 0, 128, 255}; // Purple
        props.secondaryColor = BLACK;
        props.shininess = 32.0f;
        props.specularColor = WHITE;
        props.alpha = 1.0f;
        props.roughness = 0.5f;
        props.metallic = 0.0f;
        props.ao = 1.0f;
        props.emissiveColor = BLACK;
        props.emissiveIntensity = 1.0f;
        props.type = MaterialData::MaterialType::BASIC;
        props.doubleSided = false;
        props.depthWrite = true;
        props.depthTest = true;
        props.castShadows = true;
        props.materialName = "cube_material";

        uint32_t materialId = materialSystem->GetOrCreateMaterial(props);
        cubeEntity->AddComponent<MaterialComponent>(materialId);
    }
    LOG_DEBUG("Added purple MaterialComponent to cube - entity has component: " +
              std::string(cubeEntity->HasComponent<MaterialComponent>() ? "YES" : "NO"));

    // Add transform component for the cube (static - no rotation)
    auto cubeTransform = cubeEntity->AddComponent<TransformComponent>();
    cubeTransform->position = {21.0f, 2.0f, 0.0f};
    cubeTransform->rotation = QuaternionIdentity(); // No rotation
    LOG_INFO("Added Transform component to cube (static)");

    // Add mesh component with cube geometry
    auto cubeMesh = cubeEntity->AddComponent<MeshComponent>();

    // Use MeshSystem to create cube geometry
    auto meshSystem = engine_.GetSystem<MeshSystem>();
    if (meshSystem) {
        meshSystem->CreateCube(cubeEntity, 2.0f, WHITE);
        // Don't set texture/material for now - let the purple MaterialComponent show through
        // meshSystem->SetMaterial(cubeEntity, 3); // Use orange texture material ID 3

    // Material is handled through MaterialComponent - no need to set textures separately

        // Register with LODSystem for distance-based LOD switching
        auto lodSystem = engine_.GetSystem<LODSystem>();
        if (lodSystem) {
            lodSystem->RegisterLODEntity(cubeEntity);
            LOG_INFO("Registered cube entity " + std::to_string(cubeEntity->GetId()) + " with LOD system");
    }

    LOG_INFO("Added Mesh component with orange cube (material ID 3)");
    } else {
        LOG_ERROR("MeshSystem not available for cube creation");
    }

    // NOTE: Mesh entities should not have Velocity components until we implement mesh physics
    // auto* cubeVelocity = cubeEntity->AddComponent<Velocity>();
    // cubeVelocity->SetVelocity({0.0f, 0.0f, 0.0f});
    // LOG_INFO("Added Velocity component (stationary)");

    // Add collision component for collision testing
    auto* cubeCollidable = cubeEntity->AddComponent<Collidable>(Vector3{2.0f, 2.0f, 2.0f}); // Cube bounding box
    cubeCollidable->SetCollisionLayer(LAYER_DEBRIS);
    cubeCollidable->SetCollisionMask(LAYER_WORLD | LAYER_PLAYER | LAYER_DEBRIS);
    LOG_INFO("Added Collidable component for cube collision testing");

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering cube entity with systems (after components added)");
    engine_.UpdateEntityRegistration(cubeEntity);

    // Add to dynamic entities list
    dynamicEntities_.push_back(cubeEntity);
    LOG_INFO("Orange cube entity added (static mesh test, rendered by RenderSystem)");

    // Now add rotating pyramid entity in Room 3
    LOG_INFO("Adding rotating pyramid entity in Room 3");

    Entity* pyramidEntity = engine_.CreateEntity();
    LOG_INFO("Created pyramid entity with ID: " + std::to_string(pyramidEntity->GetId()));

    // Add position component hovering in Room 3 (center of Room 3)
    pyramidEntity->AddComponent<Position>(-15.0f, 3.0f, 0.0f); // Hovering above ground
    LOG_INFO("Added Position component to pyramid at (-15, 3, 0) in Room 3");

    // Add transform component for the pyramid (will be rotated)
    auto pyramidTransform = pyramidEntity->AddComponent<TransformComponent>();
    pyramidTransform->position = {-15.0f, 3.0f, 0.0f};
    pyramidTransform->rotation = QuaternionIdentity(); // Start with no rotation
    LOG_INFO("Added Transform component to pyramid (rotating)");

    // Add gradient MaterialComponent for the pyramid BEFORE creating mesh
    MaterialSystem* materialSystem2 = engine_.GetSystem<MaterialSystem>();
    if (materialSystem2) {
        MaterialProperties props;
        props.primaryColor = PURPLE; // Start color
        props.secondaryColor = MAGENTA; // End color
        props.shininess = 32.0f;
        props.specularColor = WHITE;
        props.alpha = 1.0f;
        props.roughness = 0.5f;
        props.metallic = 0.0f;
        props.ao = 1.0f;
        props.emissiveColor = BLACK;
        props.emissiveIntensity = 1.0f;
        props.type = MaterialData::MaterialType::BASIC;
        props.doubleSided = false;
        props.depthWrite = true;
        props.depthTest = true;
        props.castShadows = true;
        props.materialName = "pyramid_gradient_material";

        uint32_t materialId = materialSystem2->GetOrCreateMaterial(props);
        MaterialComponent* pyramidMaterial = pyramidEntity->AddComponent<MaterialComponent>(materialId);
        pyramidMaterial->SetLinearGradient(); // Enable gradient mode
    }
    LOG_DEBUG("Added gradient MaterialComponent to pyramid - entity has component: " +
              std::string(pyramidEntity->HasComponent<MaterialComponent>() ? "YES" : "NO"));

    // Add mesh component with pyramid geometry
    auto pyramidMesh = pyramidEntity->AddComponent<MeshComponent>();

    // Use MeshSystem to create pyramid geometry (now it will detect the gradient material)
    auto pyramidMeshSystem = engine_.GetSystem<MeshSystem>();
    if (pyramidMeshSystem) {
        pyramidMeshSystem->CreatePyramid(pyramidEntity, 2.0f, 3.0f, {RED, GREEN, BLUE, YELLOW, GRAY}); // Different colored faces
    LOG_INFO("Added Mesh component with gradient pyramid");
    } else {
        LOG_ERROR("MeshSystem not available for pyramid creation");
    }

    // NOTE: Mesh entities should not have Velocity components until we implement mesh physics
    // auto* pyramidVelocity = pyramidEntity->AddComponent<Velocity>();
    // pyramidVelocity->SetVelocity({0.0f, 0.0f, 0.0f}); // No movement for now
    // LOG_INFO("Added Velocity component (stationary for now)");

    // Add collision component for collision testing
    auto* pyramidCollidable = pyramidEntity->AddComponent<Collidable>(Vector3{2.0f, 3.0f, 2.0f}); // Pyramid bounding box
    pyramidCollidable->SetCollisionLayer(LAYER_DEBRIS);
    pyramidCollidable->SetCollisionMask(LAYER_WORLD | LAYER_PLAYER | LAYER_DEBRIS);
    LOG_INFO("Added Collidable component for pyramid collision testing");

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering pyramid entity with systems (after components added)");
    engine_.UpdateEntityRegistration(pyramidEntity);

    // Add to dynamic entities list
    dynamicEntities_.push_back(pyramidEntity);
    LOG_INFO("Rotating pyramid entity added (rendered by RenderSystem)");
}


// REMOVED: Old BSP building methods - now handled by WorldGeometry::BuildWorldGeometry()

// REMOVED: Old BSP building methods - now handled by WorldGeometry::BuildWorldGeometry()
