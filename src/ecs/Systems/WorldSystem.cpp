#include "WorldSystem.h"
#include "../../rendering/Skybox.h"
#include "../Entity.h"
#include "../Components/Position.h"
#include "../Components/Sprite.h"
#include "../Components/Collidable.h"
#include "../Components/Velocity.h"
#include "../../core/Engine.h"
#include "../../rendering/TextureManager.h"
#include "../../utils/Logger.h"

WorldSystem::WorldSystem()
    : worldGeometry_(std::make_unique<WorldGeometry>())
    , worldRenderer_(std::make_unique<WorldRenderer>())
    , mapLoaded_(false)
{
    LOG_INFO("WorldSystem constructor called");
}

void WorldSystem::Initialize() {
    LOG_INFO("WorldSystem initialize called");
    LOG_INFO("WorldSystem initialized (stage vs actors architecture)");

    // Initialize the WorldRenderer with the WorldGeometry
    worldRenderer_->SetWorldGeometry(worldGeometry_.get());

    LoadDefaultMap();
}

bool WorldSystem::LoadDefaultMap() {
    LOG_INFO("Loading default test map");

    // Unload current map
    UnloadMap();

    // Try to load the test map file first
    std::string testMapPath = "assets/maps/test_level.map";
    MapData mapData = mapLoader_.LoadMap(testMapPath);

    if (!mapData.surfaces.empty()) {
        // Process the loaded map data through the new pipeline
        ProcessMapData(mapData);
        mapLoaded_ = true;
        LOG_INFO("Test map loaded successfully from file");
        return true;
    }

    // If file loading fails, create test map data directly
    LOG_WARNING("Test map file not found; creating test map data");
    MapData testMapData = CreateTestMap();
    ProcessMapData(testMapData);

    mapLoaded_ = true;
    LOG_INFO("Test map created and processed successfully");
    return true;
}

void WorldSystem::Shutdown() {
    UnloadMap();
    LOG_INFO("WorldSystem shutdown complete");
}

void WorldSystem::Update(float deltaTime) {
    // World system doesn't need frequent updates
    // Could be used for dynamic world elements in the future
}

void WorldSystem::Render() {
    // World rendering is handled by the RenderSystem
    // This could be used for debug visualization
}

bool WorldSystem::LoadMap(const std::string& mapPath) {
    LOG_INFO("Loading map: " + mapPath);

    // Unload current map
    UnloadMap();

    // Load and parse map file into raw MapData
    MapData mapData = mapLoader_.LoadMap(mapPath);

    if (mapData.surfaces.empty()) {
        LOG_ERROR("Failed to load map: " + mapPath + " - No surfaces found");
        return false;
    }

    // Process the raw MapData through the new building pipeline
    ProcessMapData(mapData);
    mapLoaded_ = true;

    LOG_INFO("Map loaded successfully from: " + mapPath +
             " (Surfaces: " + std::to_string(mapData.surfaces.size()) +
             ", Dynamic Entities: " + std::to_string(dynamicEntities_.size()) + ")");
    return true;
}


void WorldSystem::UnloadMap() {
    if (mapLoaded_) {
        DestroyDynamicEntities();
        worldGeometry_->Clear();
        mapLoaded_ = false;
        LOG_INFO("Map unloaded - WorldGeometry and dynamic entities cleared");
    }
}

// NEW ARCHITECTURE: Main processing pipeline
void WorldSystem::ProcessMapData(const MapData& mapData) {
    LOG_INFO("Processing MapData through NEW pipeline...");

    // Step 1: Build the WorldGeometry (static world data)
    BuildWorldGeometry(mapData);

    // Step 2: Create render batches and materials
    CreateRenderBatches(mapData);
    LoadTexturesAndMaterials(mapData);

    // Step 3: Setup skybox
    SetupSkybox(mapData);

    // Step 4: Create dynamic entities (players, lights, doors, etc.)
    CreateDynamicEntitiesFromMap(mapData);

    LOG_INFO("MapData processing complete");
}

// NEW: Build WorldGeometry from MapData
void WorldSystem::BuildWorldGeometry(const MapData& mapData) {
    LOG_INFO("Building WorldGeometry from MapData");

    // Clear any existing data
    worldGeometry_->Clear();

    // Set basic level info
    worldGeometry_->SetLevelName(mapData.name);
    worldGeometry_->SetSkyColor(mapData.skyColor);

    // Create and build BSP tree
    if (!mapData.surfaces.empty()) {
        worldGeometry_->bspTree = std::make_unique<BSPTree>();
        worldGeometry_->bspTree->Build(mapData.surfaces);
        // Calculate bounds automatically when BSP is built

        LOG_INFO("WorldGeometry built with " + std::to_string(mapData.surfaces.size()) + " surfaces");
    } else {
        LOG_WARNING("No surfaces in MapData; WorldGeometry will be empty");
    }
}

// NEW: Create render batches (placeholder for now)
void WorldSystem::CreateRenderBatches(const MapData& mapData) {
    LOG_INFO("Creating render batches");

    // For now, we'll create simple meshes from surfaces
    // In a full implementation, this would group surfaces by material and create optimized batches
    worldGeometry_->staticMeshes.clear();

    // Group surfaces by material/texture for batching
    std::unordered_map<int, std::vector<Surface>> surfacesByMaterial;

    for (const auto& surface : mapData.surfaces) {
        surfacesByMaterial[surface.textureIndex].push_back(surface);
    }

    LOG_INFO("Grouped surfaces into " + std::to_string(surfacesByMaterial.size()) + " material batches");

    // TODO: Convert surface groups to actual Mesh objects for GPU rendering
    // For now, we just store the surfaces in the BSP tree
}

// NEW: Load textures and create materials
void WorldSystem::LoadTexturesAndMaterials(const MapData& mapData) {
    LOG_INFO("Loading textures and creating materials");

    worldGeometry_->materials.clear();

    // Load textures and create materials
    for (const auto& textureInfo : mapData.textures) {
        WorldMaterial material;

        // Load via TextureManager (executable-relative)
        std::string texturePath = "assets/textures/" + textureInfo.name;
        Texture2D texture = TextureManager::Get().Load(texturePath);

        if (texture.id != 0) {
            material.texture = texture;
            material.hasTexture = true;
            LOG_INFO("Loaded texture: " + textureInfo.name);
        } else {
            material.hasTexture = false;
            material.diffuseColor = GRAY; // Fallback color
            LOG_WARNING("Failed to load texture: " + texturePath + ", using fallback color");
        }

        worldGeometry_->materials[textureInfo.index] = material;
    }

    LOG_INFO("Created " + std::to_string(worldGeometry_->materials.size()) + " materials");
}

// NEW: Setup skybox with robust loading and fallbacks
void WorldSystem::SetupSkybox(const MapData& mapData) {
    LOG_INFO("Skybox: setting up with cubemap support");

    // Try to load skybox using the new cubemap system
    std::string skyboxPath = "assets/textures/skyboxcubemaps/cubemap_cloudy&blue.png";
    if (worldGeometry_->skybox->LoadFromFile(skyboxPath)) {
        LOG_INFO("Skybox: loaded cubemap from: " + skyboxPath);
        return;
    }
    LOG_WARNING("Skybox: failed to load primary cubemap; trying fallbacks");
    std::vector<std::string> fallbackPaths = {
        "assets/textures/skybox.png",
        "assets/textures/cubemap.png",
        "assets/skybox/cloudy.png"
    };
    for (const auto& path : fallbackPaths) {
        if (worldGeometry_->skybox->LoadFromFile(path)) {
            LOG_INFO("Skybox: loaded fallback cubemap from: " + path);
            return;
        }
    }
    // If all file loading fails, create a test skybox
    LOG_WARNING("Skybox: all cubemap files failed; creating test skybox");
    if (worldGeometry_->skybox->LoadTestSkybox()) {
        LOG_INFO("Skybox: test cubemap created");
    } else {
        LOG_ERROR("Skybox: failed to create test skybox");
    }
}

// NEW: Create dynamic entities only
void WorldSystem::CreateDynamicEntitiesFromMap(const MapData& mapData) {
    LOG_INFO("Creating dynamic entities from MapData");

    // Clear existing dynamic entities
    DestroyDynamicEntities();

    // For now, create a test dynamic entity at spawn point
    // In a full implementation, this would parse entity definitions from the map
    Vector3 spawnPoint = {0.0f, 2.0f, 0.0f};
    AddTestDynamicEntity();

    LOG_INFO("Created " + std::to_string(dynamicEntities_.size()) + " dynamic entities");
}

// RENAMED: Destroy dynamic entities only
void WorldSystem::DestroyDynamicEntities() {
    for (Entity* entity : dynamicEntities_) {
        if (entity) {
            GetEngine()->DestroyEntity(entity);
        }
    }
    dynamicEntities_.clear();
    LOG_INFO("Dynamic entities destroyed");
}


// DELEGATED: Point containment check
bool WorldSystem::ContainsPoint(const Vector3& point) const {
    return worldGeometry_ ? worldGeometry_->ContainsPoint(point) : false;
}

// DELEGATED: Ray casting
float WorldSystem::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    return worldGeometry_ ? worldGeometry_->CastRay(origin, direction, maxDistance) : maxDistance;
}

bool WorldSystem::FindSpawnPoint(Vector3& spawnPoint) const {
    // For now, use a default spawn point
    // In a full implementation, this would read spawn points from the map
    spawnPoint = {0.0f, 2.0f, 0.0f};
    return true;
}

MapData WorldSystem::CreateTestMap() {
    LOG_INFO("Creating test map data");

    // Create map data structure
    MapData mapData;
    mapData.name = "Test Map - Stage vs Actors";
    mapData.skyColor = SKYBLUE;
    mapData.floorHeight = 0.0f;
    mapData.ceilingHeight = 8.0f;

    // Add basic textures (without loading them - just metadata)
    mapData.textures.emplace_back("devtextures/Dark/proto_wall_dark.png", 0);
    mapData.textures.emplace_back("devtextures/Light/proto_1024_light.png", 1);
    mapData.textures.emplace_back("devtextures/Green/proto_1024_green.png", 2);
    mapData.textures.emplace_back("skyboxcubemaps/cubemap_cloudy&blue.png", 3);

    // Create geometry and add to map data
    AddTestGeometry(mapData);

    LOG_INFO("Test map data created with " + std::to_string(mapData.surfaces.size()) + " surfaces");

    // Debug: List all surfaces
    for (size_t i = 0; i < mapData.surfaces.size(); ++i) {
        LOG_INFO("Surface " + std::to_string(i) + ": (" +
                 std::to_string(mapData.surfaces[i].start.x) + ", " +
                 std::to_string(mapData.surfaces[i].start.y) + ", " +
                 std::to_string(mapData.surfaces[i].start.z) + ") to (" +
                 std::to_string(mapData.surfaces[i].end.x) + ", " +
                 std::to_string(mapData.surfaces[i].end.y) + ", " +
                 std::to_string(mapData.surfaces[i].end.z) + ")");
    }
    return mapData;
}

// Add geometry for the test map
void WorldSystem::AddTestGeometry(MapData& mapData) {
    LOG_INFO("Adding geometry to test map");

    // Create a simple room: 20x20 units with walls
    float roomSize = 20.0f;
    float wallHeight = 8.0f;

    // North wall (back)
    mapData.surfaces.emplace_back(
        Vector3{-roomSize/2, 0.0f, -roomSize/2},
        Vector3{roomSize/2, wallHeight, -roomSize/2},
        wallHeight, GRAY
    );

    // South wall (front)
    mapData.surfaces.emplace_back(
        Vector3{-roomSize/2, 0.0f, roomSize/2},
        Vector3{roomSize/2, wallHeight, roomSize/2},
        wallHeight, GRAY
    );

    // East wall (right)
    mapData.surfaces.emplace_back(
        Vector3{roomSize/2, 0.0f, -roomSize/2},
        Vector3{roomSize/2, wallHeight, roomSize/2},
        wallHeight, GRAY
    );

    // West wall (left)
    mapData.surfaces.emplace_back(
        Vector3{-roomSize/2, 0.0f, -roomSize/2},
        Vector3{-roomSize/2, wallHeight, roomSize/2},
        wallHeight, GRAY
    );

    // Add a pillar in the middle for testing
    float pillarSize = 2.0f;
    mapData.surfaces.emplace_back(
        Vector3{-pillarSize/2, 0.0f, -pillarSize/2},
        Vector3{pillarSize/2, wallHeight, -pillarSize/2},
        wallHeight, DARKGRAY
    );
    mapData.surfaces.emplace_back(
        Vector3{pillarSize/2, 0.0f, -pillarSize/2},
        Vector3{pillarSize/2, wallHeight, pillarSize/2},
        wallHeight, DARKGRAY
    );
    mapData.surfaces.emplace_back(
        Vector3{pillarSize/2, 0.0f, pillarSize/2},
        Vector3{-pillarSize/2, wallHeight, pillarSize/2},
        wallHeight, DARKGRAY
    );
    mapData.surfaces.emplace_back(
        Vector3{-pillarSize/2, 0.0f, pillarSize/2},
        Vector3{-pillarSize/2, wallHeight, -pillarSize/2},
        wallHeight, DARKGRAY
    );
}

void WorldSystem::CreateRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating first room geometry");

    // Room dimensions
    const float roomSize = 10.0f;
    const float wallHeight = 8.0f;
    const float floorHeight = 0.0f;
    const float ceilingHeight = 8.0f;

    // Create walls
    std::vector<Surface> roomSurfaces = {
        // North wall
        Surface{{ -roomSize/2, floorHeight, -roomSize/2},
                {-roomSize/2, wallHeight, -roomSize/2}, wallHeight, GRAY},

        // South wall
        Surface{{-roomSize/2, floorHeight, roomSize/2},
                {-roomSize/2, wallHeight, roomSize/2}, wallHeight, GRAY},

        // East wall
        Surface{{roomSize/2, floorHeight, -roomSize/2},
                {roomSize/2, wallHeight, -roomSize/2}, wallHeight, GRAY},

        // West wall (with door opening)
        Surface{{-roomSize/2, floorHeight, -roomSize/2},
                {roomSize/2, wallHeight, -roomSize/2}, wallHeight, GRAY},

        // Floor
        Surface{{-roomSize/2, floorHeight, -roomSize/2},
                {roomSize/2, floorHeight, roomSize/2}, 0.0f, DARKGRAY},

        // Ceiling
        Surface{{-roomSize/2, ceilingHeight, -roomSize/2},
                {roomSize/2, ceilingHeight, roomSize/2}, 0.0f, LIGHTGRAY}
    };

    // Add surfaces to map data
    mapData.surfaces.insert(mapData.surfaces.end(), roomSurfaces.begin(), roomSurfaces.end());
}

void WorldSystem::CreateCorridorGeometry(MapData& mapData) {
    LOG_INFO("Creating corridor geometry");

    const float corridorWidth = 4.0f;
    const float corridorLength = 8.0f;
    const float wallHeight = 8.0f;
    const float floorHeight = 0.0f;

    // Corridor starts at x=10 (end of first room) and goes to x=18
    std::vector<Surface> corridorSurfaces = {
        // Left wall
        Surface{{10.0f, floorHeight, -corridorWidth/2},
                {10.0f + corridorLength, wallHeight, -corridorWidth/2}, wallHeight, GRAY},

        // Right wall
        Surface{{10.0f, floorHeight, corridorWidth/2},
                {10.0f + corridorLength, wallHeight, corridorWidth/2}, wallHeight, GRAY},

        // Floor
        Surface{{10.0f, floorHeight, -corridorWidth/2},
                {10.0f + corridorLength, floorHeight, corridorWidth/2}, 0.0f, DARKGRAY},

        // Ceiling
        Surface{{10.0f, wallHeight, -corridorWidth/2},
                {10.0f + corridorLength, wallHeight, corridorWidth/2}, 0.0f, LIGHTGRAY}
    };

    // Add surfaces to map data
    mapData.surfaces.insert(mapData.surfaces.end(), corridorSurfaces.begin(), corridorSurfaces.end());
}

void WorldSystem::CreateSecondRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating second room geometry");

    // Second room starts at x=18
    const float roomSize = 12.0f;
    const float wallHeight = 8.0f;
    const float floorHeight = 0.0f;

    // Create walls (no ceiling for skybox visibility)
    std::vector<Surface> roomSurfaces = {
        // North wall
        Surface{{18.0f, floorHeight, -roomSize/2},
                {18.0f, wallHeight, -roomSize/2}, wallHeight, GRAY},

        // South wall
        Surface{{18.0f, floorHeight, roomSize/2},
                {18.0f, wallHeight, roomSize/2}, wallHeight, GRAY},

        // East wall
        Surface{{18.0f + roomSize, floorHeight, -roomSize/2},
                {18.0f + roomSize, wallHeight, -roomSize/2}, wallHeight, GRAY},

        // West wall (corridor entrance)
        Surface{{18.0f + roomSize, floorHeight, roomSize/2},
                {18.0f + roomSize, wallHeight, roomSize/2}, wallHeight, GRAY},

        // Floor
        Surface{{18.0f, floorHeight, -roomSize/2},
                {18.0f + roomSize, floorHeight, roomSize/2}, 0.0f, DARKGRAY}

        // No ceiling - open to sky
    };

    // Add surfaces to map data
    mapData.surfaces.insert(mapData.surfaces.end(), roomSurfaces.begin(), roomSurfaces.end());
}

// REMOVED: CreateSurfaceEntity - ARCHITECTURAL FIX
// Static surfaces are no longer converted to entities!
// They are stored in WorldGeometry and rendered by WorldRenderer.

// NEW: Add a test dynamic entity to verify the new architecture works
void WorldSystem::AddTestDynamicEntity() {
    LOG_INFO("Adding test dynamic entity (stage vs actors architecture)");

    Entity* testEntity = GetEngine()->CreateEntity();
    LOG_INFO("Created test dynamic entity with ID: " + std::to_string(testEntity->GetId()));

    // Add position component at spawn point
    testEntity->AddComponent<Position>(0.0f, 2.0f, 0.0f);
    LOG_INFO("Added Position component to test entity at (0, 2, 0)");

    // Add sprite component with bright color to make it visible
    auto sprite = testEntity->AddComponent<Sprite>();
    Color testColor = {255, 255, 0, 255}; // YELLOW color - clearly visible
    sprite->SetTint(testColor);
    LOG_INFO("Added Sprite component to test entity (yellow tint)");

    // Add velocity component to make it move (dynamic behavior)
    auto* velocity = testEntity->AddComponent<Velocity>();
    velocity->SetVelocity({0.5f, 0.0f, 0.0f}); // Move slowly right
    LOG_INFO("Added Velocity component (entity will move)");

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering test dynamic entity with systems (after components added)");
    GetEngine()->UpdateEntityRegistration(testEntity);

    // Add to dynamic entities list (NOT world entities - that's the architectural fix!)
    dynamicEntities_.push_back(testEntity);
    LOG_INFO("Test dynamic entity added (rendered by RenderSystem)");
}

Entity* WorldSystem::CreateLightEntity(const Vector3& position, const Color& color, float intensity) {
    Entity* entity = GetEngine()->CreateEntity();

    entity->AddComponent<Position>(position);

    // Add light component (would be implemented in lighting system)
    // auto light = entity->AddComponent<Light>();
    // light->SetColor(color);
    // light->SetIntensity(intensity);

    dynamicEntities_.push_back(entity);
    return entity;
}

// REMOVED: Old BSP building methods - now handled by WorldGeometry::BuildWorldGeometry()
